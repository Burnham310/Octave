const std = @import("std");
const assert = std.debug.assert;
const Lexer =  @import("lexer.zig");
const BS = Lexer.BuiltinSymbols;
const Symbol = Lexer.Symbol;
const Ast = @import("ast.zig");
const TypePool = @import("type_pool.zig");
const InternPool = @import("intern_pool.zig");
const Type = TypePool.Type;

const Tonality = @import("tonality.zig");

const Sema = @This();

ast: *Ast,
lexer: *Lexer,
main_formal: ?*Ast.Formal = null,
builtin_vars: Builtin = .{},
builtin_tys: std.AutoHashMapUnmanaged(Symbol, Type) = .{},
scope: Scope = .{ .parent = null },
active_scope: *Scope = undefined,

anno_extra_index: u32 = 0,

expr_extras: std.ArrayListUnmanaged(ExprAnnotation) = .{},

a: std.mem.Allocator,

pub const TypedValue = struct { Type, u32 };

pub const VarLoc = union(enum) {
    decl: *Ast.Formal,
    builtin: TypedValue,
};
pub const Map = std.AutoHashMapUnmanaged(Symbol, VarLoc);
pub const Builtin = std.AutoHashMapUnmanaged(Symbol, TypedValue);

pub const Scope = struct {
    pub const Entry = Map.Entry;
    pub const KV = Map.KV;
    defs: Map = .empty,
    parent: ?*Scope,

    pub fn get(self: Scope, sym: Symbol) ?Entry {
        return self.defs.getEntry(sym) orelse if (self.parent) |parent| parent.get(sym) else null;
    }

    pub fn fetch_put(self: *Scope, a: std.mem.Allocator, sym: Symbol, loc: VarLoc) ?Entry {
        if (self.get(sym)) |entry| return entry;
        self.defs.putNoClobber(a, sym, loc) catch unreachable;
        return null;
    }

    pub fn fetch_put_else_report(
        self: *Scope, lexer: *Lexer, 
        a: std.mem.Allocator, 
        sym: Symbol, loc: VarLoc, 
        off: u32) !void {
        if (self.fetch_put(a, sym, loc)) |shadowed| {
            lexer.report_err_line(off, "variable `{s}` is alread defined", .{lexer.lookup(sym)});
            if (shadowed.value_ptr.* == .decl) 
                lexer.report_note_line(shadowed.value_ptr.*.decl.first_off(), "previously defined here", .{});
            return Error.Redefined;
        }
    }

    pub fn fetch_put_builtin_assert(self: *Scope, a: std.mem.Allocator, sym: Symbol, builtin: TypedValue) void {
        assert(self.fetch_put(a, sym, .{ .builtin = builtin }) == null);
    }
};

pub const ExprAnnotation = struct {
    ty: Type,
    data: Data,

    pub const Data = union {
        config_tys: std.AutoHashMapUnmanaged(Symbol, Type),
        formal_ref: VarLoc,
    };
};

