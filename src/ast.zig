const std = @import("std");

const Lexer = @import("lexer.zig");
const Symbol = Lexer.Symbol;
const TypePool = @import("type_pool.zig");

const Ast = @This();

pub const Expr = struct {
    anno_extra: u32 = undefined,
    off: u32,
    data: union(enum) {
        num: isize,
        ident: Ident,
        sec: *Section,
        infix: Infix,
        prefix: Prefix,
        list: List,
        sequence: List,
        @"for": For,
        @"if": If,
    },

    pub const Ident = struct {
        sym: Symbol,
        sema_expr: *Expr = undefined,
    };

    pub const Section = struct {
        rcurly_off: u32,
        variable: []*Formal,
        config: []*Formal,
        notes: []*Expr,
        pub fn dump(self: Section, writer: *std.io.Writer, lexer: Lexer, level: u32) void {
            for (self.variable) |formal| {
                formal.dump(writer, lexer, level+1);
            }
            for (self.config) |formal| {
                formal.dump(writer, lexer, level+1);
            }
            for (self.notes) |expr| {
                expr.dump(writer, lexer, level+1);
            }
        }
    };

    pub const List = struct {
        els: []*Expr,
        rbrac_off: u32,
    };

    pub const Prefix = struct {
        op: Lexer.TokenType,
        rhs: *Expr,
    };

    pub const Infix = struct {
        op: Lexer.TokenType,
        lhs: *Expr,
        rhs: *Expr,
    };

    pub const For = struct {
        lhs: *Expr,
        rhs: *Expr,
        body: []*Expr,
        end_off: u32,
        with: ?*Formal,
    };

    pub const If = struct {
        cond: *Expr,
        then: *Expr,
        @"else": *Expr,

    };

    pub fn first_off(self: Expr) u32 {
        switch (self.data) {
            .num, .ident, .sec, .list, .sequence, .prefix, .@"for", .@"if" => return self.off,
            .infix => |infix| return infix.lhs.first_off(),
        }
    }
    
    pub fn last_off(self: Expr, lexer: Lexer) u32 {
        switch (self.data) {
           .num => return @as(u32, @intCast(lexer.re_int_impl(self.off).len)) + self.off,
           .ident => return @as(u32, @intCast(lexer.re_ident_impl(self.off).len)) + self.off,
           .infix => |infix| return infix.rhs.last_off(lexer),
           .prefix => |prefix| return prefix.rhs.last_off(lexer),
           .sec => |sec| return sec.rcurly_off,
           .list, .sequence => |list| return list.rbrac_off,
           .@"for" => |@"for"| return @"for".end_off,
           .@"if" => |@"if"| return @"if".@"else".last_off(lexer),
        }
    }

    pub fn dump(self: Expr, writer: *std.io.Writer, lexer: Lexer, level: u32) void {
        for (0..level) |_|
            writer.writeByte(' ') catch unreachable; 
        _ = writer.write("|-") catch unreachable;
        switch (self.data) {
            .num => |num| {
                _ = writer.print("Num<{}>", .{num}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
            },
            .ident => |ident| {
                _ = writer.print("Ident<{s}>", .{lexer.lookup(ident.sym)}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
            },
            .sec => |section| {
                _ = writer.write("Section") catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                section.dump(writer, lexer, level);
            },
            .prefix => |prefix| {
                _ = writer.print("Prefix<{s}>", .{@tagName(prefix.op)}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                prefix.rhs.dump(writer, lexer, level+1);
            },
            .infix => |infix| {
                _ = writer.print("Infix<{s}>", .{@tagName(infix.op)}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                infix.lhs.dump(writer, lexer, level+1);
                infix.rhs.dump(writer, lexer, level+1);
            },
            .list => |list| {
                _ = writer.print("List<{}>", .{list.els.len}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                for (list.els) |el| {
                    el.dump(writer, lexer, level+1);
                }
            },
            .sequence => |list| {
                _ = writer.print("Sequence<{}>", .{list.els.len}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                for (list.els) |el| {
                    el.dump(writer, lexer, level+1);
                }
            },
            .@"for" => |@"for"| {
                _ = writer.print("For", .{}) catch unreachable;
                writer.writeByte('\n') catch unreachable; 
                @"for".lhs.dump(writer, lexer, level+1);
                @"for".rhs.dump(writer, lexer, level+1);
                for (@"for".body) |body| {
                    body.dump(writer, lexer, level+1);
                }
            },
            .@"if" => |@"if"| {
                _ = writer.print("If", .{}) catch unreachable;
                @"if".cond.dump(writer, lexer, level+1);
                @"if".then.dump(writer, lexer, level+1);
                @"if".@"else".dump(writer, lexer, level+1);
            },

        }
    }

    
};

pub const Formal = struct {
    eq_off: u32,
    ident_off: u32,
    ident: Symbol,
    expr: *Expr,

    pub fn dump(self: Formal, writer: *std.io.Writer, lexer: Lexer, level: u32) void {
        for (0..level) |_|
            writer.writeByte(' ') catch unreachable; 

        _ = writer.write("|-") catch unreachable; 
        _ = writer.write(lexer.lookup(self.ident)) catch unreachable; 
        writer.writeByte('=') catch unreachable; 
        writer.writeByte('\n') catch unreachable; 
        self.expr.dump(writer, lexer, level+1);
    }

    pub fn first_off(self: Formal) u32 {
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

pub fn dump(self: Ast, writer: *std.io.Writer, lexer: Lexer) void {
    for (self.toplevels) |toplevel| {
        toplevel.dump(writer, lexer, 0);
    }
}
