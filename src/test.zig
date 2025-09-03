const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const Cli = @import("cli.zig");
const CompileStage = Cli.CompileStage;
const ErrorReturnCode = Cli.ErrorReturnCode;

const Note = @import("main.zig").Note;

const TestResult = struct {
    path: []const u8,  
    return_code: ErrorReturnCode,
    diagnostic: []const u8,
};
const TestResults = std.ArrayListUnmanaged(TestResult);
const fatal = std.process.fatal;
// pub const Note = struct {
//     freq: f32,
//     duration: f32,
//     gap: f32, // gap since last note
//     amp: f32,
//     inst: SectionEvaluator.Instrument,
// 
//     pub fn is_eof(self: Note) bool {
//         return self.freq == 0 and self.duration == 0;
//     }
// 
//     pub fn eof(gap: f32) Note {
//         return .{ .freq = 0,. duration = 0, .gap = gap, .amp = 0, .inst = .guitar };
//     }
// };

// Hashing floats require a bit of hack...
const NoteContext = struct {
    pub fn hash(_: NoteContext, note: Note) u64 {
        var hasher = std.hash.Wyhash.init(0);
        auto_hash(&hasher, note);
        return hasher.final();
    }

    pub fn eql(_: NoteContext, a: Note, b: Note) bool {
        return 
            a.freq == b.freq and 
            a.duration == b.duration and 
            a.gap == b.gap and
            a.amp == b.amp and
            a.inst == b.inst;
    }
    
    fn auto_hash(hasher: anytype, key: anytype) void {
        const Key = @TypeOf(key);
        switch (@typeInfo(Key)) {
            .float => |info| {
                if (key == 0.0)
                    return std.hash.autoHash(hasher, @as(std.meta.Int(.unsigned, info.bits), @bitCast(@as(f32, 0.0))));
                return std.hash.autoHash(hasher, @as(std.meta.Int(.unsigned, info.bits), @bitCast(key)));
            },
            .@"struct" => |info| {
                inline for (info.fields) |field| {
                    auto_hash(hasher, @field(key, field.name));
                }
            },

            else => return std.hash.autoHash(hasher, key),
        }
    }
};

