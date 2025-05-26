const c = @cImport({
    @cInclude("lexer.h");
    @cInclude("ast.h");
    @cInclude("parser.h");
    @cInclude("backend.h");
    @cInclude("utils.h");
    @cInclude("sema.h");
});
const std = @import("std");
const Allocator = std.mem.Allocator;

fn usage(prog_name: []const u8) void {
    std.debug.print("Usage: {s} <input_file> -o <output_file>\n", .{prog_name});
}

const Error = error {
    InvalidCliArg,
    SyntaxError,
    TypeMismatch,
};

pub fn main() !void  {
    const alloc = std.heap.c_allocator;
    var args = try std.process.argsWithAllocator(alloc);
    defer args.deinit();

    const program_name = args.next().?;
    errdefer usage(program_name);
    const input_path = args.next() orelse {
        return Error.InvalidCliArg;
    };
    if (!std.mem.eql(u8, args.next() orelse {
        return Error.InvalidCliArg;
    }, "-o")) {
        return Error.InvalidCliArg;
    }
    const output_path = args.next() orelse {
        return Error.InvalidCliArg;
    };
    const cwd = std.fs.cwd();
    const input_f = try cwd.openFileZ(input_path, .{.mode = .read_only });
    defer input_f.close();
    const output_f = if (std.mem.eql(u8, output_path, "-")) 
        std.io.getStdOut() else try cwd.createFileZ(output_path, .{});
    defer output_f.close();
    const buf = try input_f.readToEndAlloc(alloc, 1024 * 1024);
    var lexer = c.lexer_init(buf.ptr, buf.len, input_path);
    var pgm = c.parse_ast(&lexer);
    if (!pgm.success) {
        return Error.SyntaxError;
    }
    var ctx: c.Context = undefined;

    c.sema_analy(&pgm, &lexer, &ctx);
    if (!ctx.success) {
        return Error.TypeMismatch;
    }
}

