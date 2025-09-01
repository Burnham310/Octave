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
scope: Scope = .{ .parent = null },
active_scope: *Scope = undefined,

anno_extra_index: u32 = 0,

expr_extras: std.ArrayListUnmanaged(ExprAnnotation) = .{},

a: std.mem.Allocator,

pub const Map = std.AutoHashMapUnmanaged(Symbol, *Ast.Formal);

pub const Scope = struct {
    pub const Entry = Map.Entry;
    pub const KV = Map.KV;
    defs: Map = .empty,
    parent: ?*Scope,

    pub fn get(self: Scope, sym: Symbol) ?Entry {
        return self.defs.getEntry(sym) orelse if (self.parent) |parent| parent.get(sym) else null;
    }

    pub fn fetch_put(self: *Scope, a: std.mem.Allocator, sym: Symbol, formal: *Ast.Formal) ?Entry {
        if (self.get(sym)) |entry| return entry;
        self.defs.putNoClobber(a, sym, formal) catch unreachable;
        return null;
    }
};

pub const ExprAnnotation = struct {
    ty: Type,
};

pub const Annotations = struct {
    main_formal: *Ast.Formal,
    builtin_vars: std.AutoHashMapUnmanaged(Symbol, struct {Type, u32}),
    config_tys: std.AutoHashMapUnmanaged(Symbol, Type),
    expr_extras: []ExprAnnotation,

    pub fn deinit(self: *Annotations, a: std.mem.Allocator) void {
       self.builtin_vars.deinit(a); 
    }
};

pub const Error = error {
    TypeMismatched,
    Undefined,
    Redefined,
    UnknownConfig,
    InsufficientInference,
};

fn new_anno(self: *Sema, anno: ExprAnnotation) u32 {
    self.expr_extras.append(anno) catch unreachable; 
    return self.expr_extras.items.len - 1;
}

const TypeDesc = union(enum) {
    concrete: Type,
    iterable: Type,
    either: struct {*TypeDesc, *TypeDesc},
    none,

    pub fn validate(self: TypeDesc, t: Type) ?Type {
        switch (self) {
            .concrete => |concrete_t| return conform_to(t, concrete_t),
            .iterable => |iter_inner| {
                if (conform_to(t, iter_inner)) |to| return to;
                const full = TypePool.lookup(t);
                switch (full) {
                    .@"for" => |inner| return if (conform_to(inner, iter_inner)) |_| TypePool.intern(.{.@"for" = iter_inner}) else null,
                    else => return null,
                }
            },
            .either => |either| {
                return either.@"0".validate(t) orelse either.@"1".validate(t);
            },
            .none =>
                return t,
        }
    }

    pub fn format(value: TypeDesc, writer: *std.io.Writer) !void {
        switch (value) {
            .concrete => |concrete_t| return writer.print("{f}", .{TypePool.lookup(concrete_t)}),
            .iterable => |iter_inner| return writer.print("{[t]f} | for<{[t]f}>", .{.t = TypePool.lookup(iter_inner)}),
            .either => |either| return writer.print("{f} | {f}", .{either.@"1".*, either.@"0".*}),
            .none => return writer.print("anytype", .{}),
        }
    }

    pub fn conform_to(from: Type, to: Type) ?Type {
        if (from == to) return to;
        if (from == Type.pitch and to == Type.note) return to;
        if (from == Type.int and (to == Type.pitch or to == Type.note)) return to;
        return null;
    }

    pub fn get_list_el(self: TypeDesc) !TypeDesc {
        switch (self) {
            .none => return .none,
            .iterable,
            .concrete => |concrete| {
                const full = TypePool.lookup(concrete);
                switch (full) {
                    .list => |el| return TypeDesc { .iterable = el },
                    else => {
                        return Error.InsufficientInference;
                    }
                }
            },
            .either => |either| {
                return either.@"0".get_list_el() catch either.@"1".get_list_el();
            },
        }
    }
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
    self.config_tys.putNoClobber(a, self.lexer.intern("instrument"), Type.int) catch unreachable;
    self.config_tys.putNoClobber(a, self.lexer.intern("volume"), Type.int) catch unreachable;

    self.active_scope = &self.scope;
    // typecheck each toplevel declaration
    for (self.ast.toplevels) |formal| {
        try self.sema_formal(formal);
        if (formal.ident == Lexer.BuiltinSymbols.main)
            self.main_formal = formal;
    }
    if (self.main_formal == null) {
        self.lexer.report_err(0, "main is undefiend", .{});
        return Error.Undefined;
    }
    var desc_sec = TypeDesc {.concrete = Type.section};
    var desc_secs = TypeDesc {.concrete = TypePool.intern(.{.list = Type.section})};
    const either = TypeDesc {.either = .{&desc_sec, &desc_secs }};
    _ = try self.sema_expr(self.main_formal.?.expr, either);
    return Annotations {
        .main_formal = self.main_formal.?,
        .builtin_vars = self.builtin_vars,
        .config_tys = self.config_tys,
        // .sec_envs = self.sec_envs,
        .expr_extras = self.expr_extras.toOwnedSlice(self.a) catch unreachable,
    };

}