// Check rather the outputted sequence of notes is equal to the expected.
// We represent music as a "sequence" of notes. However, notes can be simultaneously.
// Therefore, note contains a `gap` attribute, we tell us how much time should be waited
// for the next note to be played, after this note is played. The `gap` can be different from its `duration`.
// When a note has `gap = 0`, it means that this note would be played concurrently with the next note.
// Such notes to be played concurrently is called the staging sequences.
fn check_stdout(stdout_file: std.fs.File, expected_file: std.fs.File, record_playing: bool, diagnostic: *std.io.Writer, a: Allocator) bool {
    var buf1: [256]u8 = undefined;
    var stdout_reader = stdout_file.reader(&buf1);
    const stdout = &stdout_reader.interface;
        
    const stdout_content = stdout.allocRemaining(a, .unlimited) catch unreachable;
    const stdout_notes = std.json.parseFromSlice([]Note, a, stdout_content, .{}) catch |e| {
        diagnostic.print("{}: stdout is not a valid json of `[]Note`", .{e}) catch unreachable;
        return false;
    };
    defer stdout_notes.deinit();
    // std.log.debug("notes: {}", .{stdout_notes.value.len});
    var buf2: [256]u8 = undefined;
    if (record_playing) {
        expected_file.setEndPos(0) catch unreachable;
        expected_file.seekTo(0) catch unreachable;
        var expected_writer = expected_file.writer(&buf2);
        expected_writer.interface.writeAll(stdout_content) catch unreachable;
        return true;
    } else {
        var expected_reader = expected_file.reader(&buf2);
        const expected = &expected_reader.interface;
        const expected_content = expected.allocRemaining(a, .unlimited) catch unreachable;
        const expected_notes = std.json.parseFromSlice([]Note, a, expected_content, .{}) catch |e| {
            diagnostic.print("{}: expected stdout is not a valid json of `[]Note`", .{e}) catch unreachable;
            return false;
        };
        defer expected_notes.deinit();
        if (expected_notes.value.len != stdout_notes.value.len) {
            diagnostic.print("expect {} notes, got {}", .{expected_notes.value.len, stdout_notes.value.len}) catch unreachable;
            return false;
        }
        //
        // Staging sequence
        // Notes inside a staging sequence are played simultaneously. Therefore, they can appear in any order,
        // and the equality should still holds.
        //
        // When a note has `gap = 0`, it is the start of the staging sequence.
        // The staging sequence ends when a note has `gap > 0` (this end note is also in the staging sequence).
        // We then compare the expected and outputted s6d6ead44fc739eeb40fe67e3ac696dfa313b1d20taging sequences in a orderless manner.
        //
        // To do this, we put note in the staging sequence inside a map (Note -> i32).
        // An expected note increments the count, an outputted note decrements it.
        // After all note is considered, all the counts be zero.
        const StagingRecord = struct {
            count: i32,
            index: usize, // The note's index in the total sequence. This is used for better error reporting.
        };
        var staging_notes = std.HashMapUnmanaged(Note, StagingRecord, NoteContext, std.hash_map.default_max_load_percentage).empty;
        defer staging_notes.deinit(a);
        var staging = false;
        for (expected_notes.value, stdout_notes.value, 0..) |expected_note, stdout_note, i| {
            if (expected_note.gap != stdout_note.gap) {
                diagnostic.print("At {}th note, expect gap {}, got {}", .{i, expected_note.gap, stdout_note.gap}) catch unreachable;
                return false;
            }
            if (expected_note.gap == 0 or staging) {
                const gop_expected = staging_notes.getOrPutValue(a, expected_note, .{ .count = 0, .index = i}) catch unreachable;
                gop_expected.value_ptr.count += 1;
                const gop_stdout = staging_notes.getOrPutValue(a, stdout_note, .{ .count = 0, .index = i }) catch unreachable;
                gop_stdout.value_ptr.count -= 1;
                
                if (expected_note.gap == 0) {
                    staging = true;
                } else {
                    staging = false;
                    var it = staging_notes.iterator();
                    while (it.next()) |entry| {
                        if (entry.value_ptr.count > 0) {
                            diagnostic.print("At {}th, {} in concurrent sequence is expected, but does not exist", 
                                .{ entry.value_ptr.index, entry.key_ptr.* }) catch unreachable;
                            return false;
                        } else if (entry.value_ptr.count < 0) {
                            diagnostic.print("At {}th, {} in concurrent sequence is not expected, but exists", 
                                .{ entry.value_ptr.index, entry.key_ptr.* }) catch unreachable;
                            return false;
                        }
                    }
                }
            } else {
                assert(!staging);
                if (!NoteContext.eql(.{}, expected_note, stdout_note)) return false;
            }
        }
    }!
    return true;
}

