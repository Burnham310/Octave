const std = @import("std");
const Allocator = std.mem.Allocator;

const InternPool = @import("intern_pool.zig");
const Lexer = @import("lexer.zig");
const Parser = @import("parser.zig");
const TypePool = @import("type_pool.zig");
const Sema = @import("sema.zig");

const Eval = @import("evaluator.zig");
const Player = @import("player.zig");

const Zynth = @import("zynth");


fn usage(prog_name: []const u8) void {
    std.debug.print("Usage: {s} <input_file> -o <output_file>\n", .{prog_name});
}

fn str_eql(a: []const u8, b: []const u8) bool {
    return std.mem.eql(u8, a, b);
}

const CliError = error {
    ExpectMoreArg,
    DuplicateArg,
    InvalidOption,
};

const ErrorReturnCode = enum {
    cli,
    lex,
    parse,
    sema,
    eval,
};

const CompileStage = enum {
    lex,
    parse,
    sema,
    play,
};

const ArgParser = struct {
    a: std.mem.Allocator,
    prefix_args: std.StringHashMapUnmanaged(Parse) = .{},
    postional_args: std.ArrayListUnmanaged(Parse) = .{},

    positional_ct: u32 = 0,

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
        
        pure_flag: bool = false,
    };

    const EnumContext = struct {
        fields: []std.builtin.Type.EnumField,
    };

    const ParseFn = fn ([]const u8, *anyopaque) bool;

    pub fn add_opt(self: *ArgParser, comptime T: type, ref: *T, default: ?*const T, arg_ty: ArgType, meta_var_name: []const u8) void {
        const d: ?Default = if (default) |d| Default{.ptr = @ptrCast(d), .size = @sizeOf(T)} else null;
        const info = @typeInfo(T);
        const p = switch(info) {
            .float => Parse {.f = parse_f32, .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name},
            .@"enum" => Parse {.f = gen_parse_enum(T), .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name},
            .bool => Parse {.f = undefined, .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name, .pure_flag = true},
            else => 
                if (T == []const u8)
                    Parse {.f = parse_str, .ptr = @ptrCast(ref), .default = d, .meta_var_name = meta_var_name}
                else
                    @compileError("Unsupported opt type " ++ @typeName(T)),
        };
        switch (arg_ty) {
            .prefix => |prefix| 
                self.prefix_args.put(self.a, prefix, p) catch unreachable,
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
                    if (str_eql(field.name, raw_arg)) {
                        @as(*T, @alignCast(@ptrCast(ptr))).* = @enumFromInt(i);
                        return true;
                    }
                }
                return false;
            }
        }.f;
    }

    pub fn parse(self: *ArgParser, args: *std.process.ArgIterator) !void {
        while (args.next()) |raw_arg| {
            if (self.prefix_args.getPtr(raw_arg)) |p| {
                
                if (p.occurence > 0) {
                    std.log.err("option `{s} {s}` already occured", .{raw_arg, p.meta_var_name});
                    return CliError.DuplicateArg;
                }
                p.occurence += 1;
                if (p.pure_flag) {
                    @as(*bool, @alignCast(@ptrCast(p.ptr))).* = true;
                    continue;
                }
                const next = args.next() orelse {
                    std.log.err("expect `{s}` after `{s}`", .{p.meta_var_name, raw_arg});
                    return CliError.ExpectMoreArg;
                };
                if (!p.f(next, p.ptr)) {
                    return CliError.InvalidOption;
                }
                continue;
            }
            if (self.positional_ct >= self.postional_args.items.len) {
                std.log.err("too many positional argument", .{});
                return CliError.InvalidOption;
            }
            const p = &self.postional_args.items[self.positional_ct];
            p.occurence += 1;
            self.positional_ct += 1;
            if (!p.f(raw_arg, p.ptr)) {
                return CliError.InvalidOption;
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
                    return CliError.ExpectMoreArg;
                }
            }
        }
        for (self.postional_args.items) |p| {
            if (p.occurence == 2) {
                if (p.default) |default| {
                    const bytes_dst: []u8 = @as([*]u8, @ptrCast(p.ptr))[0..default.size];
                    const bytes_src: []const u8 = @as([*]const u8, @ptrCast(default.ptr))[0..default.size];
                    @memcpy(bytes_dst, bytes_src);
                } else {
                    std.log.err("missing positional argument `{s}`", .{p.meta_var_name});
                    return CliError.ExpectMoreArg;
                }
            }
        }
    }
};

