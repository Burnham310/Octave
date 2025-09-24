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
const Instrument = @import("instrument.zig");

const Zynth = @import("zynth");

pub const Note = struct {
    content: Content,
    duration: f32,
    gap: f32, // gap since last note
    amp: f32,
    inst: Instrument,

    pub const Content = union(enum) {
        i: i32,
        u: u32,
        f: f32,
    };

    pub fn is_eof(self: Note) bool {
        return self.duration == 0;
    }

    pub fn eof(gap: f32) Note {
        return .{ .content = .{ .i = 0 },. duration = 0, .gap = gap, .amp = 0, .inst = Instrument.dummy };
    }
};

const Fraction = struct {
    numerator: i32,
    dominator: i32,

    pub const zero = Fraction {.numerator = 0, .dominator = 1 };

    pub fn to_float(self: Fraction) f32 {
        return @as(f32, @floatFromInt(self.numerator)) / @as(f32, @floatFromInt(self.dominator));
    }

    pub fn to_sec(self: Fraction, bpm: f32) f32 {
        return self.to_float() * 4 * (60/bpm);
    }

    pub fn lcm(a: i32, b: i32) i32 {
        const big = @max(a, b);
        const small = @min(a, b);
        var i: i32 = big;
        while (big < a*b+1): (i += big) {
            if (@mod(i, small) == 0)
                return i;
        }
        unreachable;
    }

    pub fn add(a: Fraction, b: Fraction) Fraction {
        const new_dom = lcm(a.dominator, b.dominator); 
        const new_a = new_dom / a.dominator * a.numerator;
        const new_b = new_dom / b.dominator * b.numerator;
        return .{.numerator = new_a + new_b, .dominator = new_dom};
    }

    pub fn is_greater(a: Fraction, b: Fraction) bool {
        return a.numerator * b.dominator > b.numerator * b.dominator;
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
pub const SectionEvaluator = struct {
    ast: *Ast,
    anno: *Sema.Annotations,
    content_ty: Type,
    content_ty_full: TypePool.TypeFull,
    sec: *Ast.Expr,

    // state
    its: []u32,
    gap: f32 = 0,
    first_in_expr: bool = true,
    config: SectionConfig = .{},

    peek_buf: ?Note = null,

    instrument: Instrument,

    pub fn reset_all(self: *SectionEvaluator) void {
        @memset(self.its, 0);
        self.gap = 0;
        self.first_in_expr = true;
        self.peek_buf = null;
    }

    pub fn init(ast: *Ast, anno: *Sema.Annotations, sec: *Ast.Expr, a: Allocator) SectionEvaluator {
        const its = a.alloc(u32, anno.expr_extras.len) catch unreachable;
        const sec_anno = anno.expr_extras[sec.anno_extra];
        const sec_ty = sec_anno.ty;
        const sec_content_ty = TypePool.lookup(sec_ty).section;

        @memset(its, 0);
        return .{
            .ast = ast,
            .anno = anno,
            .content_ty = sec_content_ty, 
            .content_ty_full = TypePool.lookup(sec_content_ty),
            .sec = sec,
            .its = its,
            .instrument = Instrument.dummy,
        };
    }

    pub const InstrumentNo = enum(u8) {
        guitar,
        sine,
        triangle,
        drum,
    };

    const SectionConfig = struct {
        scale: Tonality.Scale = .MiddleCMajor,
        bpm: f32 = 120,
        tempo: Fraction = .{.numerator = 4, .dominator = 4},
        volume: f32 = 1,
        inst: InstrumentNo = .guitar,
    };

    const PreNote = struct {
        pitch: NotePitch,
        duration: Fraction,
        gap: Fraction,
    };

    const NotePitch = struct {
        deg: i32,
        shift: i32,
        amp: f32 = 1,
        parallel: bool, pub const zero = NotePitch {.deg = 1, .shift = 0, .amp = 0, .parallel = false};
    };

    const Val = union(enum) {
        num: isize,
        pitch: NotePitch,
        frac: Fraction,
        note: PreNote,
    };

    pub fn expr_implicit_cast(val: Val, to: Type, duration_infer: Fraction) Val {
        switch (val) {
            .num => |i| {
                if (to == Type.pitch) 
                    return Val { .pitch = .{ .deg = @intCast(i), .shift = 0, .parallel = false } };
                if (to == Type.pitch_note) 
                    return Val { 
                        .note = .{ 
                            .pitch = .{ .deg = @intCast(i), .shift = 0, .parallel = false }, 
                            .duration = duration_infer, 
                            .gap = duration_infer 
                        }
                    };

            },
            .pitch => |p| {
                if (to == Type.pitch_note) 
                    return Val {.note = .{.pitch = p, .duration = duration_infer, .gap = if (p.parallel) .zero else duration_infer }};
            },
            else => {},
        }
        return val;
    }

    pub fn eval_expr_strict(self: *SectionEvaluator, expr: *Ast.Expr, duration_infer: Fraction) Val {
        const val = self.eval_expr_strict_impl(expr, duration_infer);
        const ty = self.anno.expr_extras[expr.anno_extra].ty;
        return expr_implicit_cast(val, ty, duration_infer);
    }

    pub fn eval_expr_strict_impl(self: *SectionEvaluator, expr: *Ast.Expr, duration_infer: Fraction) Val {
        const extra = self.anno.expr_extras[expr.anno_extra]; 
        const t_full = TypePool.lookup(extra.ty);
        _ = t_full;
        switch (expr.data) {
            .num => |i| return Val {.num = i},
            .prefix => @panic("TODO"),
            .infix => |infix| {
                const lhs = self.eval_expr_strict(infix.lhs, duration_infer);
                const rhs = self.eval_expr_strict(infix.rhs, duration_infer);
                switch (infix.op) {
                    .single_quote => {
                        return Val { .note = PreNote { .pitch = lhs.pitch, .duration = rhs.frac, .gap = @panic("TODO") } };
                    },
                    .slash => {
                        return Val { .frac = .{ .numerator = @intCast(lhs.num), .dominator = @intCast(rhs.num) } };  
                    },
                    .plus => return Val { .num = lhs.num + rhs.num },
                    .minus => return Val { .num = lhs.num - rhs.num },
                    .times => return Val { .num = lhs.num * rhs.num },
                    .le => return Val { .num = @intFromBool(lhs.num < rhs.num) },
                    .ge => return Val { .num = @intFromBool(lhs.num > rhs.num) },
                    .geq => return Val { .num = @intFromBool(lhs.num >= rhs.num) },
                    .leq => return Val { .num = @intFromBool(lhs.num <= rhs.num) },
                    .eq => return Val { .num = @intFromBool(lhs.num == rhs.num) },
                    else => unreachable,
                }
                
            },
            .list, .sequence => @panic("TODO"),
            .ident => |ident| {
                if (self.anno.builtin_vars.get(ident.sym)) |builtin| {
                    return Val { .num = @intCast(builtin[1]) };
                }
                // std.log.debug("ident: {}", .{ident.sema_expr});
                const var_loc = extra.data.formal_ref;
                switch (var_loc) {
                    .decl => |formal| 
                        return self.eval_expr_strict(formal.expr, duration_infer),
                    .builtin => |typed_value|
                        return Val { .num = @intCast(typed_value[1]) },
                }
            },
            .sec => unreachable,
            .@"for" => @panic("TODO"),
            .@"if" => |@"if"|{
                const cond = self.eval_expr_strict(@"if".cond, .zero).num;
                if (cond != 0) return self.eval_expr_strict(@"if".then, duration_infer)
                else return self.eval_expr_strict(@"if".@"else", duration_infer);
            },
            .tag_decl, .tag_use => unreachable,
        }  
    }

    pub fn reset(self: *SectionEvaluator, expr: *Ast.Expr) void {
        self.its[expr.anno_extra] = 0;
        const extra = self.anno.expr_extras[expr.anno_extra]; 
        switch (expr.data) {
            .num => {},
            .ident => {
                switch (extra.data.formal_ref) {
                    .decl => |formal| self.reset(formal.expr),
                    .builtin => {},
                }
            },
            .sec => unreachable,
            .prefix => |prefix| self.reset(prefix.rhs),
            .infix => |infix| {
                self.reset(infix.lhs);
                self.reset(infix.rhs);
            },
            .list, .sequence => |list| for (list.els) |el| self.reset(el),
            .@"for" => |@"for"| for (@"for".body) |el| self.reset(el),
            .@"if" => |@"if"| {
                self.reset(@"if".then);
                self.reset(@"if".@"else");
            },
            .tag_decl, .tag_use => unreachable,
        }
    }

    pub fn eval_expr(self: *SectionEvaluator, expr: *Ast.Expr, duration_infer: Fraction) ?Val {
        const val = self.eval_expr_impl(expr, duration_infer) orelse return null;
        const ty = self.anno.expr_extras[expr.anno_extra].ty;
        return expr_implicit_cast(val, ty, duration_infer);
    }

    pub fn eval_expr_impl(self: *SectionEvaluator, expr: *Ast.Expr, duration_infer: Fraction) ?Val {
        const extra = self.anno.expr_extras[expr.anno_extra];
        const ty = extra.ty;
        const t_full = TypePool.lookup(ty);
        _ = t_full;
        const it = &self.its[expr.anno_extra]; 
        switch (expr.data) {
            .num => |i| {
                if (it.* > 0) return null;
                it.* += 1;
                return Val { .num = @intCast(i) };
            },
            .prefix => |prefix| {
                if (it.* > 0) return null;
                var pitch = (self.eval_expr(prefix.rhs, duration_infer) orelse {
                    it.* += 1;
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
                if (it.* > 0) return null;
                switch (infix.op) {
                    .plus, .minus => {
                        it.* += 1;
                        return self.eval_expr_strict(expr, duration_infer);
                    },
                    else => {},
                }
                const rhs = self.eval_expr_strict(infix.rhs, .zero).frac;
                const lhs = self.eval_expr(infix.lhs, rhs) orelse {
                    it.* += 1;
                    return null;
                };
                switch (infix.op) {
                    .single_quote => {
                        return Val {.note = PreNote {.pitch = lhs.pitch, .duration = rhs, .gap = if (lhs.pitch.parallel) .zero else rhs }};
                    },
                    .slash => {
                        @panic("unreachable");
                        // return Val {.frac = .{.numerator = @intCast(lhs.num), .dominator = @intCast(rhs.num) }};  
                    },
                    else => unreachable,
                }
                
            },
            .list => |list| {
                while (it.* < list.els.len): (it.* += 1) {
                    var val = self.eval_expr(list.els[it.*], duration_infer) orelse continue;
                    val.pitch.parallel = true;
                    return val;
                }
                if (it.* == list.els.len) {
                    it.* += 1;
                    return Val {.pitch = .{.deg = 1, .shift = -999, .amp = 0, .parallel = false}};
                }
                return null;
            },
            .sequence => |seq| {
                while (it.* < seq.els.len): (it.* += 1) {
                    const body_expr = seq.els[it.*];
                    const val = self.eval_expr(body_expr, duration_infer) orelse {
                        self.reset(body_expr);
                        continue;
                    }; 
                    return val;
                }
                return null;

            },
            .ident => {
                const var_loc = extra.data.formal_ref;
                switch (var_loc) {
                    .decl => |formal| 
                        return self.eval_expr(formal.expr, duration_infer) orelse {
                            self.reset(formal.expr);
                            return null;
                        },
                    .builtin => |typed_value| 
                        if (it.* == 0) { 
                            it.* += 1;
                            return Val { .num = @intCast(typed_value[1]) };
                        }
                        else return null,
                }
            },
            .sec => unreachable,
            .@"for" => |@"for"| {
                const lhs = self.eval_expr_strict(@"for".lhs, .zero).num;
                const rhs = self.eval_expr_strict(@"for".rhs, .zero).num;
                const times = rhs - lhs;
                if (times < 0) @panic("for loop rhs < lhs");
                while (it.* < @"for".body.len * @as(usize, @intCast(times))): (it.* += 1) {
                    if (@"for".with) |with| {
                        with.expr.data.num = @intCast(@as(isize, @intCast(it.* / @"for".body.len)) + lhs);
                    }
                    const body_expr = @"for".body[it.* % @"for".body.len];
                    const val = self.eval_expr(body_expr, duration_infer) orelse {
                        self.reset(body_expr);
                        continue;
                    }; 
                    return val;
                }
                return null;
            },
            .@"if" => |@"if"|{
                const cond = self.eval_expr_strict(@"if".cond, .zero).num;
                if (cond != 0) return self.eval_expr(@"if".then, duration_infer)
                else return self.eval_expr(@"if".@"else", duration_infer);
            },
            .tag_decl, .tag_use => unreachable,
        }  
    } 

    pub fn eval_config(self: *SectionEvaluator, a: Allocator) void {
        const config = &self.config;
        // const config_tys = self.anno.expr_extras[self.sec.anno_extra].config_tys;
        for (self.sec.data.sec.config) |formal| {
            if (formal.ident == BS.tonic) config.scale.tonic = @enumFromInt(self.eval_expr_strict(formal.expr, .zero).pitch.deg)
            else if (formal.ident == BS.octave) config.scale.octave = @intCast(self.eval_expr_strict(formal.expr, .zero).num)
            else if (formal.ident == BS.mode) config.scale.mode = @enumFromInt(self.eval_expr_strict(formal.expr, .zero).num)
            else if (formal.ident == BS.bpm) config.bpm = @floatFromInt(self.eval_expr_strict(formal.expr, .zero).num)
            else if (formal.ident == BS.tempo) config.tempo = self.eval_expr_strict(formal.expr, .zero).frac
            else if (formal.ident == BS.instrument) {
                const inst_no = self.eval_expr_strict(formal.expr, .zero).num;
                config.inst = std.enums.fromInt(InstrumentNo, inst_no)
                    orelse @panic("TODO: handle error");
           
            }
            else if (formal.ident == BS.instrument) {} 
            else if (formal.ident == BS.volume) config.volume = @as(f32, @floatFromInt(self.eval_expr_strict(formal.expr, .zero).num)) / 100.0
            else unreachable;
        }
        switch (config.inst) {
            .guitar => {
                const guitar = a.create(Instrument.AcousticGuitar) catch unreachable;
                const random = a.create(std.Random.Xoroshiro128) catch unreachable;
                random.* = .init(0);
                guitar.* = Instrument.AcousticGuitar { .random = random.random() };
                self.instrument = Instrument.make(Instrument.AcousticGuitar, guitar);
            },
            .sine => {
                const midi_keyboard = a.create(Instrument.MidiKeyboard) catch unreachable;
                midi_keyboard.* = Instrument.MidiKeyboard { .shape = .Sine };
                self.instrument = Instrument.make(Instrument.MidiKeyboard, midi_keyboard);

            },
            .triangle => {
                const midi_keyboard = a.create(Instrument.MidiKeyboard) catch unreachable;
                midi_keyboard.* = Instrument.MidiKeyboard { .shape = .Triangle };
                self.instrument = Instrument.make(Instrument.MidiKeyboard, midi_keyboard);
            },
            .drum => {
                const drum_set = a.create(Instrument.DrumSet) catch unreachable;
                drum_set.* = Instrument.DrumSet { .mixer = .{}, .a = a };
                self.instrument = Instrument.make(Instrument.DrumSet, drum_set);
            },
        }
    }

    pub fn peek(self: *SectionEvaluator) Note {
        if (self.peek_buf) |note| return note;
        self.peek_buf = self.eval();
        return self.peek_buf.?;
    }

    pub fn eval(self: *SectionEvaluator) Note {
        if (self.peek_buf) |peek_buf| {
            self.peek_buf = null;
            return peek_buf;
        }
        const sec = self.sec.data.sec;

        const config = &self.config;
        const note_exprs = sec.notes;
        
        const default_dura = Fraction { .numerator = 1, .dominator = config.tempo.dominator };
        const it = &self.its[self.sec.anno_extra]; 
        while (it.* < note_exprs.len): (it.* += 1) {
            while (self.eval_expr(note_exprs[it.*], default_dura)) |val| {
                // std.log.debug("val: {}", .{val});
                // const note_content = ) 
                const pre_note = expr_implicit_cast(val, Type.pitch_note, default_dura).note;
                const note_content = switch (self.content_ty_full) {
                    .pitch, .int => blk: {
                        const abspitch = config.scale.get_abspitch(@intCast(pre_note.pitch.deg)) + pre_note.pitch.shift;
                        break :blk Note.Content { .i = @intCast(abspitch) };
                    },
                    .sym_set => Note.Content { .u = @intCast(pre_note.pitch.deg) },
                    else => unreachable,
                };

                       // const freq = Tonality.abspitch_to_freq(abspitch);
                const note = Note {
                    .content = note_content,
                    .duration = pre_note.duration.to_sec(config.bpm),
                    .gap = pre_note.gap.to_sec(config.bpm),
                    .amp = pre_note.pitch.amp * config.volume,
                    .inst = self.instrument,
                };
                self.first_in_expr = false;
                self.gap = note.duration;
                // std.log.debug("{}", .{note});
                return note;
            }
            self.first_in_expr = true;
        }
        return Note.eof(0);
    }
};

pub const SectionProgress = struct {
    eval: SectionEvaluator,
    t: f32 = 0, // TODO: switch to fraction?
};
pub const Evaluator = struct {
    sec_evals: []SectionProgress,
    last_eof: Note = undefined,
    t: f32 = 0,
     
    pub fn init(ast: *Ast, anno: *Sema.Annotations, a: Allocator) Evaluator {
        var sec_evals: []SectionProgress = undefined;
        switch (anno.main_formal.expr.data) {
            .list => |list| {
                sec_evals = a.alloc(SectionProgress, list.els.len) catch unreachable;
                for (list.els, 0..) |expr, i| {
                    assert(expr.data == .ident);
                    const extra = anno.expr_extras[expr.anno_extra];
                    const sema_expr = extra.data.formal_ref.decl.expr;
                    assert(sema_expr.data == .sec);
                    sec_evals[i] = .{.eval = SectionEvaluator.init(ast, anno, sema_expr, a)};
                    sec_evals[i].eval.eval_config(a);
                }
            },
            .sec => {
                sec_evals = a.alloc(SectionProgress, 1) catch unreachable;
                sec_evals[0] = SectionProgress {.eval = SectionEvaluator .init(ast, anno, anno.main_formal.expr, a)};
                sec_evals[0].eval.eval_config(a);
            },
            else => unreachable,
        }
        return Evaluator {.sec_evals = sec_evals};
    }

    fn peek(self: *Evaluator) struct {f32, usize} {
        var latest_next = std.math.floatMax(f32); 
        var latest_next_index: usize = std.math.maxInt(usize);

        for (self.sec_evals[0..], 0..) |*prog, i| {
            const note = prog.eval.peek();
            if (note.is_eof()) {
                self.last_eof = note;
                continue;
            }
            if (prog.t < latest_next) {
                latest_next = prog.t;
                latest_next_index = i;
            }
        }
        return .{latest_next, latest_next_index};
    }
    
    pub fn eval(self: *Evaluator) Note {
        if (self.sec_evals.len == 1) return self.sec_evals[0].eval.eval();

        const latest_next, const latest_next_index = self.peek(); 
        if (latest_next_index == std.math.maxInt(usize)) return self.last_eof;
        var latest_note = self.sec_evals[latest_next_index].eval.eval();
        self.sec_evals[latest_next_index].t += latest_note.gap;

        const latest_next2, const latest_next_index2 = self.peek(); 
        if (latest_next_index2 != std.math.maxInt(usize))
            latest_note.gap = latest_next2 - latest_next;
        return latest_note;
    }

    pub fn reset(self: *Evaluator) void {
        for (self.sec_evals) |*sec| {
            sec.t = 0;
            sec.eval.reset_all();
        }
        self.t = 0;
    }
};
