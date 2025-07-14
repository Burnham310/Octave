const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const Ast = @import("ast.zig");
const Sema = @import("sema.zig");
const Tonality = @import("tonality.zig");
const ThreadSafeQueue = @import("thread_safe_queue.zig").ThreadSafeQueue;

const Zynth = @import("zynth");

pub fn abspitch_from_scale(scale: c.Scale, degree: usize) c.AbsPitch {
    std.debug.assert(degree <= c.DIATONIC and degree >= 1);
    const base = scale.tonic + scale.octave * 12;
    // degree is 1-based
    const degree_shift = degree + scale.mode - 1;
    // Step is how much we should walk from the tonic
    const step = if (degree_shift < c.DIATONIC)
	c.BASE_MODE[degree_shift] - c.BASE_MODE[scale.mode]
        else 12 + c.BASE_MODE[degree_shift % c.DIATONIC] - c.BASE_MODE[scale.mode];
    return base + step;
}


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


// Design goal: we want to lazily evaluate a section
// Even if we evaluate this note(_expr) by note(_expr), it is not enough:
// ONE note_expr can generate multiple note (chord), or even an indefinte amount of note (for loop).
// Therefore, lazy section is not enough, we need lazy note,
// which is much more complicated, it turns out.
// Let's look at the various form of note
// 
// Single Num: 1
// Infix Expression: 1'0
// Chord: [1 3 5] ([1 3 5]'2)
// For loop: for 1~<5 loop 1'2 2 end
pub const Evaluator = struct {
    anno: *Sema.Annotations,
    ast: *Ast,
    queue: ThreadSafeQueue(Note),
    worker: std.Thread,
    iterators: []u32,
    
    fn post_inc(it: *u32) u32 {
        it.* += 1;
        return it.* - 1;
    }

    pub fn init(ctx: *Sema.Annotations, a: Allocator) Evaluator {
        return .{
            .ctx = ctx,
            .queue = ThreadSafeQueue(Note).initCapacity(10, a),
            .worker = undefined,
            .iterators = a.alloc(u32, ctx.pgm.exprs.len) catch unreachable,
        };
    }

    pub fn start(self: *Evaluator) void {
        self.worker = std.Thread.spawn(.{}, eval, .{ self } ) catch undefined; // TODO: handle error
    }

    fn as_slice(slice: anytype) []@TypeOf(slice.ptr.*) {
        return slice.ptr[0..slice.len];
    }

    // only works on number
    fn eval_strict(self: *Evaluator, expr: *Ast.Expr) c.Degree {
        const pgm: *c.Pgm = self.ctx.pgm.?;
        const exprs = as_slice(pgm.exprs);
        const expr = &exprs[@intCast(expr_idx)]; 
        return switch (expr.tag) {
            c.EXPR_NUM => c.Degree { .degree = @intCast(expr.data.num), .shift = 0 },
            c.EXPR_INFIX => blk: {
                std.debug.assert(expr.data.infix.op.type == '\'');
                const shift = self.eval_strict(expr.data.infix.lhs).degree;
                const deg = self.eval_strict(expr.data.infix.rhs).degree;
                break :blk c.Degree { .degree = @intCast(deg), .shift = shift };
                
            },
            // c.EXPR_IDENT => blk: {
            //     const sec_envs = as_slice(self.ctx.sec_envs);
            //     const env_i = c.hmgeti(sec_envs[0], expr.data.ident);
            //     break :blk if (env_i >= 0)
            //         sec_envs[0][env_i].value.data.i
            //         else c.hmget(self.ctx.pgm_env, expr.data.ident).data.i;
            // },
            else => @panic("TODO"),
            
        };
    }

    fn eval_lazy_head(self: *Evaluator, expr_idx: c.ExprIdx) c.ValData {
        const pgm: *c.Pgm = self.ctx.pgm.?;
        const exprs = as_slice(pgm.exprs);
        const secs = as_slice(pgm.secs);
        const expr = &exprs[@intCast(expr_idx)]; 
        const it = &self.iterators[expr_idx];
        switch (expr.tag) {
            c.EXPR_NUM => return if (post_inc(it) == 0) c.ValData {.i = expr.data.num } else null,
            c.EXPR_INFIX => {
                std.debug.assert(expr.data.infix.op.type == '\'');
                const shift = self.eval_strict(expr.data.infix.lhs).degree;
                const deg = self.eval_strict(expr.data.infix.rhs).degree;
                _ = shift;
                _ = deg;
                return undefined;
            },
            c.EXPR_LIST => {
                const list = as_slice(expr.data.chord_notes);
                while (it.* < list.len): (it.* += 1) {
                    if (self.eval_lazy_head(list[it.*])) |next| return self.eval_strict(next);
                    it.* += 1;
                }
                return null;
            },
            c.EXPR_SEC => {
                const sec = &secs[@intCast(expr.data.sec)];
                const note_exprs = as_slice(sec.note_exprs);
                return if (nth < note_exprs.len) note_exprs[nth] else null;
            },
            else => @panic("TODO")
        }
    
    }

    fn eval(self: *Evaluator) void {
        const anno = self.anno;
        const ast = self.ast;
        const bpm = 120.0;
        // Temporay Implementation: assume it only has one declaration, which is main.
        // main MUST be a single section.
        // The section cannot contain any declaration.
        // It cannot contain any chord, but only single notes
        assert(ast.toplevel.len == 1);
        assert(ast.secs.len == 1);

        const main_formal = anno.main_formal;
        const sec = main_formal.expr.data.sec;
        assert(sec.config.len == 0);
        // const configs = as_slice(sec.config);
        // const env = c.get_curr_env(ctx, 0);
        // for (configs) |formal_idx| {
        //     const formal = &formals[@intCast(formal_idx)];
        //     c.hmgetp(env.*, formal.ident).value.data.i = eval_strict(formal.expr);
        // }

        // const scale = c.Scale {
        //     .tonic = c.hmgetp(env, ctx.builtin_syms.tonic),
        //     .octave = c.hmgetp(env, ctx.builtin_syms.octave),
        //     .mode = c.hmgetp(env, ctx.builtin_syms.mode),
        // };
        //
        const scale = Tonality.Scale {
            .tonic = Tonality.Pitch.C,
            .octave = 3,
            .mode = Tonality.Mode.major,
        };

        const note_exprs = sec.note;
        var gap: f32 = 0;
        for (note_exprs) |note_expr| {
            const note = exprs[@intCast(note_expr)].data.note;
            const dots = note.dots;
            var i: u32 = 0; 
            const dura: Zynth.NoteDuration = @enumFromInt(dots);
            while (self.eval_lazy_head(note.expr, i)) |pitch_expr_next|: (i += 1) {
                const deg = self.eval_strict(pitch_expr_next);
                const abs_pitch = abspitch_from_scale(scale, @intCast(deg.degree));
                const freq = Zynth.pitch_to_freq(abs_pitch+deg.shift-24);
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

