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
        duration: Fraction,
    };

    const Fraction = struct {
        numerator: i32,
        dominator: i32,

        pub fn to_float(self: Fraction) f32 {
            return @as(f32, @floatFromInt(self.numerator)) / @as(f32, @floatFromInt(self.dominator));
        }

        pub fn to_sec(self: Fraction, bpm: f32) f32 {
            return self.to_float() * 4 * (60/bpm);
        }
    };

    const Val = union(enum) {
        num: isize,
        frac: Fraction,
        note: PreNote,
    };

    // only works on number
    pub fn eval_expr_strict(self: *Evaluator, expr: *Ast.Expr) Val {
        const t_full = TypePool.lookup(expr.ty);
        _ = t_full;
        switch (expr.data) {
            .num => |i| return Val {.num = i},
            .infix => |infix| {
                const lhs = self.eval_expr_strict(infix.lhs);
                const rhs = self.eval_expr_strict(infix.rhs);
                switch (infix.op) {
                    .single_quote => {
                        return Val {.note = PreNote {.deg = lhs.num, .duration = rhs.frac}};
                    },
                    .slash => {
                        return Val {.frac = .{.numerator = @intCast(lhs.num), .dominator = @intCast(rhs.num) }};  
                    },
                    else => unreachable,
                }
                
            },
            .list => unreachable,
            .ident => @panic("TODO"),
            .sec => unreachable,
        }  
    }
    pub fn eval_expr(self: *Evaluator, expr: *Ast.Expr) ?Val {
        const t_full = TypePool.lookup(expr.ty);
        _ = t_full;
        switch (expr.data) {
            .num => |i| {
                if (expr.i > 0) return null;
                expr.i += 1;
                return Val {.num = i};
            },
            .infix => |infix| {
                if (expr.i > 0) return null;
                const lhs = self.eval_expr(infix.lhs) orelse {
                    expr.i += 1;
                    return null;
                };
                const rhs = self.eval_expr_strict(infix.rhs);
                switch (infix.op) {
                    .single_quote => {
                        return Val {.note = PreNote {.deg = lhs.num, .duration = rhs.frac}};
                    },
                    .slash => {
                        return Val {.frac = .{.numerator = @intCast(lhs.num), .dominator = @intCast(rhs.num) }};  
                    },
                    else => unreachable,
                }
                
            },
            .list => |list| {
                while (expr.i < list.els.len): (expr.i += 1) {
                    return self.eval_expr(list.els[expr.i]) orelse continue;
                }
                return null;
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
        var gap: f32 = 0;
        const default_dura = Fraction {.numerator = 1, .dominator = 4};
        for (note_exprs) |note_expr| {
            var first = true;
            while (self.eval_expr(note_expr)) |val| {
                const pre_note = switch (val) {
                    .num => |i| PreNote  {.deg = i, .duration = default_dura},
                    .note => val.note,
                    else => |t| @panic(@tagName(t)),
                };

                const abspitch = scale.get_abspitch(@intCast(pre_note.deg));
                const freq = Tonality.abspitch_to_freq(abspitch);
                const note = Note {.freq = freq, .duration = pre_note.duration.to_sec(bpm), .gap = if (first) gap else 0};
                self.queue.push(note);
                first = false;
                gap = note.duration;
            }
        }
        self.queue.push(Note.eof(gap));
    }

    pub fn get(self: *Evaluator) Note {
        return self.queue.pull();
    } };