const Options = struct {
    input_path: []const u8,
    volume: f32,
    repeat: bool,
    compile_stage: CompileStage,
};

var debug_dump_trace = false;

pub fn exit_or_dump_trace(exit_code: ErrorReturnCode, e: anyerror) noreturn {
    //const builtin = @import("builtin");
    if (debug_dump_trace) std.process.exit(@intFromEnum(exit_code));
    std.log.err("{}", .{e});
    unreachable;
}

pub fn main() !void  {
    const alloc = std.heap.c_allocator;
    var args = try std.process.argsWithAllocator(alloc);
    defer args.deinit(); 

    const program_name = args.next().?;
    _ = program_name;
    var opts: Options = undefined;
    var args_parser = ArgParser {.a = alloc}; 
    args_parser.add_opt([]const u8, &opts.input_path, null, .positional, "<input-path>");
    //args_parser.add_opt([]const u8, &opts.output_path, &"-", .{.prefix = "-o"}, "<output-path>");
    args_parser.add_opt(CompileStage, &opts.compile_stage, &CompileStage.play, .{.prefix = "--stage"}, "<compile-stage>");
    args_parser.add_opt(bool, &opts.repeat, &false, .{.prefix = "--repeat"}, "");
    args_parser.add_opt(bool, &debug_dump_trace, &true, .{.prefix = "--debug"}, "");
    args_parser.add_opt(f32, &opts.volume, &1.0, .{.prefix = "--volume"}, "<volume>");
    
    args_parser.parse(&args) catch |e| exit_or_dump_trace(.cli, e);

    const stdout_raw = std.io.getStdOut();
    const stdout_writer = stdout_raw.writer();
    //var stdout_buffered = std.io.bufferedWriter(stdout_reader);
    //const stdout = stdout_buffered.writer();
    //defer stdout_buffered.flush() catch unreachable;

    const cwd = std.fs.cwd();
    const input_f = try cwd.openFile(opts.input_path, .{.mode = .read_only });
    defer input_f.close();
    InternPool.init_global_string_pool(alloc);
    // ----- Lexing -----
    const buf = try input_f.readToEndAlloc(alloc, 1024 * 1024);
    var lexer = Lexer.init(buf, opts.input_path, alloc);
    if (opts.compile_stage == .lex) {
        while (true) {
            const tk = try lexer.next();
            if (tk.tag == .eof) break;
            const loc = lexer.to_loc(tk.off);
            try stdout_writer.print("{s} {}:{}\n", .{lexer.stringify_token(tk), loc.row, loc.col});
        }
        return;
    }
    // ----- Parsing -----
    var parser = Parser { .lexer = &lexer, .a = alloc };
    var ast = parser.parse() catch |e| exit_or_dump_trace(.parse, e);
    if (opts.compile_stage == .parse) {
        ast.dump(stdout_writer, lexer);
        return;
    }
    // ----- Sema -----
    TypePool.init(alloc);
    var sema = Sema {.lexer = &lexer, .ast = &ast, .a = alloc };
    var anno = sema.sema() catch |e| exit_or_dump_trace(.sema, e);
    defer anno.deinit(alloc);
    if (opts.compile_stage == .sema) {
        std.log.info("type check successful", .{});
        return;
    }
    // ----- Compile -----
    if (opts.repeat) @panic("TODO");
    var eval = Eval.Evaluator.init(&ast, &anno, alloc);
    eval.start();

    var player = Player {.evaluator = &eval, .a = alloc, .volume = opts.volume };
    var silence = Zynth.Waveform.Simple.silence;
    var cutoff = Zynth.Envelop.SimpleCutoff {.cutoff_sec = 0.5, .sub_stream = silence.streamer()};
    var and_then = Zynth.Delay.AndThen {.lhs = player.streamer(), .rhs = cutoff.streamer()};

    var audio_ctx = Zynth.Audio.SimpleAudioCtx {};
    try audio_ctx.init(and_then.streamer());
    try audio_ctx.start();

    
    audio_ctx.drain();
}

