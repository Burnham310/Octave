const std = @import("std");
const zynth_build = @import("zynth");

const Build = std.Build;
const Step = Build.Step;

pub fn build(b: *Build) void {
    const target = b.standardTargetOptions(.{});
    const opt = b.standardOptimizeOption(.{});

    const zynth = b.dependency("zynth", .{.target = target, .optimize = opt});
    const zynth_mod = zynth.module("zynth");
    const zynth_preset_mod = zynth.module("preset");
    
    if (target.result.cpu.arch.isWasm()) {
        const root_module = zynth_build.create_wasm_mod(b, opt, "octc", b.path("src/main.zig"));
        root_module.addImport("zynth", zynth_mod);
        root_module.addImport("zynth_preset", zynth_preset_mod);
        const libc_include = zynth_build.get_wasm_include_from_sysroot(b);
        root_module.addIncludePath(libc_include);
        const wasm_step = zynth_build.compile_to_wasm(b, opt, root_module, "octc", 65536 * 10);
        b.getInstallStep().dependOn(wasm_step);

        const test_path = b.path("test");
        b.installDirectory(.{
            .source_dir = test_path,
            .install_dir = .{ .custom = "web" },
            .install_subdir = ".",
            .include_extensions = &.{".oct"},
        });

        b.installDirectory(.{
            .source_dir = b.path("web"),
            .install_dir = .{ .custom = "web" },
            .install_subdir = ".",
        });

        // collect the the .oct test files names into a json.
        // They will be used to display on the website
        var test_dir = test_path.getPath3(b, b.getInstallStep()).openDir(".", .{ .iterate = true }) catch unreachable;
        var it = test_dir.iterate();
        var examples = std.ArrayList([]const u8).empty;
        while (it.next() catch unreachable) |entry| {
            if (entry.kind != .file) continue;
            const ext = std.fs.path.extension(entry.name);
            if (!std.mem.eql(u8, ext, ".oct")) continue; 
            examples.append(b.allocator, b.allocator.dupe(u8, entry.name) catch unreachable) catch unreachable;
        }
        const examples_json = std.fmt.allocPrint(b.allocator, "{f}", .{std.json.fmt(examples.items, .{.whitespace = .indent_2 })}) catch unreachable;
        const wf = b.addWriteFiles();
        const wf_json = wf.add("manfiest.json", examples_json);
        const install_json = b.addInstallFile(wf_json, "web/manifest.json");
        b.getInstallStep().dependOn(&install_json.step);
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
        // .use_llvm = true,
    });
    octave_compiler.root_module.addImport("zynth", zynth_mod);
    octave_compiler.root_module.addImport("zynth_preset", zynth_preset_mod);
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
    const record_opt = b.option(bool, "record", "tell the test system to record the output") orelse false;

    const run_test = b.addRunArtifact(test_system);
    run_test.addArtifactArg(octave_compiler);
    run_test.addFileArg(b.path("test"));
    if (record_opt) run_test.addArg("--record");

    run_test.step.dependOn(&octave_compiler.step);
    test_step.dependOn(&run_test.step);

    const install_test = b.addInstallArtifact(test_system, .{});
    install_test_step.dependOn(&install_test.step);
    //const prefix_filter = b.option([]const u8, "prefix-filter", "filter out tests/examples to run with the provided prefix");
}
