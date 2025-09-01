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

const Cli = @import("cli.zig");


const Options = struct {
    input_path: []const u8,
    volume: f32,
    repeat: bool,
    compile_stage: Cli.CompileStage,
    debug: bool,
    json: bool,
};

var debug_dump_trace = false;

pub fn exit_or_dump_trace(e: anyerror) noreturn {
    //const builtin = @import("builtin");
    if (!debug_dump_trace) std.process.exit(@intFromEnum(Cli.ErrorReturnCode.from_err(e)));
    std.log.err("{}", .{e});
    unreachable;
}

pub const std_options = Zynth.std_options;

pub fn main() !void  {
    const alloc = std.heap.c_allocator;
    var args = try std.process.argsWithAllocator(alloc);
    defer args.deinit(); 

    const program_name = args.next().?;
    var opts: Options = undefined;
    var args_parser = Cli.ArgParser {}; 
    args_parser.init(alloc, program_name, "Compiler/Interpreter for the Octave Programming Language");
    args_parser
        .add_opt([]const u8, &opts.input_path, .none, .positional, "<input-path>",
            "the path to the input .oct file")
        //args_parser.add_opt([]const u8, &opts.output_path, &"-", .{.prefix = "-o"}, "<output-path>");
        .add_opt(
            Cli.CompileStage, &opts.compile_stage, .{.just = &Cli.CompileStage.play}, .{.prefix = "--stage"}, "<compile-stage>",
            "the stage of compilaton, must be one of " ++ comptime Cli.enum_desc(Cli.CompileStage))
        .add_opt(bool, &opts.repeat, .{.just = &false}, .{.prefix = "--repeat"}, "",
            "repeat indefinetly; only make sense in the playing stage")
        .add_opt(bool, &debug_dump_trace, .{.just = &false}, .{.prefix = "--debug"}, "",
            "dump trace in case of error, needs to be compiled with trace")
        .add_opt(f32, &opts.volume, .{.just = &1.0}, .{.prefix = "--volume"}, "<volume>",
            "a floating point number representing the master volume of the player")
        .add_opt(bool, &opts.debug, .{.just = &false}, .{.prefix = "-g"}, "",
            "print all the notes to stdout instead of playing them")
        .add_opt(bool, &opts.json, .{.just = &false}, .{.prefix = "--json"}, "",
            "when `stage` is play, output the notes as a json file instead of actually playing it")
        
        .parse(&args) catch |e| exit_or_dump_trace(e);

    const stdout_raw = std.fs.File.stdout();
    var stdout_buf: [256]u8 = undefined;
    var stdout_writer = stdout_raw.writer(&stdout_buf);
    const stdout = &stdout_writer.interface;
    defer stdout.flush() catch unreachable;
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
            const tk = lexer.next() catch |e| exit_or_dump_trace(e);
            if (tk.tag == .eof) break;
            const loc = lexer.to_loc(tk.off);
            try stdout.print("{s} {}:{}\n", .{lexer.stringify_token(tk), loc.row, loc.col});
        }
        return;
    }
    // ----- Parsing -----
    var parser = Parser { .lexer = &lexer, .a = alloc };
    var ast = parser.parse() catch |e| exit_or_dump_trace(e);
    if (opts.compile_stage == .parse) {
        ast.dump(stdout, lexer);
        return;
    }
    // ----- Sema -----
    TypePool.init(alloc);
    var sema = Sema {.lexer = &lexer, .ast = &ast, .a = alloc };
    var anno = sema.sema() catch |e| exit_or_dump_trace(e);
    defer anno.deinit(alloc);
    if (opts.compile_stage == .sema) {
        return;
    }
    // ----- Compile -----
    var eval = Eval.Evaluator.init(&ast, &anno, alloc);
    try stdout.writeAll("[\n");
    if (opts.debug) {
        while (true) {
            const note = eval.eval();
            try stdout.print("{f}\n", .{std.json.fmt(note, .{ .whitespace = .indent_2, })});
            if (note.is_eof()) {
                try stdout.writeAll("\n]");
                return;
            } else {
                try stdout.writeAll(",\n");
            }
        }
    }
    var player = Player { .evaluator = &eval, .a = alloc, .volume = opts.volume };

    var streamer: Zynth.Streamer = undefined;
    var silence = Zynth.Waveform.Simple.silence;
    var cutoff = Zynth.Envelop.SimpleCutoff {.cutoff_sec = 0.5, .sub_stream = silence.streamer()};
    var and_then = Zynth.Delay.AndThen {.lhs = player.streamer(), .rhs = cutoff.streamer()};
    var repeat = Zynth.Replay.RepeatAfterStop.init(null, player.streamer());
    if (opts.repeat) {
        streamer = repeat.streamer();            
    } else {
        streamer = and_then.streamer();
    }
    
    var audio_ctx = Zynth.Audio.SimpleAudioCtx {};
    defer audio_ctx.deinit();
    try audio_ctx.init(streamer);
    std.log.info("starting...", .{});
    try audio_ctx.start();

    
    audio_ctx.drain();
    std.log.info("done.", .{});
}


