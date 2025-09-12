const std = @import("std");

const zynth = @import("zynth");
const Streamer = zynth.Streamer;

const Eval = @import("evaluator.zig");
const Note = Eval.Note;
const Tonality = @import("tonality.zig");


const Instrument = @This();

const VTable = struct {
    play: *const fn (ptr: *anyopaque, Note) void,
    mute_all: *const fn (ptr: *anyopaque) void,
};

vtable: VTable,
ptr: *anyopaque,

pub fn play(self: Instrument, note: Note) void {
    self.vtable.play(self.ptr, note);
}

pub fn mute_all(self: Instrument) void {
    self.vtable.mute_all(self.ptr);
}

pub fn make_instrument(comptime T: type, val: *T) Instrument {
    if (!@hasDecl(T, "play")) {
        @compileError(@typeName(T) ++ " does not have method `play`");
    }
    const play_type = @typeInfo(@FieldType(T, "play"));
    if (play_type != .@"fn") {
        @compileError("field `play` needs to be a function");
    }
    
    const wrapper = struct {
        pub inline fn play(ptr: *anyopaque, note: Note) void {
            const unwrapped: *T = @ptrCast(@alignCast(ptr));
            return unwrapped.play(note);
        }

        pub inline fn mute_all(ptr: *anyopaque) void {
            const unwrapped: *T = @ptrCast(@alignCast(ptr));
            return unwrapped.mute_all(); 
        }
    };
   
    return Instrument { .ptr = @ptrCast(val), .vtable = .{ .play = wrapper.play, .mute_all = wrapper.mute_all } };
}

pub const MidiKeyboard = struct {
    const Key = struct {
        wave: zynth.Waveform.Simple = undefined,
        envelop: zynth.Envelop.Envelop = undefined,
        state: State = .stopped,
        pub const State = enum {
            playing,
            stopped,
        };
    };
    const KeyRange = 52;
    keys: [KeyRange]Key,
    tmp: [4096]f32 = undefined,

    pub fn play(self: *MidiKeyboard, note: Note) void {
        if (note.note_num < 0 or note.note_num >= KeyRange) return;
        const key = &self.keys[note.note_num];
        key.state = .playing;
        key.envelop = .init(
                    &.{0.05, note.duration - 0.1, 0.05},
                    &.{0, 1, 1, 0},
                    key.wave.streamer());
        key.wave = .init(note.amp, Tonality.abspitch_to_freq(note.note_num), .sine);
    }

    pub fn read(self: *MidiKeyboard, frames: []f32) void {
        std.debug.assert(frames.len < self.tmp.len);
        for (0..self.keys) |i| {
            const key = &self.keys[i];
            if (key.state == .stopped) continue;
            key.wave.streamer().read(self.tmp[0..frames.len]);
            
        } 
    }
};

pub const AcousticGuitar = struct {
    
};
