const std = @import("std");
const assert = std.debug.assert;
const c = @import("c.zig");

const ThreadSafeQueue = @import("thread_safe_queue.zig").ThreadSafeQueue;
const Allocator = std.mem.Allocator;
pub const Note = struct {
    freq: f32,
    duration: f32,
};

pub const Evaluator = struct {
    ctx: *c.Context,
    queue: ThreadSafeQueue(?Note),
    worker: std.Thread,

    pub fn init(ctx: *c.Context, a: Allocator) Evaluator {
        return .{
            .ctx = ctx,
            .queue = ThreadSafeQueue(?Note).initCapacity(10, a),
            .worker = undefined,
        };
    }

    pub fn start(self: *Evaluator) void {
        self.worker = std.Thread.spawn(.{}, eval, .{ self.ctx, &self.queue } ) catch undefined; // TODO: handle error
    }

    fn eval(ctx: *c.Context, queue: *ThreadSafeQueue(?Note)) void {
        const pgm: *c.Pgm = ctx.pgm.?;
        var scale = c.Scale {.tonic = c.PTCH_C, .mode = c.MODE_MAJ, .octave = 1};
        // Temporay Implementation: assume it only has one declaration, which is main.
        // main MUST be a single section.
        // The section cannot contain any declaration.
        // It cannot contain any chord, but only single notes
        const formals = pgm.formals.ptr[0..pgm.formals.len];
        const exprs = pgm.exprs.ptr[0..pgm.exprs.len];
        const secs = pgm.secs.ptr[0..pgm.secs.len];
        const toplevel = pgm.toplevel.ptr[0..pgm.toplevel.len];
        assert(toplevel.len == 1);

        const main_formal = formals[0];
        assert(main_formal.ident == ctx.builtin_syms.main);

        const expr = exprs[@intCast(main_formal.expr)];
        assert(expr.tag == c.EXPR_SEC);
        
        const sec = secs[@intCast(expr.data.sec)];
        
        assert(sec.vars.len == 0);
        assert(sec.config.len == 0);
        assert(sec.labels.len == 0);

        const note_exprs = sec.note_exprs.ptr[0..sec.note_exprs.len];
        for (note_exprs, 0..) |note_expr, i| {
            _ = i;
            const note = exprs[@intCast(note_expr)].data.note;
            const deg = exprs[@intCast(note.expr)].data.num;
            const dots = note.dots;
            const abs_pitch = c.abspitch_from_scale(&scale, @intCast(deg));
            queue.push(Note {.freq = 440.0 * @exp2(@as(f32, @floatFromInt(abs_pitch))/12.0), .duration = (120.0 / 60.0) / @exp2(@as(f32, @floatFromInt(dots))) });
        }
        queue.push(null);
    }

    pub fn get(self: *Evaluator) ?Note {
        return self.queue.pull();
    } };