pub fn run_tests_on_dir(
    a: Allocator, 
    stage: CompileStage, path: []const u8, compiler_path: []const u8, 
    test_results: *TestResults,
    record_playing: bool) void {

    const dir = std.fs.cwd().openDir(path, .{ .iterate = true }) catch |e| fatal("{}: cannot open test direcotry `{s}`", .{e, path});
    var it = dir.iterate();
    while (it.next() 
        catch |e| fatal("{}: cannot iterate on `{s}`" , .{e, path})
        ) |entry| {
        if (entry.kind != .file) continue;
        const ext = std.fs.path.extension(entry.name);
        if (!std.mem.eql(u8, ext, ".oct") and !std.mem.eql(u8, ext, ".test")) continue; 
        const full_path = std.fmt.allocPrint(a, "{s}/{s}", .{path, entry.name}) catch unreachable;

        var child = switch (stage) {
            .play => 
                std.process.Child.init(&.{compiler_path, "--stage", @tagName(stage), "-g", full_path}, a),
            else => 
                std.process.Child.init(&.{compiler_path, "--stage", @tagName(stage), full_path}, a),
        };
        child.stdout_behavior = if (stage == .play) .Pipe else .Ignore;
        child.stderr_behavior = .Pipe;
        child.spawn() catch unreachable;

        var diagnostic_storage = std.io.Writer.Allocating.init(a);
        defer diagnostic_storage.deinit();
        const diagnostic = &diagnostic_storage.writer;
       
        var stderr_buf: [256]u8 = undefined;
        var stderr_reader = child.stderr.?.reader(&stderr_buf);
        _ = diagnostic.sendFileReadingAll(&stderr_reader, .unlimited) catch unreachable;

        // std.log.debug("running {s}", .{entry.name});
        const stdout_equality: bool = if (child.stdout) |stdout| blk: {
            // TODO: skip checking if diagnostic is not empty?
            const expected_output_path = std.fmt.allocPrint(a, "{s}.stdout", .{entry.name}) catch unreachable;
            const expected_f = dir.createFile(expected_output_path, .{ .read = true, .truncate = false }) catch |e| {
                diagnostic.print("{}: unable to create/open expected output file `{s}` under `{s}`", .{e, expected_output_path, path}) catch unreachable;
                break :blk true;
            };
            break :blk check_stdout(stdout, expected_f, record_playing, diagnostic, a);  
        } else true;
        const term = child.wait() catch {
            test_results.append(a, .{ .path = full_path, .return_code = .unexpected, .diagnostic = "" }) catch unreachable;
            continue;
        };
        const return_code = switch (term) {
            .Exited => |exit_code| std.enums.fromInt(ErrorReturnCode, exit_code) orelse .unexpected,
            else => .unexpected,
        };
        
        test_results.append(a, 
            .{ 
                .path = full_path, 
                .return_code = if (return_code == .success and !stdout_equality) .eval else return_code, 
                .diagnostic = diagnostic_storage.toOwnedSlice() catch unreachable
            }) catch unreachable;
    }
}

const Options = struct {
    compiler_path: []const u8,
    test_root: []const u8,
    color: bool,
    record: bool,
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
    .add_opt(bool, &opts.record, .{.just = &false}, .{.prefix = "--record"}, "",
        "record the output of playings and override the ")
    .parse(&args);

    var test_results = TestResults {};
    run_tests_on_dir(alloc, .play, "test", opts.compiler_path, &test_results, opts.record);
    run_tests_on_dir(alloc, .parse, "test/parser", opts.compiler_path, &test_results, opts.record);
    run_tests_on_dir(alloc, .lex, "test/lexer", opts.compiler_path, &test_results, opts.record);

    const stdout_raw = std.fs.File.stdout();
    var buf: [256]u8 = undefined;
    var stdout_writer = stdout_raw.writer(&buf);
    var stdout = &stdout_writer.interface;
    defer stdout.flush() catch unreachable;

    enable_color = opts.color and stdout_raw.getOrEnableAnsiEscapeSupport();
    var all_success = true;

    stdout.print("--- Overview ---\n\n", .{}) catch unreachable;
    stdout.print("total test run: {}\n\n", .{test_results.items.len}) catch unreachable;
    for (test_results.items) |result| {
        stdout.print("{s: <40}", .{result.path}) catch unreachable;
        if (result.return_code == .success) {
            stdout.print("{f}success{f}", .{Color.green, Color.reset}) catch unreachable;
        } else {
            all_success = false;
            stdout.print("{f}{s}{f}", .{Color.red, @tagName(result.return_code), Color.reset}) catch unreachable;
        }
        stdout.writeByte('\n') catch unreachable;
    }

    stdout.print("\n\n--- Details ---\n\n", .{}) catch unreachable;
    for (test_results.items) |result| {
        if (result.diagnostic.len == 0) continue;
        stdout.print("{s}:\n", .{result.path}) catch unreachable;
        stdout.print("{s}\n", .{result.diagnostic}) catch unreachable;
    }

    if (!all_success) return error.Failed;

}
