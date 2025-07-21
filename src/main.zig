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

const Error = error {
    InvalidCliArg,
    SyntaxError,
    TypeMismatch,
};

const CompileStage = enum {
    Lexing,
    Parsing,
    Sema,
    Compiling,

    pub fn parse(str: []const u8) ?CompileStage {
        const fields = @typeInfo(CompileStage).@"enum".fields;
        inline for (fields, 0..) |field, i| {
            if (str_eql(field.name, str)) return @enumFromInt(i);
        }
        return null;
    }
    
    pub fn after(self: CompileStage, target: CompileStage) bool {
        return @intFromEnum(self) >= @intFromEnum(target);
    }
};

const Options = struct {
    input_paths: [][:0]const u8 = &.{},
    output_path: [:0]const u8 = "",
    
    compile_stage: CompileStage = .Compiling,
    
    pub fn init(args: *std.process.ArgIterator, a: std.mem.Allocator) !Options {
        var input_paths = std.ArrayList([:0]const u8).init(a); 
        var output_path: ?[:0]const u8 = null;
        var compile_stage: ?CompileStage = null;

        while (args.next()) |arg| {
            if (str_eql(arg, "-o")) {
                if (output_path) |output_path_inner| {
                    std.log.err("<output-path> is already set previously `{s}`; only one <output-path> is allowed", .{output_path_inner});
                    return Error.InvalidCliArg;
                }
                output_path = args.next() orelse {
                    std.log.err("Expects <output-path> after `-o`", .{});
                    return Error.InvalidCliArg;
                };
            } else if (std.mem.startsWith(u8, arg, "--stage")) {
                if (compile_stage) |stage| {
                    std.log.err("<stage> is already set previously `{}`; only one <stage> is allowed", .{stage});
                    return Error.InvalidCliArg;
                }
                const stage_str = args.next() orelse {
                    std.log.err("Expects <stage> after `--stage`", .{});
                    return Error.InvalidCliArg;
                };
                compile_stage = CompileStage.parse(stage_str) orelse {
                    std.log.err("Expects <stage> after `--stage`, found `{s}`", .{stage_str});
                    std.log.info("<stage> should be one of Lexing|Parsing|Sema|Compiling", .{});
                    return Error.InvalidCliArg;
                };
            } else {
                try input_paths.append(arg);
            }
        }

        if (input_paths.items.len == 0) {
            std.log.err("No <input-path> found, at least is needed", .{});
            return Error.InvalidCliArg;
        }
        if (input_paths.items.len > 1) {
            std.log.err("Only 1 <input-path> is supported for now, got {}", .{input_paths.items.len});
            return Error.InvalidCliArg;
        }

        return Options {
            .input_paths = try input_paths.toOwnedSlice(),
            .output_path = output_path orelse blk: {
                std.log.info("<output-path> not set, defaults to `stdout`", .{});
                break :blk "-";
            },
            .compile_stage = compile_stage orelse CompileStage.Compiling,
        };
    }
};

pub fn main() !void  {
    const alloc = std.heap.c_allocator;
    var args = try std.process.argsWithAllocator(alloc);
    defer args.deinit();

    const program_name = args.next().?;
    _ = program_name;
    const opts = try Options.init(&args, alloc); 

    const stdout_raw = std.io.getStdOut();
    //var stdout_buffered = std.io.bufferedWriter(stdout_reader);
    //const stdout = stdout_buffered.writer();
    //defer stdout_buffered.flush() catch unreachable;

    const cwd = std.fs.cwd();
    const input_f = try cwd.openFileZ(opts.input_paths[0], .{.mode = .read_only });
    defer input_f.close();
    const output_f = if (std.mem.eql(u8, opts.output_path, "-")) 
        stdout_raw else try cwd.createFileZ(opts.output_path, .{});
    defer output_f.close();
    const output_writer = output_f.writer();

    std.log.info("Invoking {s} {s} into {s}", .{@tagName(opts.compile_stage), opts.input_paths[0], opts.output_path});

    InternPool.init_global_string_pool(alloc);
    // ----- Lexing -----
    const buf = try input_f.readToEndAlloc(alloc, 1024 * 1024);
    var lexer = Lexer.init(buf, opts.input_paths[0], alloc);
    if (opts.compile_stage == .Lexing) {
        while (true) {
            const tk = try lexer.next();
            if (tk.tag == .eof) break;
            const loc = lexer.to_loc(tk.off);
            try output_writer.print("{s} {}:{}\n", .{lexer.stringify_token(tk), loc.row, loc.col});
        }
        return;
    }
    // ----- Parsing -----
    var parser = Parser { .lexer = &lexer, .a = alloc };
    var ast = parser.parse() catch {
        return;
    };
    if (opts.compile_stage == .Parsing) {
        ast.dump(output_writer, lexer);
        return;
    }
    // ----- Sema -----
    TypePool.init(alloc);
    var sema = Sema {.lexer = &lexer, .ast = &ast, .a = alloc };
    var anno = try sema.sema();
    defer anno.deinit(alloc);
    if (opts.compile_stage == .Sema) {
        std.log.info("type check successful", .{});
        return;
    }
    // ----- Compile -----
    var eval = Eval.Evaluator.init(&ast, &anno, alloc);
    eval.start();

    var player = Player {.evaluator = &eval, .a = alloc };
    var silence = Zynth.Waveform.Simple.silence;
    var cutoff = Zynth.Envelop.SimpleCutoff {.cutoff_sec = 0.5, .sub_stream = silence.streamer()};
    var and_then = Zynth.Delay.AndThen {.lhs = player.streamer(), .rhs = cutoff.streamer()};

    var audio_ctx = Zynth.Audio.SimpleAudioCtx {};
    try audio_ctx.init(and_then.streamer());
    try audio_ctx.start();

    
    audio_ctx.drain();
}

