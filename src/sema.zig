const std = @import("std");
const Lexer =  @import("lexer.zig");
const BS = Lexer.BuiltinSymbols;
const Symbol = Lexer.Symbol;
const Ast = @import("ast.zig");
const TypePool = @import("type_pool.zig");
const Type = TypePool.Type;

const Tonality = @import("tonality.zig");

const Sema = @This();

ast: *Ast,
lexer: *Lexer,
main_formal: ?*Ast.Formal = null,
builtin_vars: std.AutoHashMapUnmanaged(Symbol, struct {Type, u32}) = .{},
config_tys: std.AutoHashMapUnmanaged(Symbol, Type) = .{},
sec_envs: std.AutoHashMapUnmanaged(*Ast.Expr.Section, SectionEnv) = .{},
global_env: SectionEnv = .{},
active_sec_env: ?*SectionEnv = null,
a: std.mem.Allocator,

pub const SectionEnv = std.AutoHashMapUnmanaged(Symbol, *Ast.Formal);

pub const Annotations = struct {
    main_formal: *Ast.Formal,
    builtin_vars: std.AutoHashMapUnmanaged(Symbol, struct {Type, u32}),
    config_tys: std.AutoHashMapUnmanaged(Symbol, Type),
    sec_envs: std.AutoHashMapUnmanaged(*Ast.Expr.Section, SectionEnv),

    pub fn deinit(self: *Annotations, a: std.mem.Allocator) void {
       self.builtin_vars.deinit(a); 
    }
};

const Error = error {
    TypeMismatched,
    Undefined,
    Redefined,
    UnknownConfig,
    InsufficientInference,
};

pub fn sema(self: *Sema) Error!Annotations {
    // initialize builtin variables
    const modes = @typeInfo(Tonality.Mode).@"enum".fields;
    const a = self.a;
    inline for (modes) |mode| {
        self.builtin_vars.putNoClobber(a, self.lexer.intern(mode.name), .{Type.mode, mode.value}) catch unreachable;
    }
    self.builtin_vars.putNoClobber(a, self.lexer.intern("major"), .{Type.mode, @intFromEnum(Tonality.Mode.major)}) catch unreachable;
    self.builtin_vars.putNoClobber(a, self.lexer.intern("minor"), .{Type.mode, @intFromEnum(Tonality.Mode.minor)}) catch unreachable;
    const pitches = @typeInfo(Tonality.Pitch).@"enum".fields;
    inline for (pitches) |pitch| {
        self.builtin_vars.putNoClobber(a, self.lexer.intern(pitch.name), .{Type.pitch, pitch.value}) catch unreachable;
    }
    // initialize config types
    self.config_tys.putNoClobber(a, self.lexer.intern("octave"), Type.int) catch unreachable;
    self.config_tys.putNoClobber(a, self.lexer.intern("mode"), Type.mode) catch unreachable;
    self.config_tys.putNoClobber(a, self.lexer.intern("tonic"), Type.pitch) catch unreachable;
    self.config_tys.putNoClobber(a, self.lexer.intern("bpm"), Type.int) catch unreachable;
    self.config_tys.putNoClobber(a, self.lexer.intern("tempo"), Type.fraction) catch unreachable;
    // typecheck each toplevel declaration
    for (self.ast.toplevels) |formal| {
        try self.sema_formal(formal);
    }
    if (self.main_formal == null) {
        self.lexer.report_err(0, "main is undefiend", .{});
        return Error.Undefined;
    }
    try self.expect_type_mappable(Type.section, try self.sema_expr(self.main_formal.?.expr, null), self.main_formal.?.ident_off);
    return Annotations {
        .main_formal = self.main_formal.?,
        .builtin_vars = self.builtin_vars,
        .config_tys = self.config_tys,
        .sec_envs = self.sec_envs,
    };

}

pub fn sema_formal(self: *Sema, formal: *Ast.Formal) Error!void {
    const env = self.active_sec_env orelse &self.global_env;
    if (env.fetchPut(self.a, formal.ident, formal) catch unreachable) |shadowed| {
        self.lexer.report_err(formal.first_off(), "variable `{s}` is alread defined", .{self.lexer.lookup(formal.ident)});
        self.lexer.report_line(formal.first_off());
        self.lexer.report_note(shadowed.value.first_off(), "previously defined here", .{});
        self.lexer.report_line(shadowed.value.first_off());
        return Error.Redefined;
    }
    if (formal.ident == Lexer.BuiltinSymbols.main)
        self.main_formal = formal;
}

pub fn sema_config(self: *Sema, formal: *Ast.Formal) Error!void {
    const expected_ty = self.config_tys.get(formal.ident) orelse {
        self.lexer.report_err(formal.first_off(), "unrecognized config name `{s}`", .{self.lexer.lookup(formal.ident)});   
        self.lexer.report_line(formal.first_off());
        return Error.UnknownConfig;
    };
    try self.expect_type(expected_ty, try self.sema_expr(formal.expr, expected_ty), formal.ident_off);
}

pub fn sema_expr(self: *Sema, expr: *Ast.Expr, infer: ?Type) Error!Type {
    expr.ty = try self.sema_expr_impl(expr, infer);
    return expr.ty;
}