pub const Annotations = struct {
    main_formal: *Ast.Formal,
    builtin_vars: std.AutoHashMapUnmanaged(Symbol, struct {Type, u32}),
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
    either: struct {*const TypeDesc, *const TypeDesc},
    none,

    pub fn validate(self: TypeDesc, t: Type) ?Type {
        switch (self) {
            .concrete => |concrete_t| return conform_to(t, concrete_t),
            .iterable => |iter_inner| {
                if (conform_to(t, iter_inner)) |to| return to;
                const full = TypePool.lookup(t);
                switch (full) {
                    .splat => |inner| return if (conform_to(inner, iter_inner)) |_| TypePool.intern(.{.splat = iter_inner}) else null,
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

    pub fn format(value: TypeDesc, writer: *std.Io.Writer) !void {
        switch (value) {
            .concrete => |concrete_t| return writer.print("{f}", .{ TypePool.lookup(concrete_t)} ),
            .iterable => |iter_inner| return writer.print("{[t]f} | for<{[t]f}>", .{ .t = TypePool.lookup(iter_inner) }),
            .either => |either| return writer.print("{f} | {f}", .{ either.@"1".*, either.@"0".* }),
            .none => return writer.print("anytype", .{}),
        }
    }

    pub fn conform_to(from: Type, to: Type) ?Type {
        if (from == to) return to;
        if (from == Type.int and (to == Type.pitch or to == Type.pitch_note)) return to;
        // const from_full = TypePool.lookup(from);
        const to_full = TypePool.lookup(to);
        if (to_full == .note and to_full.note == from)
            return to;
        return null;
    }

    pub fn get_note_inner(self: TypeDesc, a: std.mem.Allocator) ?TypeDesc {
        switch (self) {
            .either => |either| {
                const n1 = either[0].get_note_inner(a);
                const n2 = either[1].get_note_inner(a);
                if (n1 == null and n2 == null) return null;
                if (n1 == null) return n2;
                if (n2 == null) return n1;
                const n1_a = a.create(TypeDesc) catch unreachable;
                n1_a.* = n1.?;
                const n2_a = a.create(TypeDesc) catch unreachable;
                n2_a.* = n2.?;
                return TypeDesc { .either = .{ n1_a, n2_a } };
            },
            .concrete, .iterable => |ty| {
                const ty_full = TypePool.lookup(ty);
                switch (ty_full) {
                    .note => |content| {
                        if (self == .concrete) return .{ .concrete = content };
                        if (self == .iterable) return .{ .iterable = content };
                        unreachable;
                    },
                    else => return null,
                }
            },
            .none => return null,
        }
    }

    // pub fn get_type_kind(self: TypeDesc, kind: TypePool.Kind) TypePool.TypeFull {
    //     switch (self) {
    //         .d
    //     } 
    // }

    pub fn get_iterable(self: TypeDesc) ?Type {
        switch (self) {
            .iterable => |t| return t,
            .either => |either| 
                return either.@"0".get_iterable() orelse either.@"1".get_iterable(),
            else => return null,
        }
    }

    pub fn get_list_el(self: TypeDesc) !TypeDesc {
        switch (self) {
            .none => return .none,
            .iterable,
            .concrete => |concrete| {
                if (concrete.take_list_inner()) |inner| {
                    return .{ .iterable = inner };
                }
                return Error.InsufficientInference;
            },
            .either => |either| {
                return either.@"0".get_list_el() catch either.@"1".get_list_el();
            },
        }
    }
};

pub fn sema(self: *Sema) Error!Annotations {
    // initialize builtin variables
    const a = self.a;

    // initialize builtin variables
    self.builtin_vars.putNoClobber(a, self.lexer.intern("true"), .{ Type.bool, 1 }) catch unreachable;
    self.builtin_vars.putNoClobber(a, self.lexer.intern("false"), .{ Type.bool, 0 }) catch unreachable;
    

    // initialize builltin types
    self.builtin_tys.putNoClobber(a, self.lexer.intern("Int"), Type.int) catch unreachable; 
    self.builtin_tys.putNoClobber(a, self.lexer.intern("Bool"), Type.bool) catch unreachable; 
    self.builtin_tys.putNoClobber(a, self.lexer.intern("Mode"), Type.mode) catch unreachable; 
    self.builtin_tys.putNoClobber(a, self.lexer.intern("Pitch"), Type.pitch) catch unreachable; 
    self.builtin_tys.putNoClobber(a, self.lexer.intern("Frac"), Type.fraction) catch unreachable; 
    // drum sey symbol
    var sym_set: []const Symbol = &.{
        self.lexer.intern("S"), 
        self.lexer.intern("B"),
        self.lexer.intern("H"),
    };

    const sym_set_ty = blk: {
        const sym_set_span = TypePool.Span(Symbol).init(&sym_set);
        break :blk TypePool.intern(.{ .sym_set = sym_set_span });
    };
    self.builtin_tys.putNoClobber(a, self.lexer.intern("DrumSetSymbol"), sym_set_ty) catch unreachable; 


    
    self.active_scope = &self.scope;
    // typecheck each toplevel declaration
    for (self.ast.toplevels) |formal| {
        try self.sema_formal(formal);
        if (formal.ident == Lexer.BuiltinSymbols.main)
            self.main_formal = formal;
    }
    if (self.main_formal == null) {
        self.lexer.report_err(0, "`main` is undefiend", .{});
        return Error.Undefined;
    }
    const sec_ty = try self.sema_expr(self.main_formal.?.expr, .none );
    const inner_sec_ty = sec_ty.take_list_inner() orelse sec_ty;
    const inner_sec_ty_full = TypePool.lookup(inner_sec_ty);
    switch (inner_sec_ty_full) {
        .section => |content_ty| {
            _ = content_ty;   
        },
        else => {
            self.lexer.report_err(self.main_formal.?.eq_off, "main must be a section or a list of section", .{});
        }
    }
    return Annotations {
        .main_formal = self.main_formal.?,
        .builtin_vars = self.builtin_vars,
        .expr_extras = self.expr_extras.toOwnedSlice(self.a) catch unreachable,
    };

}

pub fn sema_formal(self: *Sema, formal: *Ast.Formal) Error!void {
    try self.active_scope.fetch_put_else_report(
        self.lexer, self.a,
        formal.ident, .{ .decl = formal },
        formal.first_off());
}

pub fn sema_config(self: *Sema, formal: *Ast.Formal, config_tys: *const std.AutoHashMapUnmanaged(Symbol, Type)) Error!void {
    const expected_ty = config_tys.get(formal.ident) orelse {
        self.lexer.report_err_line(formal.first_off(), "unrecognized config name `{s}`", .{self.lexer.lookup(formal.ident)});   
        return Error.UnknownConfig;
    };
    _ = try self.sema_expr(formal.expr, .{.concrete = expected_ty });
}

pub fn sema_expr(self: *Sema, expr: *Ast.Expr, infer: TypeDesc) Error!Type {
    var data: ExprAnnotation.Data = undefined;
    const from = try self.sema_expr_impl(expr, infer, &data);
    const to = try self.expect_type(infer, from, expr.off);
    const anno = ExprAnnotation { .ty = to, .data = data };
    expr.anno_extra = self.anno_extra_index;
    self.anno_extra_index += 1;
    self.expr_extras.append(self.a, anno) catch unreachable;
    return anno.ty;
}

fn expect_type(self: *Sema, expected: TypeDesc, got: Type, off: u32) !Type {
    if (expected.validate(got)) |to| {
        return to;
    }
    self.lexer.report_err_line(off, "Expect Type `{f}`, got `{f}`", .{expected, got}); 
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
fn sema_expr_impl(self: *Sema, expr: *Ast.Expr, infer: TypeDesc, data: *ExprAnnotation.Data) !Type {
    switch (expr.data) {
        .num => {
            return Type.int; 
            // self.lexer.report_err_line(expr.off, "type of number literal cannot be inferred", .{});
            // self.lexer.report_line(expr.off);
            // return Error.InsufficientInference;
        },
        .ident => |*ident| {
            if (self.active_scope.get(ident.sym)) |entry| {
                data.* = .{ .formal_ref = entry.value_ptr.* };
                switch (entry.value_ptr.*) {
                    .builtin => |builtin| {
                        return builtin[0];
                    },
                    .decl => |decl| {
                        return self.sema_expr(decl.expr, infer);
                    },
                }
            }
            if (self.builtin_vars.get(ident.sym)) |builtin|
                return builtin[0];
            self.lexer.report_err_line(expr.off, "undefined variable `{s}`", .{self.lexer.lookup(ident.sym)});
            return Error.Undefined;
        },
        .sec => |sec| {
            var map = Scope { .parent = self.active_scope };
            self.active_scope = &map;
            defer self.active_scope = map.parent.?;

            const note_content_ty = try self.eval_type_expr(sec.ty);
            const note_ty = TypePool.intern(.{ .note = note_content_ty });
            const chord_ty = TypePool.intern(.{ .note = TypePool.intern(.{ .list = note_content_ty }) });


            var config_tys = std.AutoHashMapUnmanaged(Symbol, Type).empty;
            const a = self.a;
            // initialize config type
            const note_content_ty_full = TypePool.lookup(note_content_ty);
            switch (note_content_ty_full) {
                .pitch => {
                    config_tys.putNoClobber(a, self.lexer.intern("octave"), Type.int) catch unreachable;
                    config_tys.putNoClobber(a, self.lexer.intern("mode"), Type.mode) catch unreachable;
                    config_tys.putNoClobber(a, self.lexer.intern("tonic"), Type.pitch) catch unreachable;
                    const pitches = @typeInfo(Tonality.Pitch).@"enum".fields;
                    inline for (pitches) |pitch| {
                        map.fetch_put_builtin_assert(a, self.lexer.intern(pitch.name),
                            .{ Type.pitch, pitch.value });
                    }
                    const modes = @typeInfo(Tonality.Mode).@"enum".fields;
                    inline for (modes) |mode| {
                        map.fetch_put_builtin_assert(a, self.lexer.intern(mode.name),
                            .{ Type.mode, mode.value });
                    }
                    map.fetch_put_builtin_assert(a, self.lexer.intern("major"),
                        .{ Type.mode, @intFromEnum(Tonality.Mode.major) });
                    map.fetch_put_builtin_assert(a, self.lexer.intern("minor"),
                        .{ Type.mode, @intFromEnum(Tonality.Mode.minor) });

                },
                .sym_set => |sym_set| {
                    for (0..sym_set.len) |i| {
                        map.fetch_put_builtin_assert(a, sym_set.get(i),
                            .{ note_content_ty, @intCast(i) });
                    }
                },
                .int => {},
                else => {},
            }
            config_tys.putNoClobber(a, self.lexer.intern("bpm"), Type.int) catch unreachable;
            config_tys.putNoClobber(a, self.lexer.intern("tempo"), Type.fraction) catch unreachable;
            config_tys.putNoClobber(a, self.lexer.intern("instrument"), Type.int) catch unreachable;
            config_tys.putNoClobber(a, self.lexer.intern("volume"), Type.int) catch unreachable;

            for (sec.variable) |v| {
                try self.sema_formal(v);
            }
            for (sec.config) |config| {
                try self.sema_config(config, &config_tys);
            }

            const desc_note = TypeDesc { .iterable = note_ty } ;
            const desc_chord = TypeDesc { .iterable = chord_ty };
            // const desc_note_content = TypeDesc { .iterable = note_content_ty };
            // const desc_either = TypeDesc { .either = .{ &desc_note_content, &desc_chord } };
            const desc_either2 = TypeDesc { .either = .{ &desc_chord, &desc_note } };
            for (sec.notes) |note| {
                const ty = try self.sema_expr(note, desc_either2);
                _ = ty;
            }

            return TypePool.intern(.{ .section = note_content_ty });
        },
        .prefix => |prefix| {
            var desc_pitch = TypeDesc { .concrete = Type.pitch };
            var desc_chord = TypeDesc { .concrete = Type.chord };
            const either = TypeDesc { .either = .{&desc_pitch, &desc_chord } };

            return try self.sema_expr(prefix.rhs, either);
        },
        .infix => |infix| {
            switch (infix.op) {
                .single_quote => {
                    var arena = std.heap.ArenaAllocator.init(self.a);
                    defer arena.deinit();
                    const note_content_infer = infer.get_note_inner(arena.allocator()) orelse {
                        self.lexer.report_err_line(expr.off, "the type of the lhs of `'` cannot be inferred", .{});
                        return Error.InsufficientInference;
                    };
                    const note_content = try self.sema_expr(infix.lhs, note_content_infer);
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.fraction });
                    return TypePool.intern(.{ .note = note_content });
                },
                .slash => {
                    _ = try self.sema_expr(infix.lhs, .{ .concrete = Type.int });
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.int });
                    return Type.fraction; 
                },
                .plus, .minus, .times => {
                    _ = try self.sema_expr(infix.lhs, .{ .concrete = Type.int });
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.int });
                    return Type.int;
                },
                .le, .ge, .leq, .geq, .eq => {
                    _ = try self.sema_expr(infix.lhs, .{ .concrete = Type.int });
                    _ = try self.sema_expr(infix.rhs, .{ .concrete = Type.int });
                    return Type.bool;
                },
                else => unreachable,
            }
        },
        .list => |list| {
            const el_infer = infer.get_list_el() catch |e| {
                self.lexer.report_err_line(expr.first_off(), "inference {f} cannot be matched onto list", .{infer});
                return e;
            };
            if (list.els.len == 0) {
                return TypePool.intern(.{ .list = el_infer.iterable });
            }

            const el_ty = (try self.sema_expr(list.els[0], el_infer)).take_for_inner(); 
            for (list.els[1..], 2..) |el, i| {
                const other_ty = (try self.sema_expr(el, el_infer)).take_for_inner();
                if (el_ty != other_ty) {
                    self.lexer.report_err_line(el.off, "exprssions in list must have the same type; 1th is {f}, {}th is {f}", 
                        .{TypePool.lookup(el_ty), i, TypePool.lookup(other_ty)});
                    self.lexer.report_note_line(list.els[0].off, "first expression is here", .{});
                    return Error.TypeMismatched;
                }
            }
            return TypePool.intern(.{ .list = el_ty });
        },
        .sequence => |seq| {
            if (seq.els.len == 0) {
                return infer.get_iterable() orelse {
                    self.lexer.report_err_line(expr.off, "the type of an empty sequence cannot be inferred", .{});
                    return Error.InsufficientInference;
                };
            }
            const el_ty = (try self.sema_expr(seq.els[0], infer)).take_for_inner();

            for (seq.els[1..], 2..) |el, i| {
                const other_ty = (try self.sema_expr(el, infer)).take_for_inner();
                if (el_ty != other_ty) {
                    self.lexer.report_err_line(el.off, "exprssions in for loop must have the same type; 1th is {f}, {}th is {f}", 
                        .{TypePool.lookup(el_ty), i, TypePool.lookup(other_ty)});
                    self.lexer.report_note_line(seq.els[0].off, "first expression is here", .{});
                    return Error.TypeMismatched;
                }
            }
            return TypePool.intern(.{ .splat = el_ty });
        },
        .@"for" => |@"for"| {

            if (@"for".with) |with| {
                var map = Scope { .parent = self.active_scope };
                self.active_scope = &map;
                try self.active_scope.fetch_put_else_report(self.lexer, self.a, 
                    with.ident, .{ .decl = with }, 
                    with.first_off());
            }
            defer {
                if (@"for".with) |_| self.active_scope = self.active_scope.parent.?;
            }

            _ = try self.sema_expr(@"for".lhs, .{ .concrete = Type.int });
            _ = try self.sema_expr(@"for".rhs, .{ .concrete = Type.int });
            if (@"for".body.len == 0) {
                return infer.get_iterable() orelse {
                    self.lexer.report_err_line(expr.off, "the type of an empty for loop cannot be inferred", .{});
                    return Error.InsufficientInference;
                };
            }
            const el_ty = (try self.sema_expr(@"for".body[0], infer)).take_for_inner();
             
            for (@"for".body[1..], 2..) |el, i| {
                const other_ty = (try self.sema_expr(el, infer)).take_for_inner();
                if (el_ty != other_ty) {
                    self.lexer.report_err_line(el.off, "exprssions in for loop must have the same type; 1th is {f}, {}th is {f}", 
                        .{TypePool.lookup(el_ty), i, TypePool.lookup(other_ty)});
                    self.lexer.report_note_line(@"for".body[0].off, "first expression is here", .{});
                    return Error.TypeMismatched;
                }
            }
            return TypePool.intern(.{.splat = el_ty});
        },
        .@"if" => |@"if"| {
            _ = try self.sema_expr(@"if".cond, .{ .concrete = Type.bool });
            const then_ty = try self.sema_expr(@"if".then, infer);
            const else_ty = try self.sema_expr(@"if".@"else", infer);
            if (then_ty != else_ty) {
                self.lexer.report_err_line(@"if".then.first_off(), "the type of the else branch `{f}` != the type of the then branch `{f}`", .{then_ty, else_ty});
                self.lexer.report_note_line(@"if".@"else".first_off(), "else branch is here", .{});
                return Error.TypeMismatched;
            }
            return then_ty;
        },
        .tag_decl => |_| {
            @panic("TODO");
        },
        .tag_use => |_| {
            @panic("TODO");
        },
    }
}

