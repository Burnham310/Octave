const std = @import("std");

const Build = std.Build;
const Step = Build.Step;

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
    octave_compiler.addIncludePath(b.path("src"));
    octave_compiler.linkLibC();

    b.installArtifact(octave_compiler);

    const test_step = b.step("test", "invoke the integrated test system");
    const install_test_step = b.step("install-test", "install the test executable");
    
    const test_system = b.addExecutable(.{
        .name = "test",
        .target = target,
        .optimize = .Debug,
        .root_source_file = b.path("src/test.zig"),
    });

    const run_test = b.addRunArtifact(test_system);
    run_test.addArtifactArg(octave_compiler);
    run_test.addFileArg(b.path("test"));
    run_test.step.dependOn(&octave_compiler.step);
    test_step.dependOn(&run_test.step);

    const install_test = b.addInstallArtifact(test_system, .{});
    install_test_step.dependOn(&install_test.step);
    //const prefix_filter = b.option([]const u8, "prefix-filter", "filter out tests/examples to run with the provided prefix");
}
