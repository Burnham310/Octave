const std = @import("std");

const Lexer = @import("lexer.zig");
const Parser = @import("parser.zig");
const Sema = @import("sema.zig");


pub const Error = error {
    ExpectMoreArg,
    DuplicateArg,
    InvalidOption,
};

pub const ArgParser = struct {
    a: std.mem.Allocator,
    prefix_args: std.StringHashMapUnmanaged(Parse) = .{},
    postional_args: std.ArrayListUnmanaged(Parse) = .{},

    positional_ct: u32 = 0,

    pgm_name: []const u8,
    overview: []const u8 = "",

    pub const ArgType = union(enum) {
        positional,
        prefix: []const u8,
    };

    const Default = struct {
        ptr: *const anyopaque,
        size: usize,
    };

    pub const Parse = struct {
        f: *const ParseFn,
        ptr: *anyopaque,
        occurence: u32 = 0,
        default: ?Default,
        meta_var_name: []const u8,
        desc: []const u8,
        
        pure_flag: bool = false,
    };

    const EnumContext = struct {
        fields: []std.builtin.Type.EnumField,
    };

    const ParseFn = fn ([]const u8, *anyopaque) bool;

    pub fn add_opt(
        self: *ArgParser,
        comptime T: type, ref: *T,
        default: ?*const T,
        arg_ty: ArgType,
        meta_var_name: []const u8,
        desc: []const u8) void {
        const d: ?Default = if (default) |d| Default{.ptr = @ptrCast(d), .size = @sizeOf(T)} else null;
        const info = @typeInfo(T);
        const p = switch(info) {
            .float => Parse {.f = parse_f32, .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name, .desc = desc},
            .@"enum" => Parse {.f = gen_parse_enum(T), .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name, .desc = desc},
            .bool => Parse {.f = undefined, .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name, .pure_flag = true, .desc = desc},
            else => 
                if (T == []const u8)
                    Parse {.f = parse_str, .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name, .desc = desc}
                else
                    @compileError("Unsupported opt type " ++ @typeName(T)),
        };
        switch (arg_ty) {
            .prefix => |prefix| 
                if (self.prefix_args.fetchPut(self.a, prefix, p) catch unreachable) |_| {
                    std.process.fatal("prefix arg `{s} {s}`already exists", .{prefix, p.meta_var_name});
                },
            .positional => self.postional_args.append(self.a, p) catch unreachable,
        }
    }

    fn parse_str(raw_arg: []const u8, ptr: *anyopaque) bool {
        @as(*[]const u8, @alignCast(@ptrCast(ptr))).* = raw_arg;
        return true;
    }

    fn parse_f32(raw_arg: []const u8, ptr: *anyopaque) bool {
        @as(*f32, @alignCast(@ptrCast(ptr))).* = std.fmt.parseFloat(f32, raw_arg) catch |e| {
            std.log.err("expect float, got {}", .{e});
            return false;
        };
        return true;
    }

    fn gen_parse_enum(comptime T: type) *const ParseFn {
        return struct {
            pub fn f(raw_arg: []const u8, ptr: *anyopaque) bool {
                const fields = @typeInfo(T).@"enum".fields;
                inline for (fields, 0..) |field, i| {
                    if (std.mem.eql(u8, field.name, raw_arg)) {
                        @as(*T, @alignCast(@ptrCast(ptr))).* = @enumFromInt(i);
                        return true;
                    }
                }
                return false;
            }
        }.f;
    }

    pub fn print_help(self: *ArgParser) void {
        const print = std.debug.print;
        print("usage: {s}", .{self.pgm_name});
        for (self.postional_args.items) |positional| {
            print(" {s}", .{positional.meta_var_name});
        }
        print("\n", .{});
        for (self.postional_args.items) |positional| {
            print("\t{s}\t{s}\n", .{positional.meta_var_name, positional.desc});
        }
        var it = self.prefix_args.iterator();
        while (it.next()) |entry| {
            const p = entry.value_ptr;
            print("\t{s} {s}\t{s}\n", .{entry.key_ptr.*, p.meta_var_name, p.desc});
        }
        print("\t--help\tprint this help message\n", .{});
    }

    pub fn parse(self: *ArgParser, args: *std.process.ArgIterator) !void {
        while (args.next()) |raw_arg| {
            if (self.prefix_args.getPtr(raw_arg)) |p| {

                if (p.occurence > 0) {
                    std.log.err("option `{s} {s}` already occured", .{raw_arg, p.meta_var_name});
                    return Error.DuplicateArg;
                }
                p.occurence += 1;
                if (p.pure_flag) {
                    @as(*bool, @alignCast(@ptrCast(p.ptr))).* = true;
                    continue;
                }
                const next = args.next() orelse {
                    std.log.err("expect `{s}` after `{s}`", .{p.meta_var_name, raw_arg});
                    return Error.ExpectMoreArg;
                };
                if (!p.f(next, p.ptr)) {
                    return Error.InvalidOption;
                }
                continue;
            }
            if (std.mem.eql(u8, raw_arg, "--help")) {
                self.print_help();
                std.process.exit(0);
            }
            if (self.positional_ct >= self.postional_args.items.len) {
                std.log.err("too many positional argument", .{});
                return Error.InvalidOption;
            }
            const p = &self.postional_args.items[self.positional_ct];
            p.occurence += 1;
            self.positional_ct += 1;
            if (!p.f(raw_arg, p.ptr)) {
                return Error.InvalidOption;
            }
        }     
        var map_it = self.prefix_args.iterator();
        while (map_it.next()) |kv| {
            if (kv.value_ptr.occurence == 0) {
                if (kv.value_ptr.default) |default| {
                    const bytes_dst: []u8 = @as([*]u8, @ptrCast(kv.value_ptr.ptr))[0..default.size];
                    const bytes_src: []const u8 = @as([*]const u8, @ptrCast(default.ptr))[0..default.size];
                    @memcpy(bytes_dst, bytes_src);
                } else {
                    std.log.err("missing option: `{s} {s}`", .{kv.key_ptr.*, kv.value_ptr.meta_var_name});
                    return Error.ExpectMoreArg;
                }
            }
        }
        for (self.postional_args.items) |p| {
            if (p.occurence == 0) {
                if (p.default) |default| {
                    const bytes_dst: []u8 = @as([*]u8, @ptrCast(p.ptr))[0..default.size];
                    const bytes_src: []const u8 = @as([*]const u8, @ptrCast(default.ptr))[0..default.size];
                    @memcpy(bytes_dst, bytes_src);
                } else {
                    std.log.err("missing positional argument `{s}`", .{p.meta_var_name});
                    return Error.ExpectMoreArg;
                }
            }
        }
    }
};

