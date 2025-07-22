const std = @import("std");

const Zynth = @import("zynth");
const Streamer = Zynth.Streamer;
const Eval = @import("evaluator.zig");
const Config = Zynth.Config;

const Player = @This();

evaluator: *Eval.Evaluator,
volume: f32,
frame_ct: u32 = 0, // frame since last source played
peak: ?Eval.Note = null,
mixer: Zynth.Mixer = .{},

a: std.mem.Allocator,

var random = std.Random.Xoroshiro128.init(0);


fn read(ptr: *anyopaque, frames: []f32) struct { u32, Streamer.Status } {
    const create = Zynth.Audio.create;
    const self: *Player = @alignCast(@ptrCast(ptr));
  
    const frame_len: u32 = @intCast(frames.len);
    var off: u32 = 0;
    while (off < frame_len) {
        if (self.frame_ct == 0) {
            if (self.peak) |note| {
                if (note.is_eof()) return .{ off, .Stop };
                const sine = create(self.a, Zynth.Waveform.StringNoise.init(note.amp * 0.5 * self.volume, note.freq, random.random()));
                //const envelop = create(self.a, Zynth.Envelop.Envelop.init(
                //        self.a.dupe(f32, &.{0.05, note.duration - 0.1, 0.05}) catch unreachable,
                //        self.a.dupe(f32, &.{0, 1, 1, 0}) catch unreachable,
                //        sine.streamer()
                //));
                self.mixer.play(sine.streamer());
            }
            self.peak = self.evaluator.get();
            self.frame_ct = @as(u32, @intFromFloat(self.peak.?.gap * Config.SAMPLE_RATE));
        }
        var end: u32 = frame_len;
        if (self.frame_ct > frame_len - off) { // read as much as possible
            self.frame_ct -= frame_len - off; 
        } else {
            end = off + self.frame_ct;
            self.frame_ct = 0;
        }
        _ = self.mixer.streamer().read(frames[off..end]);
        off = end;
    }
     
    return .{ frame_len, .Continue };
}

fn reset(ptr: *anyopaque) bool {
    const self: *Player = @alignCast(@ptrCast(ptr));
    self.frame_ct = 0;
    self.peak = null;
    const mixer_stream = self.mixer.streamer();
    return mixer_stream.reset();
}

pub fn streamer(self: *Player) Streamer {
    return .{
        .ptr = @ptrCast(self),
        .vtable = .{
            .read = read,
            .reset = reset,
        }
    };
}
   
