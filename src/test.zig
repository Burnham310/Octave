const std = @import("std");
const Allocator = std.mem.Allocator;

const Cli = @import("cli.zig");
const CompileStage = Cli.CompileStage;
const ErrorReturnCode = Cli.ErrorReturnCode;

const TestResult = struct {
    path: []const u8,  
    return_code: ErrorReturnCode,
};
const TestResults = std.ArrayListUnmanaged(TestResult);
const fatal = std.process.fatal;

pub fn run_tests_on_dir(
    a: Allocator, 
    stage: CompileStage, path: []const u8, compiler_path: []const u8, 
    test_results: *TestResults) void {

    const dir = std.fs.cwd().openDir(path, .{ .iterate = true }) catch |e| fatal("{}: cannot open test direcotry `{s}`", .{e, path});
    var it = dir.iterate();
    while (it.next() 
        catch |e| fatal("{}: cannot iterate on `{s}`", .{e, path})
        ) |entry| {
        if (entry.kind != .file) continue;
        const full_path = std.fmt.allocPrint(a, "{s}/{s}", .{path, entry.name}) catch unreachable;
        var child = switch (stage) {
            .play => 
                std.process.Child.init(&.{compiler_path, "--stage", @tagName(stage), "-g", full_path}, a),
            else => 
                std.process.Child.init(&.{compiler_path, "--stage", @tagName(stage), full_path}, a),
        };
        // child.stdout_behavior = if (stage == .play) .Inherit else .Ignore;
        child.stdout_behavior = .Pipe;
        child.stderr_behavior = .Ignore;
        child.spawn() catch unreachable;
        if (child.stdout) |stdout| {
            std.log.debug("{s}", .{full_path});
            var buf: [256]u8 = undefined;
            var reader = stdout.reader(&buf);
            const result = reader.interface.readAlloc(a, 1024) catch unreachable;
            std.log.debug("stdout: {}", .{result.len});
            a.free(result);
        }
        const term = child.wait() catch {
            test_results.append(a, .{.path = full_path, .return_code = .unexpected}) catch unreachable;
            continue;
        };
        const return_code = switch (term) {
            .Exited => |exit_code| std.enums.fromInt(ErrorReturnCode, exit_code) orelse .unexpected,
            else => .unexpected,
        };
        
        test_results.append(a, .{.path = full_path, .return_code = return_code}) catch unreachable;
    }
}

const Options = struct {
    compiler_path: []const u8,
    test_root: []const u8,
    color: bool,
};

var enable_color = false;

const Color = enum {
    red,
    blue,
    green,
    reset,

    pub fn format(self: Color, writer: anytype) !void {
        if (!enable_color) return;
        _ = switch (self) {
            .red => try writer.write("\x1b[31m"),
            .blue => try writer.write("\x1b[34m"),
            .green => try writer.write("\x1b[32m"),
            .reset => try writer.write("\x1b[0m"),
        };
    }
};

//pub fn color(str: []const u8, color: ColorCode) void {
//    
//}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}) {};
    const alloc = gpa.allocator();

    var args = try std.process.argsWithAllocator(alloc);
    const pgm_name = args.next().?;

    var opts: Options = undefined;
    var args_parser = Cli.ArgParser {};
    args_parser.init(alloc, pgm_name, "The Octave Test System");
    try args_parser.add_opt([]const u8, &opts.compiler_path, .none, .positional, "<compiler-path>",
        "the path to the octave compiler")
    .add_opt([]const u8, &opts.test_root, .none, .positional, "<test_root>",
        "the path to the test root directory")
    .add_opt(bool, &opts.color, .{.just = &true}, .{.prefix = "--color"}, "",
        "enable colorful output if tty is attached")
    .parse(&args);

    var test_results = TestResults {};
    run_tests_on_dir(alloc, .play, "test", opts.compiler_path, &test_results);
    run_tests_on_dir(alloc, .parse, "test/parser", opts.compiler_path, &test_results);
    run_tests_on_dir(alloc, .lex, "test/lexer", opts.compiler_path, &test_results);

    const stdout_raw = std.fs.File.stdout();
    var buf: [256]u8 = undefined;
    var stdout_writer = stdout_raw.writer(&buf);
    var stdout = &stdout_writer.interface;
    defer stdout.flush() catch unreachable;

    enable_color = opts.color and stdout_raw.getOrEnableAnsiEscapeSupport();

    for (test_results.items) |result| {
        stdout.print("{s: <40}", .{result.path}) catch unreachable;
        if (result.return_code == .success) {
            stdout.print("{f}success{f}", .{Color.green, Color.reset}) catch unreachable;
        } else {
            stdout.print("{f}{s}{f}", .{Color.red, @tagName(result.return_code), Color.reset}) catch unreachable;
        }
        stdout.writeByte('\n') catch unreachable;
    }
}