pub fn sema_formal(self: *Sema, formal: *Ast.Formal) Error!void {
    if (self.active_scope.fetch_put(self.a, formal.ident, formal)) |shadowed| {
        self.lexer.report_err(formal.first_off(), "variable `{s}` is alread defined", .{self.lexer.lookup(formal.ident)});
        self.lexer.report_line(formal.first_off());
        self.lexer.report_note(shadowed.value_ptr.*.first_off(), "previously defined here", .{});
        self.lexer.report_line(shadowed.value_ptr.*.first_off());
        return Error.Redefined;
    }
    
}

pub fn sema_config(self: *Sema, formal: *Ast.Formal) Error!void {
    const expected_ty = self.config_tys.get(formal.ident) orelse {
        self.lexer.report_err(formal.first_off(), "unrecognized config name `{s}`", .{self.lexer.lookup(formal.ident)});   
        self.lexer.report_line(formal.first_off());
        return Error.UnknownConfig;
    };
    _ = try self.sema_expr(formal.expr, .{.concrete = expected_ty });
}

pub fn sema_expr(self: *Sema, expr: *Ast.Expr, infer: TypeDesc) Error!Type {
    const from = try self.sema_expr_impl(expr, infer);
    const to = try self.expect_type(infer, from, expr.off);
    const anno = ExprAnnotation {.ty = to};
    expr.anno_extra = self.anno_extra_index;
    self.anno_extra_index += 1;
    self.expr_extras.append(self.a, anno) catch unreachable;
    return anno.ty;
}

fn expect_type(self: *Sema, expected: TypeDesc, got: Type, off: u32) !Type {
    if (expected.validate(got)) |to| {
        return to;
    }
    self.lexer.report_err(off, "Expect Type `{f}`, got `{f}`", .{expected, got}); 
    self.lexer.report_line(off);
    return Error.TypeMismatched;
}

