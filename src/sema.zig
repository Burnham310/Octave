const std = @import("std");
const Lexer =  @import("lexer.zig");
const Ast = @import("ast.zig");
const TypePool = @import("type_pool.zig");
const Type = TypePool.Type;

const Sema = @This();

ast: *Ast,
lexer: *Lexer,
main_formal: ?*Ast.Formal = null,

pub const Annotations = struct {
    main_formal: *Ast.Formal,
};

const Error = error {
    TypeMismatched,
    Undefined,
};

pub fn sema(self: *Sema) Error!Annotations {
    for (self.ast.toplevels) |formal| {
       try self.sema_formal(formal);
    }
    if (self.main_formal == null) {
        self.lexer.report_err(0, "main is undefiend", .{});
        return Error.Undefined;
    }
    return Annotations {.main_formal = self.main_formal.? };

}

pub fn sema_formal(self: *Sema, formal: *Ast.Formal) Error!void {
    if (formal.ident == Lexer.BuiltinSymbols.main)
        self.main_formal = formal;
    try self.expect_type(Type.section, try self.sema_expr(formal.expr, null), formal.ident_off);
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

pub fn sema_expr_impl(self: *Sema, expr: *Ast.Expr, _: ?Type) !Type {
    switch (expr.data) {
        .num => {
            //if (infer == TypePool.note) return TypePool.note;
            return Type.int;
        },
        .ident => @panic("TODO"),
        .sec => |sec| {
            if (sec.config.len != 0) 
                @panic("TODO");
            for (sec.notes) |note| {
                const allowed_tys = [_]Type {Type.note, Type.chord, Type.int, TypePool.intern(.{.list = Type.int})};
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
        .infix => |infix| {
           switch (infix.op) {
               .single_quote => {
                   try self.expect_type_mappable(Type.int, try self.sema_expr(infix.lhs, null), infix.rhs.off);
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
            if (list.els.len == 0) return Type.@"void";
            const el_ty = try self.sema_expr(list.els[0], null); 
            for (list.els[1..], 2..) |el, i| {
                const other_ty = try self.sema_expr(el, Type.int);
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
