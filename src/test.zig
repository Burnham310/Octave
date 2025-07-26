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

pub fn run_tests_on_dir(a: Allocator, stage: CompileStage, path: []const u8, compiler_path: []const u8, test_results: *TestResults) void {
    const dir = std.fs.cwd().openDir(path, .{ .iterate = true }) catch |e| fatal("{}: cannot open test direcotry `{s}`", .{e, path});
    var it = dir.iterate();
    while (it.next() 
        catch |e| fatal("{}: cannot iterate on `{s}`", .{e, path})
        ) |entry| {
        if (entry.kind != .file) continue;
        const full_path = std.fmt.allocPrint(a, "{s}/{s}", .{path, entry.name}) catch unreachable;
        var child = std.process.Child.init(&.{compiler_path, "--stage", @tagName(stage), full_path}, a);
        child.stdout_behavior = .Ignore;
        child.stderr_behavior = .Ignore;
        const term = child.spawnAndWait() catch {
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

    pub fn to_code(self: Color) []const u8 {
        if (!enable_color) return "";
        return switch (self) {
            .red => "\x1b[31m",
            .blue => "\x1b[34m",
            .green => "\x1b[32m",
            .reset => "\x1b[0m",
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
    _ = args.next().?;

    var opts: Options = undefined;
    var args_parser = Cli.ArgParser {.a = alloc};
    args_parser.add_opt([]const u8, &opts.compiler_path, null, .positional, "<compiler-path>");
    args_parser.add_opt([]const u8, &opts.test_root, null, .positional, "<test_root>");
    args_parser.add_opt(bool, &opts.color, &false, .{.prefix = "--color"}, "");
    try args_parser.parse(&args);

    var test_results = TestResults {};
    run_tests_on_dir(alloc, .sema, "test", opts.compiler_path, &test_results);
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
            stdout.print("{s}success{s}", .{Color.green.to_code(), Color.reset.to_code()}) catch unreachable;
        } else {
            stdout.print("{s}{s}{s}", .{Color.red.to_code(), @tagName(result.return_code), Color.reset.to_code()}) catch unreachable;
        }
        stdout.writeByte('\n') catch unreachable;
    }
}
