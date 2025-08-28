const std = @import("std");
const zynth_build = @import("zynth");

const Build = std.Build;
const Step = Build.Step;

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const opt = b.standardOptimizeOption(.{});

    const zynth = b.dependency("zynth", .{.target = target, .optimize = opt});
    const zynth_mod = zynth.module("zynth");
    
    if (target.result.cpu.arch.isWasm()) {
        const root_module = zynth_build.create_wasm_mod(b, opt, "octc", b.path("src/main.zig"));
        root_module.addImport("zynth", zynth_mod);
        const libc_include = zynth_build.get_wasm_include_from_sysroot(b);
        root_module.addIncludePath(libc_include);
        const wasm_step = zynth_build.compile_to_wasm(b, opt, root_module, "octc");
        b.getInstallStep().dependOn(wasm_step);

        b.installDirectory(.{
            .source_dir = b.path("web"),
            .install_dir = .{ .custom = "web" },
            .install_subdir = ".",
        });
        return;
    }

    const root_module = b.addModule("octc", .{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = opt,
    });

    const octave_compiler = b.addExecutable(.{
        .name = "octc",
        .root_module = root_module,
    });
    octave_compiler.root_module.addImport("zynth", zynth_mod);
    octave_compiler.addIncludePath(b.path("src"));
    octave_compiler.linkLibC();

    b.installArtifact(octave_compiler);

    const test_step = b.step("test", "invoke the integrated test system");
    const install_test_step = b.step("install-test", "install the test executable");

    const test_module = b.addModule("test", .{
        .root_source_file = b.path("src/test.zig"),
        .target = target,
        .optimize = opt,
    });
    const test_system = b.addExecutable(.{
        .name = "test",
        .root_module = test_module,
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
