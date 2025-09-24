const std = @import("std");

const Zynth = @import("zynth");
const ZynthPreset = @import("zynth_preset");
const Streamer = Zynth.Streamer;

const Eval = @import("evaluator.zig");
const Note = Eval.Note;
const Tonality = @import("tonality.zig");

const TypePool = @import("type_pool.zig");
const Type = TypePool.Type;


const Instrument = @This();

const VTable = struct {
    play: *const fn (ptr: *anyopaque, Note) void,
    mute_all: *const fn (ptr: *anyopaque) void,
    // accept_type: *const fn (ptr: *anyopaque, ty: Type) bool, 
    debug_name: *const fn (ptr: *anyopaque) []const u8,
};

vtable: VTable,
ptr: *anyopaque,
streamer: Streamer,

pub fn play(self: Instrument, note: Note) void {
    self.vtable.play(self.ptr, note);
}

pub fn mute_all(self: Instrument) void {
    self.vtable.mute_all(self.ptr);
}

// pub fn accept_type(self: Instrument, ty: Type) bool {
//     return self.vtable.accept_type(ty);
// }

pub fn debug_name(self: Instrument) []const u8 {
    return self.vtable.debug_name(self.ptr);
}

pub fn make(comptime T: type, val: *T) Instrument {
    if (!@hasDecl(T, "play")) {
        @compileError(@typeName(T) ++ " does not have method `play`");
    }
    // const play_type = @typeInfo(@FieldType(T, "play"));
    // if (play_type != .@"fn") {
    //     @compileError("field `play` needs to be a function");
    // }
    
    const wrapper = struct {
        pub fn play(ptr: *anyopaque, note: Note) void {
            const unwrapped: *T = @ptrCast(@alignCast(ptr));
            return unwrapped.play(note);
        }

        pub fn mute_all(ptr: *anyopaque) void {
            const unwrapped: *T = @ptrCast(@alignCast(ptr));
            return unwrapped.mute_all(); 
        }

        pub fn debug_name(ptr: *anyopaque) []const u8 {
            const unwrapped: *T = @ptrCast(@alignCast(ptr));
            return unwrapped.debug_name();
        }
    };

    const streamer = Streamer.make(T, val);
   
    return Instrument { 
        .ptr = @ptrCast(val),
        .vtable = .{ 
            .play = wrapper.play,
            .mute_all = wrapper.mute_all,
            // .accept_type = wrapper.accept_type,
            .debug_name = wrapper.debug_name
        },
        .streamer = streamer
    };
}

pub const MidiKeyboard = struct {
    const Key = struct {
        wave: Zynth.Waveform.Simple = undefined,
        envelop: Zynth.Envelop.Envelop(.{ .static = 4 }) = undefined,
        state: State = .stopped,
        pub const State = enum {
            playing,
            stopped,
        };
    };
    const KeyRange = 128;
    keys: [KeyRange]Key = .{ Key {} } ** KeyRange,
    tmp: [4096]f32 = undefined,
    shape: Zynth.Waveform.Shape,

    pub fn play(self: *MidiKeyboard, note: Note) void {
        const note_num = note.content.i;
        if (note_num < 0 or note_num >= KeyRange) return;
        const key = &self.keys[@intCast(note_num)];
        key.state = .playing;
        key.envelop = .init(
                    .{0.05, note.duration - 0.1, 0.05},
                    .{0, 1, 1, 0},
                    key.wave.streamer());
        key.wave = .init(note.amp, Tonality.abspitch_to_freq(note_num), self.shape);
    }

    pub fn mute_all(self: *MidiKeyboard) void {
        _ = self;
    }

    pub fn debug_name(self: *MidiKeyboard) []const u8 {
        return switch (self.shape) {
            .Sine => "sine-midikeyboard",
            .Triangle => "triangle-midikeyboard",
            .Sawtooth => "sawtooth-midikeyboard",
            .Square => "square-midikeyboard",
        };
    }

    // TODO: implement stop so that this does not run indefinitely
    pub fn read(self: *MidiKeyboard, frames: []f32) struct { u32, Streamer.Status } {
        std.debug.assert(frames.len < self.tmp.len);
        for (0..KeyRange) |i| {
            const key = &self.keys[i];
            if (key.state == .stopped) continue;
            @memset(self.tmp[0..frames.len], 0);
            const len, _ = key.envelop.streamer().read(self.tmp[0..frames.len]);
            for (0..len) |frame_i| {
                frames[frame_i] += self.tmp[frame_i];
            }
        }
        return .{ @intCast(frames.len), .Continue };
    }
};

