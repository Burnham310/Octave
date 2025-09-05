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

pub const Error = error {
    UnexpectedToken,
    EndOfStream,
};

const ErrorBoth = Error || Lexer.Error;

//pub fn init(a: std.mem.Allocator) Parser {
//    return 
//}

pub fn parse(self: *Parser) ErrorBoth!Ast {
    var toplevels = std.ArrayListUnmanaged(*Formal) {};

    while (true) {
        const formal = try self.parse_formal() orelse break;
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

pub fn parse_list_of(self: *Parser, comptime T: type, f: fn (*Parser) ErrorBoth!?T) ErrorBoth![]T {
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
                    self.lexer.report_err_line(tk.off, "Expect item after comma", .{});
                    return ErrorBoth.UnexpectedToken;
                };
                list.append(self.a, item) catch unreachable;
            },
            else => break,
        }
    }
    return list.toOwnedSlice(self.a) catch unreachable;
}

pub fn parse_formal(self: *Parser) ErrorBoth!?*Formal {
    const ident = try self.expect_token(.ident) orelse return null;
    const eq = try self.expect_token_crit(.eq, ident);
    const expr = try self.parse_expr() orelse {
        self.lexer.report_err_line(eq.off, "Expect expression after `{}`", .{eq.tag});
        return ErrorBoth.UnexpectedToken;
    };
    
    return self.create(Ast.Formal {.ident_off = ident.off, .eq_off = eq.off, .ident = self.lexer.re_ident(ident.off), .expr = expr } );
}

pub fn parse_atomic_expr(self: *Parser) ErrorBoth!?*Expr {
    const tok = try self.lexer.peek();
    switch (tok.tag) {
        .ident => {
            self.lexer.consume();
            return self.create(Expr {.off = tok.off, .data = .{ .ident = .{.sym = self.lexer.re_ident(tok.off) }}});
        },
        .int => {
            self.lexer.consume();
            return self.create(Expr {.off = tok.off, .data = .{.num = self.lexer.re_int(tok.off) }});
        },
        .lparen => {
            self.lexer.consume();
            const expr = try self.parse_expr() orelse {
                self.lexer.report_err_line(tok.off, "Expect expression after `(`.", .{});
                return ErrorBoth.UnexpectedToken;
            };
            _ = self.expect_token_crit_off(.rparen, expr.off, "expression") catch |e| {
                self.lexer.report_note_line(tok.off, "left parenthesis `(` is here", .{});
                return e;
            };
            return expr;

        },
        else => return null,
    }
}

pub fn parse_expr(self: *Parser) ErrorBoth!?*Expr {
    return self.parse_expr_climb(0);
}

const Sequence = struct {
    loff: u32,
    roff: u32,
    exprs: []*Expr,  
};

fn parse_sequence(self: *Parser, before_off: u32, before_name: []const u8) ErrorBoth!Sequence {
    const lcurly = try self.expect_token_crit_off(.lcurly, before_off, before_name);
    var expr_list = std.ArrayListUnmanaged(*Expr) {};

    while (try self.parse_expr()) |expr| {
        expr_list.append(self.a, expr) catch unreachable;
    }

    const last_expr, const last_expr_name = self.get_last_or_tok(expr_list, lcurly);
    const rcurly = try self.expect_token_crit_off(.rcurly, last_expr, last_expr_name);
    return Sequence { .loff = lcurly.off, .exprs = expr_list.toOwnedSlice(self.a) catch unreachable, .roff = rcurly.off };
}

