const std = @import("std");
const assert = std.debug.assert;
const c = @cImport({
    @cInclude("sema.h");
});

const ThreadSafeQueue = @import("thread_safe_queue.zig").ThreadSafeQueue;
const Allocator = std.mem.Allocator;
const Note = struct {};

pub const Evaulator = struct {
    ctx: *c.Context,
    queue: ThreadSafeQueue(Note),
    worker: std.Thread,
    pub fn init(ctx: *c.Context, a: Allocator) Evaulator {
        return .{
            .ctx = ctx,
            .queue = ThreadSafeQueue(Note).initCapacity(10, a),
            .worker = undefined,
        };
    }
    pub fn start(self: Evaulator) void {
        self.worker = std.Thread.spawn(.{}, eval, self.ctx, &self.queue);
    }
    fn eval(ctx: *c.Context, queue: *ThreadSafeQueue(Note)) void {
        const pgm: *c.Pgm = ctx.pgm.?;
        _ = queue;
        // Temporay Implementation: assume it only has one declaration, which is main.
        // main MUST be a single section.
        // The section cannot contain any declaration.
        // It cannot contain any chord, but only single notes
        const formals = pgm.formals.ptr[0..pgm.formals.len];
        assert(formals.len == 0);

        const main_formal = formals[0];
        assert(main_formal.ident == ctx.builtin_syms.main);

    }
    pub fn get(self: Evaulator) Note {
        return self.queue.pull();
    }
};

