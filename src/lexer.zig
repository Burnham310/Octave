const std = @import("std");
const Allocator = std.mem.Allocator;
const InternPool = @import("intern_pool.zig");
pub const Symbol = InternPool.Symbol;
const Lexer = @This();

pub const Error = error {
    Unrecognized,
    InvalidNum,
};


pub const Token = struct {
    tag: TokenType,
    off: u32,
};

pub const Loc = struct {
    row: u32,
    col: u32,
    path: []const u8,
    pub fn format(value: Loc, writer: anytype) !void {
        return writer.print("{s}:{}:{}", .{ value.path, value.row, value.col });
    }
};

pub const TokenType = enum {
    ident,
    int,
    true,
    false,
    //neq,
    //geq,
    //leq,

    // keyword
    @"if",
    then,
    @"else",
    @"for",
    loop,
    end,
    @"void",

    // single character
    eq,
    lbrac,
    rbrac,
    comma,
    bar,
    colon,
    semi_colon,
    slash,
    lcurly,
    rcurly,

    hash,
    tilde,
    power,
    period,
    //le,
    //ge,
    //ampersand,
    lparen,
    rparen,
    single_quote,
    plus,
    minus,
    times,

    eof,

    //pub fn format(
    //    self: TokenType,
    //    comptime fmt: []const u8,
    //    options: std.fmt.FormatOptions,
    //    writer: anytype) !void {

    //    _ = fmt;
    //    _ = options;

    //    switch (self) {
    //        // single character
    //        .eq => _ = try writer.write("="),
    //        .lbrac => _ = try writer.write("["),
    //        .rbrac => _ = try writer.write("]"),
    //        .comma => _ = try writer.write(","),
    //        .bar => _ = try writer.write("|"),
    //        .colon => _ = try writer.write(":"),
    //        .semi_colon => _ = try writer.write(";"),
    //        .slash => _ = try writer.write("/"),
    //        .lparen => _ = try writer.write("("),
    //        .rparen => _ = try writer.write(")"),
    //        .lcurly => _ = try writer.write("{"),
    //        .rcurly => _ = try writer.write("}"),
    //        .single_quote => _ = try writer.write("'"),
    //        .plus => _ = try writer.write("+"),
    //        .minus => _ = try writer.write("-"),
    //        .times => _ = try writer.write("*"),
    //        else => _ = try writer.write(@tagName(self)),
    //    }
    //}
};

const single_char_tokens = [_]struct {TokenType, u8} {
    .{.lbrac, '['},
    .{.rbrac, ']'},
    .{.comma, ','},
    .{.bar, '|'},
    .{.eq, '='},
    .{.colon, ':'},
    .{.semi_colon, ';'},
    .{.slash, '/'},
    //.{.le, '<'},
    //.{.ge, '>'},
    //.{.ampersand, '&'},
    .{.lcurly, '{'},
    .{.rcurly, '}'},
    .{.lparen, '('},
    .{.rparen, ')'},
    .{.single_quote, '\''},

    .{.hash, '#'},
    .{.tilde, '~'},
    .{.power, '^'},
    .{.period, '.'},
    //.{.dollar, '$'},
    .{.plus, '+'},
    .{.minus, '-'},
    .{.times, '*'},
};

src: []const u8,
off: u32,
path: []const u8, // for error reporting
peek_buf: ?Token,
string_pool: *InternPool.StringInternPool = &InternPool.string_pool,
token_char_map: std.AutoHashMapUnmanaged(TokenType, u8),
char_token_map: std.AutoHashMapUnmanaged(u8, TokenType),


pub const BuiltinSymbols = struct {
    pub var main: Symbol = undefined;
    pub var tonic: Symbol = undefined;
    pub var mode: Symbol = undefined;
    pub var octave: Symbol = undefined;
    pub var bpm: Symbol = undefined;
    pub var tempo: Symbol = undefined;
    pub var instrument: Symbol = undefined;
    pub var volume: Symbol = undefined;

    pub var C: Symbol = undefined;
    pub var D: Symbol = undefined;
    pub var E: Symbol = undefined;
    pub var F: Symbol = undefined;
    pub var G: Symbol = undefined;
    pub var A: Symbol = undefined;
    pub var B: Symbol = undefined;

    pub var major: Symbol = undefined;
    pub var minor: Symbol = undefined;

    pub fn init(string_pool: *InternPool.StringInternPool) void {
        const Self = @This();
        const decls = @typeInfo(Self).@"struct".decls;

        inline for (decls) |decl| {
            if (comptime std.mem.eql(u8, decl.name, "init")) continue; 
            @field(Self, decl.name) = string_pool.intern(decl.name);
        }
    }
};