pub fn parse_prefix(self: *Parser) ErrorBoth!?*Expr {
    const tok = try self.lexer.peek();
    if (prefix_bp(tok.tag)) |bp| {
        self.lexer.consume();
        const rhs = try self.parse_expr_climb(bp) orelse {
            self.lexer.report_err_line(tok.off, "Expect expression after prefix operator `{s}`", .{self.lexer.stringify_token(tok)});
            return ErrorBoth.UnexpectedToken;
        };
        if (tok.tag == .slash) {
            const lhs = self.create(Expr {.off = tok.off, .data = .{.num = 1 }});
            const infix = Expr.Infix {.lhs = lhs, .rhs = rhs, .op = tok.tag };
            return self.create(Expr {.off = tok.off, .data = .{.infix = infix}});
        }
        const prefix = Expr.Prefix {.rhs = rhs, .op = tok.tag};
        return self.create(Expr {.off = tok.off, .data = .{.prefix = prefix}});       
    }

    switch (tok.tag) {
        .section => {
            self.lexer.consume();
            const formal_list1 = try self.parse_list_of(*Formal, parse_formal);
            const last_formal1, const last_formal_name1 = self.get_slice_last_or_tok(formal_list1, tok);
            const semi_colon1 = try self.expect_token_crit_off(.semi_colon, last_formal1, last_formal_name1);

            const formal_list2 = try self.parse_list_of(*Formal, parse_formal);
            const last_formal2, const last_formal_name2 = self.get_slice_last_or_tok(formal_list2, semi_colon1);
            const semi_colon2 = try self.expect_token_crit_off(.semi_colon, last_formal2, last_formal_name2);

            const seq = try self.parse_sequence(semi_colon2.off, @tagName(semi_colon2.tag));
            
            const sec = Section {
                .rcurly_off = seq.roff,
                .variable = formal_list1,
                .config = formal_list2,
                .notes = seq.exprs,
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
        .lcurly => {
            // because we know the next token is lcurly, the error reporting would never be called
            // so we can safely pass gibberish here
            const seq = try self.parse_sequence(undefined, undefined);
            const list = Expr.List {.els = seq.exprs, .rbrac_off = seq.roff };
            return self.create(Expr {.off = tok.off, .data = .{.sequence = list}});
        },
        .@"for" => {
            self.lexer.consume();
            const lhs = try self.parse_expr() orelse {
                self.lexer.report_err_line(tok.off, "expect expression after `for`", .{});
                return Error.UnexpectedToken;
            };
            const tilde = try self.expect_token_crit_off(.tilde, lhs.last_off(self.lexer.*), "expr");
            const le = try self.expect_token_crit(.le, tilde);
            const rhs = try self.parse_expr() orelse {
                self.lexer.report_err_line(le.off, "expect expression after `<`", .{});
                return Error.UnexpectedToken;
            };
            const peek = try self.lexer.peek();
            const ident: ?*Formal = switch (peek.tag) {
               .with => blk: {
                    self.lexer.consume();
                    const ident = try self.expect_token_crit(.ident, peek);
                    const expr = self.create(Expr {.off = peek.off, .data = .{.num = 0}});
                    const sym =  self.lexer.re_ident(ident.off);
                    break :blk self.create(Formal {.ident = sym, .expr = expr, .eq_off = peek.off, .ident_off = peek.off });
                },
                .lcurly => null,
                else => |other| {
                    self.lexer.report_err_line(rhs.last_off(self.lexer.*), "expect `with` or `{{`, found {t}", .{other});
                    return Error.UnexpectedToken;
                }
            };
            const seq = try self.parse_sequence(
                if (ident) |id| id.ident_off else rhs.last_off(self.lexer.*), 
                if (ident) |id| self.lexer.lookup(id.ident) else "rhs");
            const @"for" = Expr.For {
                .lhs = lhs, .rhs = rhs, 
                .body = seq.exprs, 
                .end_off = seq.roff, 
                .with = ident
            };
            return self.create(Expr {.off = tok.off, .data = .{.@"for" = @"for"}});
        },
        .@"if" => {
            self.lexer.consume();
            const cond = try self.parse_expr() orelse {
                self.lexer.report_note(tok.off, "expect expr after `if`", .{});
                return Error.UnexpectedToken;
            };

            const then = try self.expect_token_crit_off(.then, cond.off, "expr");
            const then_expr = try self.parse_expr() orelse {
                self.lexer.report_note(then.off, "expect expr after `then`", .{});
                return Error.UnexpectedToken;
            };
            const @"else" = try self.expect_token_crit_off(.@"else", then_expr.off, "expr");
            const else_expr = try self.parse_expr() orelse {
                self.lexer.report_note(@"else".off, "expect expr after `else`", .{});
                return Error.UnexpectedToken;
            };
            return self.create(Expr {.off = tok.off, .data = .{.@"if" = .{.cond = cond, .then = then_expr, .@"else" = else_expr}}});

        },
        else => return null,
    } 
}

fn prefix_bp(tt: TokenType) ?u32 {
    return switch (tt) {
        .slash => 30,
        .power,
        .period,
        .hash,
        .tilde => 40,
        else => null,     
    };
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

pub fn parse_expr_climb(self: *Parser, min_bp: u32) ErrorBoth!?*Expr {
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
                self.lexer.report_err_line(tk.off, "Expect expression after `{s}`", .{self.lexer.stringify_token(tk)});
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
        self.lexer.report_err_line(off, "Expect {s} after `{s}`, but encounter {}", .{ @tagName(kind), before, e });
        return Error.EndOfStream;
    };
    if (tok.tag != kind) {
        self.lexer.report_err_line(tok.off, "Expect {s} after `{s}`, found {s}", .{ @tagName(kind), before, self.lexer.stringify_token(tok) });
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