// Typecheck the expr, and return the type of expression.
// Every expr in Octave has a type. Some expression, such as number literal, can have multiple type
// For this purpose, sema_expr has a `infer` parameter, which tells expression what type the parent expression is expecting.
// When the child expression cannot be somehow the inferred type, an error is returned.
//
// The infer parameter can either be, a concrete type, or a description of type.
// Currently, the only description is `iterable`. 
// This function does not check whether `infer` is valid with the resulted type. Instead, it is checked at sema_expr.
fn sema_expr_impl(self: *Sema, expr: *Ast.Expr, infer: TypeDesc) !Type {
    switch (expr.data) {
        .num => {
            return Type.int; 
            // self.lexer.report_err(expr.off, "type of number literal cannot be inferred", .{});
            // self.lexer.report_line(expr.off);
            // return Error.InsufficientInference;
        },
        .ident => |*ident| {
            if (self.active_scope.get(ident.sym)) |entry| {
                ident.sema_expr = entry.value_ptr.*.expr;
                return self.sema_expr(entry.value_ptr.*.expr, infer);
            }
            if (self.builtin_vars.get(ident.sym)) |builtin|
                return builtin[0];
            self.lexer.report_err(expr.off, "undefined variable `{s}`", .{self.lexer.lookup(ident.sym)});
            self.lexer.report_line(expr.off);
            return Error.Undefined;
        },
        .sec => |sec| {
            var map = Scope { .parent = self.active_scope };
            self.active_scope = &map;
            defer self.active_scope = map.parent.?;
            for (sec.variable) |v| {
                try self.sema_formal(v);
            }
            for (sec.config) |config| {
                try self.sema_config(config);
            }
            var desc_note = TypeDesc {.iterable = Type.note};
            var desc_notes = TypeDesc {.iterable = TypePool.intern(.{.list = Type.pitch})};
            const desc_either = TypeDesc {.either = .{&desc_note, &desc_notes}};
            // const allowed_tys = [_]Type {Type.note, Type.pitch, Type.void, TypePool.intern(.{.list = Type.pitch})};
            for (sec.notes) |note| {
                const ty = try self.sema_expr(note, desc_either);
                _ = ty;
                // std.log.debug("sec {f}", .{ty});
                
                // for (allowed_tys) |allowed| {
                //     if (allowed == note_ty) break;
                // } else {
                //     self.lexer.report_err(note.off, "expect one of Type note, chord, or num, got {f}", .{TypePool.lookup(note_ty)});
                //     self.lexer.report_line(note.off);
                //     return Error.TypeMismatched;
                // }
            }
            return Type.section;
        },
        .prefix => |prefix| {
            var desc_pitch = TypeDesc {.concrete = Type.pitch};
            var desc_chord = TypeDesc {.concrete = TypePool.intern(.{.list = Type.note})};
            const either = TypeDesc {.either = .{&desc_pitch, &desc_chord }};

            _ = try self.sema_expr(prefix.rhs, either);
            return Type.pitch;
        },
        .infix => |infix| {
            switch (infix.op) {
                .single_quote => {
                    var desc_pitch = TypeDesc {.concrete = Type.pitch};
                    var desc_chord = TypeDesc {.concrete = Type.chord};
                    const either = TypeDesc {.either = .{&desc_pitch, &desc_chord }};
                    _ = try self.sema_expr(infix.lhs, either);
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.fraction});
                    return Type.note; 
                },
                .slash => {
                    _ = try self.sema_expr(infix.lhs, .{ .concrete = Type.int }); // TODO: degree?
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.int }); // TODO: degree?
                    return Type.fraction; 

                },
                .plus, .minus => {
                    _ = try self.sema_expr(infix.lhs, .{ .concrete = Type.int }); // TODO: degree?
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.int }); // TODO: degree?
                    return Type.int;
                },
                else => unreachable,
            }
        },
        .list => |list| {
            const el_infer = infer.get_list_el() catch |e| {
                self.lexer.report_err(expr.first_off(), "inference {f} cannot be matched onto list", .{infer});
                self.lexer.report_line(expr.first_off());
                return e;
            };
            if (list.els.len == 0) {
                return TypePool.intern(.{ .list = el_infer.iterable });
                // self.lexer.report_err(expr.off, "type of an empty list cannot be inferred", .{}); 
                // return Error.InsufficientInference;
                // return Type.pitch;
            }


            const el_ty = (try self.sema_expr(list.els[0], el_infer)).take_for_inner(); 
            for (list.els[1..], 2..) |el, i| {
                const other_ty = (try self.sema_expr(el, el_infer)).take_for_inner();
                if (el_ty != other_ty) {
                    self.lexer.report_err(el.off, "exprssions in list must have the same type; 1th is {f}, {}th is {f}", 
                        .{TypePool.lookup(el_ty), i, TypePool.lookup(other_ty)});
                    self.lexer.report_line(el.off);
                    self.lexer.report_note(list.els[0].off, "first expression is here", .{});
                    self.lexer.report_line(list.els[0].off);
                    return Error.TypeMismatched;
                }
            }
            return TypePool.intern(.{.list = el_ty});
        },
        .@"for" => |@"for"| {

            if (@"for".with) |with| {
                var map = Scope { .parent = self.active_scope };
                self.active_scope = &map;
                if (map.fetch_put(self.a, with.ident, with)) |shadowed| {
                    self.lexer.report_err(with.first_off(), "variable `{s}` is alread defined", .{self.lexer.lookup(with.ident)});
                    self.lexer.report_line(with.first_off());
                    self.lexer.report_note(shadowed.value_ptr.*.first_off(), "previously defined here", .{});
                    self.lexer.report_line(shadowed.value_ptr.*.first_off());
                    return Error.Redefined;

                }
            }
            defer {
                if (@"for".with) |_| self.active_scope = self.active_scope.parent.?;
            }

            _ = try self.sema_expr(@"for".lhs, .{ .concrete = Type.int });
            _ = try self.sema_expr(@"for".rhs, .{ .concrete = Type.int });
            if (@"for".body.len == 0) {
                @panic("TODO");
                // if (infer) |infer_inner|
                //     return TypePool.intern(.{.list = infer_inner});
                // self.lexer.report_err(expr.off, "body of a for loop cannot be empty", .{}); 
                // return Error.InsufficientInference;
            }
            const el_ty = (try self.sema_expr(@"for".body[0], infer)).take_for_inner();
             
            for (@"for".body[1..], 2..) |el, i| {
                const other_ty = (try self.sema_expr(el, infer)).take_for_inner();
                if (el_ty != other_ty) {
                    self.lexer.report_err(el.off, "exprssions in for loop must have the same type; 1th is {f}, {}th is {f}", 
                        .{TypePool.lookup(el_ty), i, TypePool.lookup(other_ty)});
                    self.lexer.report_line(el.off);
                    self.lexer.report_note(@"for".body[0].off, "first expression is here", .{});
                    self.lexer.report_line(@"for".body[0].off);
                    return Error.TypeMismatched;
                }
            }
            return TypePool.intern(.{.@"for" = el_ty});
        },
    }
}

test "packed struct equality" {
    const S = packed struct {t:u32};
    const s1 = S {.t = 0};
    const s2 = S {.t = 1};
    return std.testing.expect(s1 != s2);
}