pub fn init(src: []const u8, path: []const u8, a: std.mem.Allocator) Lexer {
    BuiltinSymbols.init(&InternPool.string_pool);
    var lexer = Lexer {
        .src = src,
        .off = 0,
        .path = path,
        .peek_buf = null,
        .string_pool = &InternPool.string_pool,
        .token_char_map = std.AutoHashMapUnmanaged(TokenType, u8) {},
        .char_token_map = std.AutoHashMapUnmanaged(u8, TokenType) {},
    };
    for (single_char_tokens) |tk_char| {
        lexer.token_char_map.putNoClobber(a, tk_char[0], tk_char[1]) catch unreachable;
        lexer.char_token_map.putNoClobber(a, tk_char[1], tk_char[0]) catch unreachable;
    }
    return lexer;
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
            '\n' => {
                res.row += 1;
                res.col = 1;
            },
            else => res.col += 1,
        }
    }
    return res;
}

fn skip_ws(self: *Lexer) bool {
    var skipped_any = false;
    while (self.off < self.src.len) : (self.off += 1) {
        const c = self.src[self.off];
        if (!std.ascii.isWhitespace(c)) {
            break;
        }
        skipped_any = true;
    }
    return skipped_any;
}
fn skip_comment(self: *Lexer) bool {
    if (self.off < self.src.len - 1 and self.src[self.off] == '/' and self.src[self.off + 1] == '/') {
        //log.err("comment", .{});
        while (self.off < self.src.len): (self.off += 1) {
            if (self.src[self.off] == '\n') {
                self.off += 1;
                //log.err("comment break {c}", .{self.src[self.off]});
                return true;
            }
        }
        return false;
    }
    return false;
}
fn next_char(self: *Lexer) ?u8 {
    if (self.off >= self.src.len) return null;
    defer {
        self.off += 1;
    }
    return self.src[self.off];
}

fn rewind_char(self: *Lexer) void {
    self.off -= 1;
}

fn rewind_char2(self: *Lexer) void {
    self.off -= 2;
}

fn match_single(self: *Lexer) ?Token {
    return Token {
        .tag = self.char_token_map.get(self.next_char() orelse return null) orelse {
            self.off -= 1;
            return null;
        },
        .off = self.off - 1,
    };
}

fn match_ident(self: *Lexer) ?Token {
    const keyword_map = std.StaticStringMap(TokenType).initComptime(&.{
        .{"if", .@"if"},
        .{"then", .then},
        .{"for", .@"for"},
        .{"loop", .loop},
        .{"end", .end},
    });
    const off = self.off;
    const first = self.next_char() orelse return null;
    if (!std.ascii.isAlphabetic(first) and first != '_') return null;

    while (self.next_char()) |c| {
        if (std.ascii.isAlphanumeric(c) or c == '_') continue;
        self.rewind_char();
        break;
    }
    const str = self.src[off..self.off];

    return Token { .tag = keyword_map.get(str) orelse .ident, .off = off };
}

pub fn match_num(self: *Lexer) Error!?Token {
    const off = self.off;
    const first = self.next_char() orelse return null;
    // var have_sign = false;
    // if (first == '-' or first == '+') {
    //     first = self.next_char() orelse {
    //         self.off -= 1;
    //         return null;
    //     };
    //     have_sign = true;
    // }
    if (!std.ascii.isDigit(first)) { // make sure at least one digit
        self.rewind_char();
        return null;
    }
    //var dot = false;
    while (self.next_char()) |c| {
        // TODO error if not space or digit
        switch (c) {
            'a'...'z', 'A'...'Z' => return Error.InvalidNum,

            '0'...'9' => {},
            // '.' => {
            //     if (dot) {
            //         log.err("{} Mulitple `.` in number literal", .{self.to_loc(off)});
            //         return LexerError.InvalidNum;
            //     } else {
            //         dot = true;
            //     }
            // },
            else => break,
        }
    }
    defer self.off -= 1;
    return Token { .tag = .int, .off = off };
}

