const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const c = @import("c.zig");
const ThreadSafeQueue = @import("thread_safe_queue.zig").ThreadSafeQueue;

const Zynth = @import("zynth");

pub const Note = struct {
    freq: f32,
    duration: f32,
    gap: f32, // gap since last note

    pub fn is_eof(self: Note) bool {
        return self.freq == 0 and self.duration == 0;
    }

    pub fn eof(gap: f32) Note {
        return .{ .freq = 0,. duration = 0, .gap = gap };
    }
};

pub const Evaluator = struct {
    ctx: *c.Context,
    queue: ThreadSafeQueue(Note),
    worker: std.Thread,

    pub fn init(ctx: *c.Context, a: Allocator) Evaluator {
        return .{
            .ctx = ctx,
            .queue = ThreadSafeQueue(Note).initCapacity(10, a),
            .worker = undefined,
        };
    }

    pub fn start(self: *Evaluator) void {
        self.worker = std.Thread.spawn(.{}, eval, .{ self } ) catch undefined; // TODO: handle error
    }

    fn as_slice(slice: anytype) []@TypeOf(slice.ptr.*) {
        return slice.ptr[0..slice.len];
    }

    // only works on number
    fn eval_strict(self: *Evaluator, expr_idx: c.ExprIdx) isize {
        const pgm: *c.Pgm = self.ctx.pgm.?;
        const exprs = as_slice(pgm.exprs);
        const expr = &exprs[@intCast(expr_idx)]; 
        return switch (expr.tag) {
            c.EXPR_NUM => expr.data.num,
            else => @panic("TODO"),
            
        };
    }

    fn eval_lazy(self: *Evaluator, expr_idx: c.ExprIdx, nth: u32) ?c.ExprIdx {
        const pgm: *c.Pgm = self.ctx.pgm.?;
        const exprs = as_slice(pgm.exprs);
        const expr = &exprs[@intCast(expr_idx)]; 
        return switch (expr.tag) {
            c.EXPR_NUM => if (nth == 0) expr_idx else null,
            c.EXPR_LIST => if (nth < expr.data.chord_notes.len) expr.data.chord_notes.ptr[nth] else null,
            else => @panic("TODO"),
        };
    }

    fn eval(self: *Evaluator) void {
        const ctx = self.ctx;
        const pgm: *c.Pgm = ctx.pgm.?;
        var scale = c.Scale {.tonic = c.PTCH_C, .mode = c.MODE_MAJ, .octave = 1};
        const bpm = 120.0;
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
        var gap: f32 = 0;
        for (note_exprs) |note_expr| {
            const note = exprs[@intCast(note_expr)].data.note;
            const dots = note.dots;
            var i: u32 = 0; 
            const dura: Zynth.NoteDuration = @enumFromInt(dots);
            while (self.eval_lazy(note.expr, i)) |pitch_expr_next|: (i += 1) {
                const deg = self.eval_strict(pitch_expr_next);
                const abs_pitch = c.abspitch_from_scale(&scale, @intCast(deg));
                const freq = Zynth.pitch_to_freq(abs_pitch);
                // the first one should have gap equal to the duration of the previous note
                // the rest should have zero
                self.queue.push(Note {.freq = freq, .duration = dura.to_sec(bpm), .gap = gap }); 
                gap = 0;
            }
            gap = dura.to_sec(bpm);
            
        }
        self.queue.push(Note.eof(gap));
    }

    pub fn get(self: *Evaluator) Note {
        return self.queue.pull();
    } };

