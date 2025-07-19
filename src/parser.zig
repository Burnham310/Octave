const std = @import("std");

const Ast = @import("ast.zig");
const Formal = Ast.Formal;
const Expr = Ast.Expr;
const Section = Expr.Section;

const Lexer = @import("lexer.zig");
const Token = Lexer.Token;
const TokenType = Lexer.TokenType;

const Parser = @This();

formals: std.SegmentedList(Formal, 1) = .{},
exprs: std.SegmentedList(Expr, 0) = .{},
secs: std.SegmentedList(Section, 1) = .{},

lexer: *Lexer,

a: std.mem.Allocator,

const Error = error {
    UnexpectedToken,
    EndOfStream,
} || Lexer.Error;

//pub fn init(a: std.mem.Allocator) Parser {
//    return 
//}

pub fn parse(self: *Parser) Error!Ast {
    var toplevels = std.ArrayListUnmanaged(*Formal) {};

    while (true) {
        const formal = self.parse_formal() catch {
            self.lexer.skip_to_new_line_star();
            continue;
        } orelse break;
        toplevels.append(self.a, formal) catch unreachable;
    }
    const last = if (toplevels.getLastOrNull()) |last| last.last_off(self.lexer.*) else 0;
    _ = try self.expect_token_crit_off(.eof, last, "declaration");

    return Ast {
        .toplevels = toplevels.toOwnedSlice(self.a) catch unreachable,
        .formals = self.formals,
        .exprs = self.exprs,
        .secs = self.secs,
    };
}

pub inline fn create(self: *Parser, el: anytype) *@TypeOf(el)  {
    const T = @TypeOf(el);
    const list = if (T == Expr) &self.exprs else if (T == Formal) &self.formals else if (T == Section) &self.secs else @compileError("Unknown el type " ++ @typeName(T));
    list.append(self.a, el) catch unreachable;
    return list.uncheckedAt(list.count()-1);
}

pub fn parse_list_of(self: *Parser, comptime T: type, f: fn (*Parser) Error!?T) Error![]T {
    var list = std.ArrayListUnmanaged(T) {};
    defer list.deinit(self.a);
    const first = try f(self) orelse return (list.toOwnedSlice(self.a) catch unreachable);
    list.append(self.a, first) catch unreachable;
    while (true) {
        const tk = try self.lexer.peek();
        switch (tk.tag) {
            .comma => {
                self.lexer.consume();
                const item = try f(self) orelse {
                    self.lexer.report_err(tk.off, "Expect item after comma", .{});
                    return Error.UnexpectedToken;
                };
                list.append(self.a, item) catch unreachable;
            },
            else => break,
        }
    }
    return list.toOwnedSlice(self.a) catch unreachable;
}

pub fn parse_formal(self: *Parser) Error!?*Formal {
    const ident = try self.expect_token(.ident) orelse return null;
    const eq = try self.expect_token_crit(.eq, ident);
    const expr = try self.parse_expr() orelse {
        self.lexer.report_err(eq.off, "Expect expression after `{}`", .{eq.tag});
        return Error.UnexpectedToken;
    };
    
    return self.create(Ast.Formal {.ident_off = ident.off, .eq_off = eq.off, .ident = self.lexer.re_ident(ident.off), .expr = expr } );
}

pub fn parse_atomic_expr(self: *Parser) Error!?*Expr {
    const tok = try self.lexer.peek();
    switch (tok.tag) {
        .ident => {
            self.lexer.consume();
            return self.create(Expr {.off = tok.off, .data = .{ .ident = self.lexer.re_ident(tok.off) }});
        },
        .int => {
            self.lexer.consume();
            return self.create(Expr {.off = tok.off, .data = .{.num = self.lexer.re_int(tok.off) }});
        },
        .lparen => {
            self.lexer.consume();
            const expr = try self.parse_expr() orelse {
                self.lexer.report_err(tok.off, "Expect expression after `(`.", .{});
                self.lexer.report_line(tok.off);
                return Error.UnexpectedToken;
            };
            _ = self.expect_token_crit_off(.rparen, expr.off, "expression") catch |e| {
                self.lexer.report_note(tok.off, "left parenthesis `(` is here", .{});
                self.lexer.report_line(tok.off);
                return e;
            };
            return expr;

        },
        else => return null,
    }
}

pub fn parse_expr(self: *Parser) Error!?*Expr {
    return self.parse_expr_climb(0);
}