pub fn skip_to_new_line_star(self: *Lexer) void {
    while (self.off < self.src.len): (self.off += 1) {
        if (self.src[self.off] == '\n') {
            self.off += 1;
            break;
        }
    }
}


pub fn next(self: *Lexer) Error!Token {
    if (self.peek_buf) |peek_buf| {
        self.peek_buf = null;
        return peek_buf;
    }
    while (self.skip_ws() or self.skip_comment()) {}
    if (self.off >= self.src.len) return Token {.tag = .eof, .off = @intCast(self.src.len-1)};

    const tk = try self.match_num() orelse self.match_single() orelse self.match_ident() orelse {
        self.report_err(self.off, "unrecognized token: `{c}`", .{self.src[self.off]});
        self.report_line(self.off);
        return Error.Unrecognized;
    };

    return tk;
}

pub fn peek(self: *Lexer) Error!Token {
    if (self.peek_buf) |peek_buf| return peek_buf;
    self.peek_buf = try self.next();
    return self.peek_buf.?;
}

pub fn consume(self: *Lexer) void {
    _ = self.next() catch unreachable;
}

// ---------- re_* Function Familiy ----------
// Most of the token does not carry additional data, and the tag alone is enough.
// Since Token only stores offset and type, we need to re-ify the token when needed.
pub fn re_int(self: Lexer, off: u32) isize {
    return std.fmt.parseInt(isize, self.re_int_impl(off), 10) catch unreachable; 
}

pub fn re_int_impl(self: Lexer, off: u32) []const u8 {
    var i = off + 1;
    while (i < self.src.len): (i += 1) {
        // TODO error if not space or digit
        switch (self.src[i]) {
            '0'...'9' => {},
            'a'...'z', 'A'...'Z' => unreachable,
            else => break,
        }
    }
    return self.src[off .. i]; 
}

pub fn re_ident(self: Lexer, off: u32) Symbol {
    return self.intern(self.re_ident_impl(off));
}

pub fn re_ident_impl(self: Lexer, off: u32) []const u8 {
    switch (self.src[off]) {
        'A'...'Z', 'a'...'z', '_' => {},
        else => {},
    }
    var i: u32 = off + 1;
    while (i < self.src.len): (i += 1) {
        switch (self.src[i]) {
            'A'...'Z', 'a'...'z', '_', '0'...'9' => {},
            else => break,
        }
    }
    return self.src[off .. i];
}

pub fn stringify_token(self: Lexer, tk: Token) []const u8 {
    if (self.token_char_map.get(tk.tag)) |char| {
        return &.{char};
    }
    switch (tk.tag) {
        .ident => return self.re_ident_impl(tk.off),
        else => _ = return @tagName(tk.tag),
    }
}

// short cut to parser.lexer.lookup
pub fn lookup(self: Lexer, sym: Symbol) []const u8 {
    return self.string_pool.lookup(sym);
}

// short cut to parser.lexer.intern
pub fn intern(self: Lexer, str: []const u8) Symbol {
    return self.string_pool.intern(str);
}

// ---------- Error Reporting Utilities ----------
pub fn report(self: Lexer, prefix: []const u8, off: u32, comptime fmt: []const u8, args: anytype) void {
    std.debug.print("{s}: {f} ", .{prefix, self.to_loc(off)});
    std.debug.print(fmt++"\n", args);
}

pub fn report_err(self: Lexer, off: u32, comptime fmt: []const u8, args: anytype) void {
    return self.report("error", off, fmt, args);
}

pub fn report_note(self: Lexer, off: u32, comptime fmt: []const u8, args: anytype) void {
    return self.report("note", off, fmt, args);
}

// print the line that the offset is in
pub fn report_line(self: Lexer, off: u32) void {
    var start: u32 = off;
    var end: u32 = off;

    while (start > 0): (start -= 1) {
        if (self.src[start] == '\n' and start != off) {
            start += 1;
            break;
        }
    }

    while (end < self.src.len): (end += 1) {
        if (self.src[end] == '\n') {
            break;
        } 
    }
    std.debug.print("\t{s}\n", .{self.src[start..end]});
    self.highligh_off(off-start);
}

pub fn highligh_off(self: Lexer, line_pos: u32) void {
    _ = self;
    std.debug.print("\t", .{});
    for (0..line_pos) |_| {
        std.debug.print(" ", .{});
    }
    std.debug.print("^\n", .{});
}
