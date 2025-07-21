const std = @import("std");
const Allocator = std.mem.Allocator;
const assert = std.debug.assert;

const Ast = @import("ast.zig");
const Sema = @import("sema.zig");
const TypePool = @import("type_pool.zig");
const Type = TypePool.Type;
const Tonality = @import("tonality.zig");
const ThreadSafeQueue = @import("thread_safe_queue.zig").ThreadSafeQueue;
const BS = @import("lexer.zig").BuiltinSymbols;

const Zynth = @import("zynth");

pub const Note = struct {
    freq: f32,
    duration: f32,
    gap: f32, // gap since last note
    amp: f32,

    pub fn is_eof(self: Note) bool {
        return self.freq == 0 and self.duration == 0;
    }

    pub fn eof(gap: f32) Note {
        return .{ .freq = 0,. duration = 0, .gap = gap, .amp = 0 };
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

    const SectionConfig = struct {
        scale: Tonality.Scale = .MiddleCMajor,
        bpm: f32 = 120,
        tempo: Fraction = .{.numerator = 4, .dominator = 4},
    };

    const PreNote = struct {
        pitch: NotePitch,
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

    const NotePitch = struct {
        deg: i32,
        shift: i32,
        amp: f32 = 1,
    };

    const Val = union(enum) {
        num: isize,
        pitch: NotePitch,
        frac: Fraction,
        note: PreNote,
    };

    // only works on number
    pub fn eval_expr_strict(self: *Evaluator, expr: *Ast.Expr) Val {
        const t_full = TypePool.lookup(expr.ty);
        _ = t_full;
        switch (expr.data) {
            .num => |i| return Val {.num = i},
            .prefix => @panic("TODO"),
            .infix => |infix| {
                const lhs = self.eval_expr_strict(infix.lhs);
                const rhs = self.eval_expr_strict(infix.rhs);
                switch (infix.op) {
                    .single_quote => {
                        return Val {.note = PreNote {.pitch = lhs.pitch, .duration = rhs.frac}};
                    },
                    .slash => {
                        return Val {.frac = .{.numerator = @intCast(lhs.num), .dominator = @intCast(rhs.num) }};  
                    },
                    else => unreachable,
                }
                
            },
            .list => unreachable,
            .ident => |ident| {
                return Val {.num = self.anno.builtin_vars.get(ident).?[1]};
            },
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
                if (expr.ty == Type.int) return Val {.num = @intCast(i)};
                if (expr.ty == Type.pitch) return Val {.pitch = .{.deg = @intCast(i), .shift = 0}}
                else unreachable;
            },
            .prefix => |prefix| {
                if (expr.i > 0) return null;
                var pitch = (self.eval_expr(prefix.rhs) orelse {
                    expr.i += 1;
                    return null;
                }).pitch;
                switch (prefix.op) {
                    .power => pitch.shift += 12,
                    .period => pitch.shift -= 12,
                    .hash => pitch.shift += 1,
                    .tilde => pitch.shift -= 1,
                    else => unreachable,
                } 
                return Val {.pitch = pitch};
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
                        return Val {.note = PreNote {.pitch = lhs.pitch, .duration = rhs.frac}};
                    },
                    .slash => {
                        return Val {.frac = .{.numerator = @intCast(lhs.num), .dominator = @intCast(rhs.num) }};  
                    },
                    else => unreachable,
                }
                
            },
            .list => |list| {
                if (expr.ty == Type.@"void" and expr.i == 0) {
                    expr.i += 1;
                    return Val {.pitch = .{.deg = 1, .shift = 0, .amp = 0}};
                }
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
        // Temporay Implementation: assume it only has one declaration, which is main.
        // main MUST be a single section.
        // The section cannot contain any declaration.
        // It cannot contain any chord, but only single notes
        assert(ast.toplevels.len == 1);
        assert(ast.secs.len == 1);

        const main_formal = anno.main_formal;
        assert(main_formal == ast.toplevels[0]);
        const sec = main_formal.expr.data.sec;

        var config = SectionConfig {};
        for (sec.config) |formal| {
            if (formal.ident == BS.tonic) config.scale.tonic = @enumFromInt(self.eval_expr_strict(formal.expr).num)
            else if (formal.ident == BS.octave) config.scale.octave = @intCast(self.eval_expr_strict(formal.expr).num)
            else if (formal.ident == BS.mode) config.scale.mode = @enumFromInt(self.eval_expr_strict(formal.expr).num)
            else if (formal.ident == BS.bpm) config.bpm = @floatFromInt(self.eval_expr_strict(formal.expr).num)
            else if (formal.ident == BS.tempo) config.tempo = self.eval_expr_strict(formal.expr).frac
            else unreachable;
        }
        const note_exprs = sec.notes;
        
        var gap: f32 = 0;
        const default_dura = Fraction {.numerator = 1, .dominator = config.tempo.dominator};
        for (note_exprs) |note_expr| {
            var first = true;
            while (self.eval_expr(note_expr)) |val| {
                const pre_note = switch (val) {
                    .pitch => |pitch| PreNote  {.pitch = pitch, .duration = default_dura},
                    .note => val.note,
                    else => |t| @panic(@tagName(t)),
                };

                const abspitch = config.scale.get_abspitch(@intCast(pre_note.pitch.deg)) + pre_note.pitch.shift;
                const freq = Tonality.abspitch_to_freq(abspitch);
                const note = Note {
                    .freq = freq,
                    .duration = pre_note.duration.to_sec(config.bpm),
                    .gap = if (first) gap else 0,
                    .amp = pre_note.pitch.amp
                };
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