fn eval_type_expr(self: *Sema, expr: *Ast.TypeExpr) Error!Type {
    switch (expr.data) {
        .ident => |sym| {
            return self.builtin_tys.get(sym) orelse {
                self.lexer.report_err_line(expr.first_off(), "Undefined type `{s}`", .{self.lexer.lookup(sym)});
                return Error.Undefined;
            };
        }
    }
    return Type.pitch;
}

const expect = std.testing.expect;
const expectEqual = std.testing.expectEqual;

test "conform" {
    const ta = std.testing.allocator;

    TypePool.init_global_type_pool(ta);
    defer TypePool.deinit();
    InternPool.init_global_string_pool(ta);
    defer InternPool.deinit_global_string_pool();

    const note_interned = TypePool.intern(.{ .note = Type.pitch });
    try expect(note_interned == Type.pitch_note);

    try expect(TypeDesc.conform_to(Type.pitch, Type.pitch_note) == Type.pitch_note);

    const int_note_interned = TypePool.intern(.{ .note = Type.int });
    try expect(TypeDesc.conform_to(Type.int, int_note_interned) == int_note_interned);

    const string_pool = &InternPool.string_pool;
    const sym_set: []const Symbol =  &.{
        string_pool.intern("Cat"), 
        string_pool.intern("Dog"),
        string_pool.intern("Mouse"),
    };

    const sym_set_ty = blk: {
        const sym_set_span = TypePool.Span(Symbol).init(&sym_set);
        break :blk TypePool.intern(.{ .sym_set = sym_set_span });

    };
    const sym_set_span = TypePool.lookup(sym_set_ty).sym_set;
    try expectEqual(sym_set_span.len, sym_set.len);
    for (0..sym_set_span.len) |i| {
        try expectEqual(sym_set[i], sym_set_span.get(i));
    }
}
fn test_validate(desc: TypeDesc, t: Type) !void {
   return test_validate2(desc, t, t); 
}

fn test_validate2(desc: TypeDesc, t: Type, expected: Type) !void {
    const res_or_null = desc.validate(t);
    if (res_or_null) |res| {
        if (res != expected) std.log.err("expect {f}, got {f}", .{t, res})
        else return; 
    } else
        std.log.err("expect {f}, got null", .{t});
    return error.Invalid;
}

test "desc validate" {
    const ta = std.testing.allocator;
    TypePool.init_global_type_pool(ta);
    defer TypePool.deinit();

    const desc_note = TypeDesc { .iterable = Type.pitch_note } ;
    const desc_chord = TypeDesc { .iterable = TypePool.intern(.{ .list = Type.pitch_note }) };
    // const desc_note_content = TypeDesc { .iterable = note_content_ty };
    // const desc_either = TypeDesc { .either = .{ &desc_note_content, &desc_chord } };
    const desc_either = TypeDesc { .either = .{ &desc_chord, &desc_note } };
    _ = desc_either;

    try test_validate(desc_note, Type.pitch_note);
    try test_validate(desc_note, TypePool.intern(.{ .splat = Type.pitch_note }));
    try test_validate2(desc_note, Type.pitch, Type.pitch_note);
    try test_validate2(desc_note, Type.int, Type.pitch_note);
}
