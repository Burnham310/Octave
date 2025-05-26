const std = @import("std");


pub fn build(b: *std.Build) void {
    const target = b.resolveTargetQuery(.{});
    const opt = b.standardOptimizeOption(.{});

    const octave_compiler = b.addExecutable(.{
        .name = "octc",
        .target = target,
        .optimize = opt,
        .root_source_file = b.path("src/main.zig"),
    });
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
    // octave_compiler.addCSourceFile(.{
    //     .file = b.path("entry/main.c"),
    // });
    octave_compiler.addIncludePath(b.path("src"));
    octave_compiler.linkLibC();

    b.installArtifact(octave_compiler);
}