pub fn expect_type(self: *Sema, expected: Type, got: Type, off: u32) !void {
    if (expected == got) {
        return;
    }
    self.lexer.report_err(off, "Expect Type `{}`, got `{}`", .{TypePool.lookup(expected), TypePool.lookup(got)}); 
    self.lexer.report_line(off);
    return Error.TypeMismatched;
}

pub fn expect_type_mappable(self: *Sema, expected: Type, got: Type, off: u32) !void {
    if (expected == got) {
        return;
    }
    const got_full = TypePool.lookup(got);
    switch (got_full) {
        .list => |el_ty| if (el_ty == expected) return,
        else => {},
    }
    self.lexer.report_err(off, "Expect Type `{}` or `[{}]`, got `{}`", .{TypePool.lookup(expected), TypePool.lookup(expected), TypePool.lookup(got)}); 
    self.lexer.report_line(off);
    return Error.TypeMismatched;
}

pub fn sema_expr_impl(self: *Sema, expr: *Ast.Expr, infer: ?Type) !Type {
    switch (expr.data) {
        .num => {
            if (infer == Type.pitch) return Type.pitch
            else if (infer == Type.int) return Type.int
            else if (infer == Type.note) return Type.pitch // What?? TODO: clean this up
            else {
                self.lexer.report_err(expr.off, "type of number literal cannot be inferred", .{});
                self.lexer.report_line(expr.off);
                return Error.InsufficientInference;
            }
        },
        .ident => |*ident| {
            if (self.active_sec_env) |env| {
                if (env.get(ident.sym)) |formal| {
                    ident.sema_expr = formal.expr;
                    return self.sema_expr(formal.expr, infer);
                }
            }
            if (self.builtin_vars.get(ident.sym)) |builtin|
                return builtin[0];
            if (self.global_env.get(ident.sym)) |formal| {
               return self.sema_expr(formal.expr, infer); 
            }
            self.lexer.report_err(expr.off, "undefined variable `{s}`", .{self.lexer.lookup(ident.sym)});
            self.lexer.report_line(expr.off);
            return Error.Undefined;
        },
        .sec => |sec| {
            const gop = self.sec_envs.getOrPut(self.a, sec) catch unreachable;
            std.debug.assert(!gop.found_existing);
            gop.value_ptr.* = .{};
            const env = gop.value_ptr;
            self.active_sec_env = env;
            for (sec.variable) |v| {
                try self.sema_formal(v);
            }
            for (sec.config) |config| {
                try self.sema_config(config);
            }
            const allowed_tys = [_]Type {Type.note, Type.pitch, Type.void, TypePool.intern(.{.list = Type.pitch})};
            for (sec.notes) |note| {
                const note_ty = try self.sema_expr(note, Type.note);
                for (allowed_tys) |allowed| {
                    if (allowed == note_ty) break;
                } else {
                    self.lexer.report_err(note.off, "expect one of Type note, chord, or num, got {}", .{TypePool.lookup(note_ty)});
                    self.lexer.report_line(note.off);
                    return Error.TypeMismatched;
                }
            }
            return Type.section;
        },
        .prefix => |prefix| {
            try self.expect_type_mappable(Type.pitch, try self.sema_expr(prefix.rhs, Type.pitch), prefix.rhs.off);
            return Type.pitch;
        },
        .infix => |infix| {
            switch (infix.op) {
                .single_quote => {
                    try self.expect_type_mappable(Type.pitch, try self.sema_expr(infix.lhs, Type.pitch), infix.rhs.off);
                    try self.expect_type(Type.fraction, try self.sema_expr(infix.rhs, Type.fraction), infix.rhs.off);
                    return Type.note; 
                },
                .slash => {
                    try self.expect_type(Type.int, try self.sema_expr(infix.lhs, Type.int), infix.lhs.off); // TODO: degree?
                    try self.expect_type(Type.int, try self.sema_expr(infix.rhs, Type.int), infix.rhs.off);
                    return Type.fraction; 

                },
               else => unreachable,
            }
        },
        .list => |list| {
            if (list.els.len == 0) {
                //if (infer) |infer_inner|
                //    return TypePool.intern(.{.list = infer_inner});
                //self.lexer.report_err(expr.off, "type of an empty list cannot be inferred", .{}); 
                //return Error.InsufficientInference;
                return Type.pitch;
            }
            const el_ty = try self.sema_expr(list.els[0], infer); 
            for (list.els[1..], 2..) |el, i| {
                const other_ty = try self.sema_expr(el, infer);
                if (el_ty != other_ty) {
                    self.lexer.report_err(el.off, "exprssions in list must have the same type; 1th is {}, {}th is {}", 
                        .{TypePool.lookup(el_ty), i, TypePool.lookup(other_ty)});
                    self.lexer.report_line(el.off);
                    self.lexer.report_note(list.els[0].off, "first expression is here", .{});
                    self.lexer.report_line(list.els[0].off);
                    return Error.TypeMismatched;
                }
            }
            return TypePool.intern(.{.list = el_ty});
        },
    }
}

test "packed struct equality" {
    const S = packed struct {t:u32};
    const s1 = S {.t = 0};
    const s2 = S {.t = 1};
    return std.testing.expect(s1 != s2);
}
