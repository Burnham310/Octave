const std = @import("std");

const Build = std.Build;
const Step = Build.Step;

fn run_compiler_on_dir(
    b: *Build,
    compiler: *Step.Compile,
    dir_path: []const u8,
    prefix_filter: ?[]const u8,
    step_opt_name: []const u8,
    step_opt_desc: []const u8,
    addtional_flags: []const []const u8) *Step {

    const run_examples_step = b.step(step_opt_name, step_opt_desc); 
    const test_path = b.path(dir_path).getPath3(b, null);
    const test_dir = test_path.openDir(".", .{.iterate = true}) catch unreachable;
    var test_dir_it = test_dir.iterate();
    while (test_dir_it.next() catch unreachable) |file| {
        if (file.kind != .file) continue;
        if (prefix_filter) |prefix| {
            if (!std.mem.startsWith(u8, file.name, prefix)) continue;
        }
        const file_path = test_path.joinString(b.allocator, file.name) catch unreachable;
        const run_step = Step.Run.create(b, "");
        run_step.producer = compiler;
        run_step.addArtifactArg(compiler);
        run_step.addArg(file_path);
        run_step.addArgs(addtional_flags);
        run_examples_step.dependOn(&run_step.step);
    }
    return run_examples_step;
}

pub fn build(b: *Build) void {
    const target = b.resolveTargetQuery(.{});
    const opt = b.standardOptimizeOption(.{});

    const zynth = b.dependency("zynth", .{});
    const zynth_mod = zynth.module("zynth");

    const octave_compiler = b.addExecutable(.{
        .name = "octc",
        .target = target,
        .optimize = opt,
        .root_source_file = b.path("src/main.zig"),
    });
    octave_compiler.root_module.addImport("zynth", zynth_mod);
    octave_compiler.addCSourceFiles(.{
        .root = b.path("src"),
        .files = &.{
            "backend.c",
            "compiler.c",
            "ds.c",
            "lexer.c",
            "midi.c",
            "parser.c",
            "sema.c",
            "utils.c",
        },
        .flags = &.{
            "-g", "-Wall", "-Wextra", "-Wno-switch", "-Wno-missing-braces",
        }
    });
    octave_compiler.addIncludePath(b.path("src"));
    octave_compiler.linkLibC();
    b.installArtifact(octave_compiler);

    const prefix_filter = b.option([]const u8, "prefix-filter", "filter out tests/examples to run with the provided prefix");

    _ = run_compiler_on_dir(b, octave_compiler, "test", prefix_filter, "run-examples", "Run the examples, only does Semantic Anaylsis",
        &.{ "-o", "-", "--stage", "Compiling" });

    _ = run_compiler_on_dir(b, octave_compiler, "test/lexer", prefix_filter, "run-lexer-tests", "Run the lexer tests",
        &.{ "-o", "-", "--stage", "Lexing" });

    
}
