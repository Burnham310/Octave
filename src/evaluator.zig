const c = @cImport({
    @cInclude("sema.h");
});
const std = @import("std");
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
        self.worker = std.Thread.spawn(.{}, eval, self.pgm, self.ctx, &self.queue);
    }
    fn eval(ctx: *c.Context, queue: *ThreadSafeQueue(Note)) void {
        const pgm: *c.Pgm = ctx.pgm.?;
        _ = pgm;
        _ = queue;
        // for (pgm.toplevel)
    }
    pub fn get(self: Evaulator) Note {
        return self.queue.pull();
    }
};