pub const ErrorReturnCode = enum(u8) {
    success = 0,
    cli,
    lex,
    parse,
    sema,
    eval,
    unexpected,

    pub fn from_err(e: anyerror) ErrorReturnCode {
        if (is_err_from_set(Error, e)) return .cli;
        if (is_err_from_set(Lexer.Error, e)) return .lex;
        if (is_err_from_set(Parser.Error, e)) return .parse;
        if (is_err_from_set(Sema.Error, e)) return .sema;
        //if (is_err_from_set(e, Eval.Error)) return .lex;S
        return .unexpected;
    }

    pub fn is_err_from_set(comptime T: type, e: anyerror) bool {
        const err_info = @typeInfo(T).error_set.?;
        return inline for (err_info) |err| {
            if (@field(T, err.name) == e) break true;
        } else false;
    }
};


pub const CompileStage = enum {
    lex,
    parse,
    sema,
    play,
};

fn enum_desc_len(comptime E: type) usize {
    const fields = @typeInfo(E).@"enum".fields;
    if (fields.len == 0) return 0;
    var len: usize = 0;
    inline for (fields) |field| {
        len += field.name.len;
    }
    len += fields.len - 1;
    return len;
}

pub fn enum_desc(comptime E: type) []const u8 {
    const fields = @typeInfo(E).@"enum".fields;
    if (fields.len == 0) return "";
    var desc: []const u8 = fields[0].name;
    inline for (fields[1..]) |field| {
        desc = desc ++ "|";
        desc = desc ++ field.name;
    }
    return desc;

}