pub fn parse_prefix(self: *Parser) Error!?*Expr {
    const tok = try self.lexer.peek();
    switch (tok.tag) {
        .lcurly => {
            self.lexer.consume();
            var expr_list = std.ArrayListUnmanaged(*Expr) {};
            const formal_list = try self.parse_list_of(*Formal, parse_formal);
            const last_formal, const last_formal_name = self.get_slice_last_or_tok(formal_list, tok);
            const semi_colon = try self.expect_token_crit_off(.semi_colon, last_formal, last_formal_name);
            while (try self.parse_expr()) |expr| {
                expr_list.append(self.a, expr) catch unreachable;
            }
            const last_expr, const last_expr_name = self.get_last_or_tok(expr_list, semi_colon);
            const rcurly = try self.expect_token_crit_off(.rcurly, last_expr, last_expr_name);
            const sec = Section {
                .rcurly_off = rcurly.off,
                .config = formal_list,
                .notes = expr_list.toOwnedSlice(self.a) catch unreachable
            };
            return self.create(Expr {.off = tok.off, .data = .{.sec = self.create(sec) }});
        },
        .lbrac => {
            self.lexer.consume();
            var expr_list = std.ArrayListUnmanaged(*Expr) {};
            while (try self.parse_expr()) |expr| {
                expr_list.append(self.a, expr) catch unreachable;
            }
            const last_expr, const last_expr_name = self.get_last_or_tok(expr_list, tok);
            const rbrac = try self.expect_token_crit_off(.rbrac, last_expr, last_expr_name);
            const list = Expr.List {.els = expr_list.toOwnedSlice(self.a) catch unreachable, .rbrac_off = rbrac.off };
            return self.create(Expr {.off = tok.off, .data = .{.list = list}});

        },
        .slash => {
            self.lexer.consume();
            const rhs = try self.parse_expr() orelse {
                self.lexer.report_err(tok.off, "expect expression after `/`", .{});
                self.lexer.report_line(tok.off);
                return Error.UnexpectedToken;
            };
            const lhs = self.create(Expr {.off = tok.off, .data = .{.num = 1 }});
            const infix = Expr.Infix {.lhs = lhs, .rhs = rhs, .op = tok.tag };
            return self.create(Expr {.off = tok.off, .data = .{.infix = infix}});
        },
        else => return null,
    } 
}

fn prefix_bp(tt: TokenType) ?u32 {
    switch (tt) {
        .slash => 30,
        
    }
}

fn infix_bp(tt: TokenType) ?struct {u32, u32} {
    return switch (tt) {
        .plus, .minus => .{5, 4},
        .times => .{7, 6},
        .single_quote => .{19, 20},
        .slash => .{29, 30},
        else => null,
    };
}

fn postfix_bp(tt: TokenType) ?u32 {
    return switch (tt) {
        else => null,
    };
}

pub fn parse_expr_climb(self: *Parser, min_bp: u32) Error!?*Expr {
    var lhs = try self.parse_prefix() orelse try self.parse_atomic_expr() orelse return null; // TODO parse prefix expr
   
    while (true) {
        const tk = try self.lexer.peek();
        lhs = if (postfix_bp(tk.tag)) |lbp| {
            if (lbp < min_bp) break;
            self.lexer.consume();
            @panic("TODO");
            // return make postfix
        } else if (infix_bp(tk.tag)) |bp| infix: {
            const lbp, const rbp = bp;
            if (lbp < min_bp or (lbp == rbp and lbp == min_bp)) break;
            self.lexer.consume();
            const rhs = try self.parse_expr_climb(rbp) orelse {
                self.lexer.report_err(tk.off, "Expect expression after `{s}`", .{self.lexer.stringify_token(tk)});
                self.lexer.report_line(tk.off);
                return Error.UnexpectedToken;
            };
            const infix = Expr.Infix {.op = tk.tag, .lhs = lhs, .rhs = rhs };
            break :infix self.create(Expr {.off = tk.off, .data = .{.infix = infix }});
        } else break;
    }

    return lhs;
}


pub fn expect_token_crit(self: *Parser, kind: TokenType, before: Token) !Token {
    return self.expect_token_crit_off(kind, before.off, self.lexer.stringify_token(before));
}

pub fn expect_token_crit_off(self: *Parser, kind: TokenType, off: u32, before: []const u8) !Token {
    const tok = self.lexer.next() catch |e| {
        self.lexer.report_err(off, "Expect {} after `{s}`, but encounter {}", .{ kind, before, e });
        self.lexer.report_line(off);
        return Error.EndOfStream;
    };
    if (tok.tag != kind) {
        self.lexer.report_err(tok.off, "Expect {} after `{s}`, found {s}", .{ kind, before, self.lexer.stringify_token(tok) });
        self.lexer.report_line(tok.off);
        return Error.UnexpectedToken;
    }
    return tok;
}

pub fn expect_token(self: *Parser, kind: TokenType) !?Token {
    const tok = try self.lexer.peek();
    if (tok.tag != kind) {
        return null;
    }
    self.lexer.consume();
    return tok;
}

pub fn get_last_or_tok(self: Parser, nodes: anytype, tok: Token) struct {u32, []const u8} {
    const T =  @TypeOf(nodes.items[0]);
    const name = if (T == *Expr) "expression" else if (T == *Formal) "formal" else @compileError("Unknown Type " ++ @typeName(T));
    return 
        if (nodes.getLastOrNull()) |node| .{node.last_off(self.lexer.*), name} 
        else .{tok.off, self.lexer.stringify_token(tok)};
}

pub fn get_slice_last_or_tok(self: Parser, nodes: anytype, tok: Token) struct {u32, []const u8} {
    const T =  @TypeOf(nodes[0]);
    const name = if (T == *Expr) "expression" else if (T == *Formal) "formal" else @compileError("Unknown Type " ++ @typeName(T));
    return 
        if (nodes.len > 0) .{nodes[nodes.len-1].last_off(self.lexer.*), name} 
        else .{tok.off, self.lexer.stringify_token(tok)};
}
