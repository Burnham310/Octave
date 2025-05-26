const std = @import("std");
const Allocator = std.mem.Allocator;
const InternPool = @import("intern_pool.zig");
const Lexer = @This();

const Token = struct {
    tag: TokenType,
    off: u32,
};
pub const Loc = struct {
    row: u32,
    col: u32,
    path: []const u8,
    pub fn format(value: Loc, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        return writer.print("{s}:{}:{}", .{ value.path, value.row, value.col });
    }
};  
const TokenType = enum {
    ident,
    int,
    dots,
    qual,
    true,
    false,
    eq,
    neq,
    geq,
    leq,
    @"if",
    then,
    @"else",
    @"for",
    loop,
    end,
    @"void",

    // single character
    lbrac,
    rbrac,
    comma,
    bar,
    eq,
    colon,
    semi_colon,
    slash,
    le,
    ge,
    ampersand,
    lparen,
    rparen,
    single_quote,
    tilde,
    dollar,
    plus,
    minus,
    times,
    at,
};

src: []const u8,
off: u32,
path: []const u8, // for error reporting
peak: ?TokenType,
string_pool: InternPool.StringInternPool,

pub fn init(src: []const u8, path: []const u8, a: Allocator) Lexer {
    return .{
        .src = src,
        .off = 0,
        .path = path,
        .peak = null,
        .string_pool = InternPool.StringInternPool.init(a),
    };
}
pub fn deinit(self: *Lexer) void {
    self.string_pool.deinit();
}
pub fn to_loc(lexer: Lexer, off: u32) Loc {
    var i: u32 = 0;
    var res = Loc {.row = 1, .col = 1, .path = lexer.path};
    while (i < off): (i += 1) {
        const c = lexer.src[i];
        switch (c) {
            '\n', '\r' => {
                res.row += 1;
                res.col = 1;
            },
            else => res.col += 1,
        }
    }
    return res;
}

fn skip_ws(self: *Lexer) void {
    while (self.off < self.src.len) : (self.off += 1) {
        self.skipComment();
        const c = self.src[self.off];
        if (!std.ascii.isWhitespace(c)) {
            break;
        }
    }
}
fn skip_comment(self: *Lexer) void {
    if (self.off < self.src.len - 1 and self.src[self.off] == '/' and self.src[self.off + 1] == '/') {
        //log.err("comment", .{});
        while (self.off < self.src.len): (self.off += 1) {
            if (self.src[self.off] == '\n') {
                self.off += 1;
                //log.err("comment break {c}", .{self.src[self.off]});
                break;
            }
        }
        // runs out of character
    }
}
fn next_char(self: *Lexer) ?u8 {
    if (self.off >= self.src.len) return null;
    defer {
        self.off += 1;
    }
    return self.src[self.off];
}

fn rewindChar2(self: *Lexer) void {
    self.off -= 2;
}
fn match_single(self: *Lexer) ?Token {
    return Token {
        .tag = switch (self.next_char() orelse return null) {
            '[' => .lbrac,
            ']' => .rbrac,
            ',' => .comma,
            '|' => .bar,
            '=' => .eq,
            ':' => .colon,
            ';' => .semi_colon,
            '/' => .slash,
            '<' => .le,
            '>' => .ge,
            '&' => .ampersand,
            '(' => .lparen,
            ')' => .rparen,
            '\'' => .single_quote,
            '~' => .tilde,
            '$' => .dollar,
            '+' => .plus,
            '-' => .minus,
            '*' => .times,
            '@' => .at,
            else => {
                self.off -= 1;
                return null;
            },
        },
        .off = self.off,
    };
}

fn match_qualifier(self: *Lexer) void {

}