pub const AcousticGuitar = struct {
    const Key = struct {
        wave: Zynth.Waveform.StringNoise = undefined,
        state: State = .stopped,
        pub const State = enum {
            playing,
            stopped,
        };
    };
    const KeyRange = 128;
    keys: [KeyRange]Key = .{ Key {} } ** KeyRange,
    tmp: [4096]f32 = undefined,
    random: std.Random,

    pub fn play(self: *AcousticGuitar, note: Note) void {
        const note_num = note.content.i;
        if (note_num < 0 or note_num >= KeyRange) return;
        const key = &self.keys[@intCast(note_num)];
        key.state = .playing;
        key.wave = .init(note.amp, Tonality.abspitch_to_freq(note_num), self.random, note.duration);
    }

    pub fn mute_all(self: *AcousticGuitar) void {
        _ = self;
    }
    
    pub fn debug_name(_: *AcousticGuitar) []const u8 {
        return "acoustic-guitar";
    }

    // TODO: implement stop so that this does not run indefinitely
    pub fn read(self: *AcousticGuitar, frames: []f32) struct { u32, Streamer.Status } {
        std.debug.assert(frames.len < self.tmp.len);
        for (0..KeyRange) |i| {
            const key = &self.keys[i];
            if (key.state == .stopped) continue;
            @memset(self.tmp[0..frames.len], 0);
            const len, _ = key.wave.streamer().read(self.tmp[0..frames.len]);
            for (0..len) |frame_i| {
                frames[frame_i] += self.tmp[frame_i];
            }
        }
        return .{ @intCast(frames.len), .Continue };
    }
};

pub const DrumSet = struct {
    pub const Sym = enum {
        S,
        B,
        H,
    };
    mixer: Zynth.Mixer,
    a: std.mem.Allocator,

    pub fn play(self: *DrumSet, note: Note) void {
       const drum_sym: Sym = @enumFromInt(note.content.u);
       switch (drum_sym) {
            .S => {
                self.mixer.play(ZynthPreset.Drum.snare(self.a) catch unreachable);
            },
            .B => {
                self.mixer.play(ZynthPreset.Drum.bass(self.a) catch unreachable);
            },
            .H => {
                self.mixer.play(ZynthPreset.Drum.close_hi_hat(self.a) catch unreachable);
            }
       }
    }

    pub fn mute_all(self: *DrumSet) void {
        _ = self;
    }

    pub fn debug_name(_: *DrumSet) []const u8 {
        return "acoustic-guitar";
    }

    // TODO: implement stop so that this does not run indefinitely
    pub fn read(self: *DrumSet, frames: []f32) struct { u32, Streamer.Status } {
        return self.mixer.streamer().read(frames);
    }
};

pub const Dummy = struct {
    pub fn play(_: *Dummy, _: Note) void {
        @panic("Dummy Instrument");
    }

    pub fn mute_all(_: *Dummy) void {
        @panic("Dummy Instrument");
    }

    pub fn debug_name(_: *Dummy) []const u8 {
        return "dummy";
    }

    // TODO: implement stop so that this does not run indefinitely
    pub fn read(_: *Dummy, _: []f32) struct { u32, Streamer.Status } {
        @panic("Dummy Instrument");
    }
};
var dummy_ = Dummy {};
pub const dummy = make(Dummy, &dummy_);
