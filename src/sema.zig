const std = @import("std");
const Lexer =  @import("lexer.zig");
const Ast = @import("ast.zig");
const TypePool = @import("type_pool.zig");
const Type = TypePool.Type;

const Sema = @This();

ast: *Ast,
lexer: *Lexer,
main_formal: ?*Ast.Formal = undefined,

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
    try self.expect_type(TypePool.section, try self.sema_expr(formal.expr, null), formal.ident_off);
}

pub fn sema_expr(self: *Sema, expr: *Ast.Expr, infer: ?Type) Error!Type {
    expr.ty = try self.sema_expr_impl(expr, infer);
    return expr.ty;
}

pub fn expect_type(self: *Sema, expected: Type, got: Type, off: u32) !void {
    if (expected == got) {
        return;
    }
    self.lexer.report_err(off, "Expect Type `{}`, got `{}`", .{expected, got}); 
    self.lexer.report_line(off);
    return Error.TypeMismatched;

}

pub fn sema_expr_impl(self: *Sema, expr: *Ast.Expr, infer: ?Type) !Type {
    switch (expr.data) {
        .num => {
            if (infer == TypePool.note) return TypePool.note;
            return TypePool.int;
        },
        .ident => @panic("TODO"),
        .sec => |sec| {
            if (sec.config.len != 0) 
                @panic("TODO");
            for (sec.notes) |note| {
                try self.expect_type(TypePool.note, try self.sema_expr(note, TypePool.note), note.off);
            }
            return TypePool.section;
        },
        .infix => |infix| {
           switch (infix.op) {
               .single_quote => {
                   try self.expect_type(TypePool.int, try self.sema_expr(infix.lhs, TypePool.int), infix.lhs.off); // TODO: degree?
                   try self.expect_type(TypePool.int, try self.sema_expr(infix.rhs, TypePool.int), infix.rhs.off);
                   return TypePool.note; 
               },
               else => @panic("TODO"),
           }
        },
    }
}
