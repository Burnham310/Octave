const std = @import("std");

const Lexer = @import("lexer.zig");
const Symbol = Lexer.Symbol;
const TypePool = @import("type_pool.zig");

const Ast = @This();

pub const Expr = struct {
    ty: TypePool.Type = undefined,
    off: u32,
    data: union(enum) {
        num: isize,
        ident: Symbol,
        sec: *Section,
        infix: Infix,
    },

    pub const Section = struct {
        rcurly_off: u32,
        config: []*Formal,
        notes: []*Expr,
        pub fn dump(self: Section, writer: anytype, lexer: Lexer, level: u32) void {
            for (self.config) |formal| {
                formal.dump(writer, lexer, level+1);
            }
            for (self.notes) |expr| {
                expr.dump(writer, lexer, level+1);
            }
        }
    };

    pub const Infix = struct {
        op: Lexer.TokenType,
        lhs: *Expr,
        rhs: *Expr,
    };

    pub fn first_off(self: Expr) u32 {
        switch (self.data) {
            .num, .ident, .sec => return self.off,
            .infix => |infix| return infix.lhs.first_off(),
        }
    }
    
    pub fn last_off(self: Expr, lexer: Lexer) u32 {
        switch (self.data) {
           .num => return @as(u32, @intCast(lexer.re_int_impl(self.off).len)) + self.off,
           .ident => return @as(u32, @intCast(lexer.re_ident_impl(self.off).len)) + self.off,
           .infix => |infix| return infix.rhs.last_off(lexer),
           .sec => |sec| return sec.rcurly_off,
        }
    }

    pub fn dump(self: Expr, writer: anytype, lexer: Lexer, level: u32) void {
        for (0..level) |_|
            writer.writeByte(' ') catch unreachable; 
        _ = writer.write("|-") catch unreachable;
                switch (self.data) {
            .num => |num| {
                _ = writer.print("Num<{}>", .{num}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
            },
            .ident => |ident| {
                _ = writer.print("Ident<{s}>", .{lexer.lookup(ident)}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
            },
            .sec => |section| {
                _ = writer.write("Section") catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                section.dump(writer, lexer, level);
            },
            .infix => |infix| {
                _ = writer.print("{}", .{infix.op}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                infix.lhs.dump(writer, lexer, level+1);
                infix.rhs.dump(writer, lexer, level+1);
            },

        }
    }
};

pub const Formal = struct {
    eq_off: u32,
    ident_off: u32,
    ident: Symbol,
    expr: *Expr,

    pub fn dump(self: Formal, writer: anytype, lexer: Lexer, level: u32) void {
        for (0..level) |_|
            writer.writeByte(' ') catch unreachable; 

        _ = writer.write("|-") catch unreachable; 
        _ = writer.write(lexer.lookup(self.ident)) catch unreachable; 
        writer.writeByte('=') catch unreachable; 
        writer.writeByte('\n') catch unreachable; 
        self.expr.dump(writer, lexer, level+1);
    }

    pub fn first_of(self: Formal) u32 {
       return self.ident_off; 
    }

    pub fn last_off(self: Formal, lexer: Lexer) u32 {
        return self.expr.last_off(lexer);
    }
};

toplevels: []*Formal,
formals: std.SegmentedList(Formal, 1),
exprs: std.SegmentedList(Expr, 0),
secs: std.SegmentedList(Expr.Section, 1),

pub fn dump(self: Ast, writer: anytype, lexer: Lexer) void {
    for (self.toplevels) |toplevel| {
        toplevel.dump(writer, lexer, 0);
    }
}
