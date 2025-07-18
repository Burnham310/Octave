const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const Ast = @import("ast.zig");
const Sema = @import("sema.zig");
const TypePool = @import("type_pool.zig");
const Tonality = @import("tonality.zig");
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


// Design goal: we want to lazily evaluate a section
// Even if we evaluate this note(_expr) by note(_expr), it is not enough:
// ONE note_expr can generate multiple note (chord), or even an indefinte amount of note (for loop).
// Therefore, lazy section is not enough, we need lazy note,
// which is much more complicated, it turns out.
// Let's look at the various form of note
// 
// Single Num: 1
// Infix Expression: 1'1, [1 3 5]'2
// Chord: [1 3 5]
// For loop: for 1~<5 loop 1'2 2 end
//
// single num, trivial
// chord, we take the first element
// infix, we know that the semantics of the expression is that it apply it to every element to the left, so we can just take first on the left and apply
pub const Evaluator = struct {
    ast: *Ast,
    anno: *Sema.Annotations,
    queue: ThreadSafeQueue(Note),
    worker: std.Thread,
    //iterators: []u32,
    
    fn post_inc(it: *u32) u32 {
        it.* += 1;
        return it.* - 1;
    }

    pub fn init(ast: *Ast, anno: *Sema.Annotations, a: Allocator) Evaluator {
        return .{
            .ast = ast,
            .anno = anno,
            .queue = ThreadSafeQueue(Note).initCapacity(9, a),
            .worker = undefined,
            //.iterators = a.alloc(u32, anno.pgm.exprs.len) catch unreachable,
        };
    }

    pub fn start(self: *Evaluator) void {
        self.worker = std.Thread.spawn(.{}, eval, .{ self } ) catch undefined; // TODO: handle error
    }

    fn as_slice(slice: anytype) []@TypeOf(slice.ptr.*) {
        return slice.ptr[0..slice.len];
    }

    const PreNote = struct {
        deg: isize,
        duration: Zynth.NoteDuration,
    };

    const Val = union {
        num: isize,
        note: PreNote,
    };

    // only works on number
    pub fn eval_expr(self: *Evaluator, expr: *Ast.Expr) Val {
        const t_full = TypePool.lookup(expr.ty);
        switch (expr.data) {
            .num => |i| {
                switch (t_full) {
                    .int => return Val {.num = i },
                    .note => return Val {.note = PreNote {.deg = i, .duration = .Quarter}},
                    else => unreachable,
                }
            },
            .infix => |infix| {
                std.debug.assert(infix.op == .single_quote);
                const lhs = self.eval_expr(infix.lhs);
                const rhs = self.eval_expr(infix.rhs);
                return Val {.note = PreNote {.deg = lhs.num, .duration = @enumFromInt(rhs.num - 1)}};
            },
            .ident => @panic("TODO"),
            .sec => unreachable,
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
        assert(ast.toplevels.len == 1);
        assert(ast.secs.len == 1);

        const main_formal = anno.main_formal;
        assert(main_formal == ast.toplevels[0]);
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
        const scale = Tonality.Scale.MiddleCMajor;
        const note_exprs = sec.notes;
        var gap: f32 = 0;
        //for (note_exprs) |note_expr| {
        //    const note = exprs[@intCast(note_expr)].data.note;
        //    const dots = note.dots;
        //    var i: u32 = 0; 
        //    const dura: Zynth.NoteDuration = @enumFromInt(dots);
        //    while (self.eval_lazy_head(note.expr, i)) |pitch_expr_next|: (i += 1) {
        //        const deg = self.eval_strict(pitch_expr_next);
        //        const abs_pitch = abspitch_from_scale(scale, @intCast(deg.degree));
        //        const freq = Zynth.pitch_to_freq(abs_pitch+deg.shift-24);
        //        // the first one should have gap equal to the duration of the previous note
        //        // the rest should have zero
        //        self.queue.push(Note {.freq = freq, .duration = dura.to_sec(bpm), .gap = gap }); 
        //        gap = 0;
        //    }
        //    gap = dura.to_sec(bpm);
        //    
        //}
        for (note_exprs) |note_expr| {
            const pre_note = self.eval_expr(note_expr).note;
            const abspitch = scale.get_abspitch(@intCast(pre_note.deg));
            const freq = Tonality.abspitch_to_freq(abspitch);
            const note = Note {.freq = freq, .duration = pre_note.duration.to_sec(bpm), .gap = gap};
            gap = note.duration;
            self.queue.push(note);
        }
        self.queue.push(Note.eof(gap));
    }

    pub fn get(self: *Evaluator) Note {
        return self.queue.pull();
    } };

