const std = @import("std");

const Zynth = @import("zynth");
const Streamer = Zynth.Streamer;
const Eval = @import("evaluator.zig");
const Config = Zynth.Config;

const Player = @This();

evaluator: *Eval.Evaluator,
frame_ct: u32 = 0, // frame since last source played
sub_streamer: ?Streamer = null,

a: std.mem.Allocator,


fn read(ptr: *anyopaque, frames: []f32) struct { u32, Streamer.Status } {
    const create = Zynth.Audio.create;
    const self: *Player = @alignCast(@ptrCast(ptr));
    
    const frame_len: u32 = @intCast(frames.len);
    var off: u32 = 0;
    while (off < frame_len) {
        if (self.frame_ct == 0) {
            const note = self.evaluator.get() orelse return .{ off, .Stop };
            const sine = create(self.a, Zynth.Waveform.Simple.init(0.5, note.freq, .Triangle));
            const envelop = create(self.a, Zynth.Envelop.Envelop.init(
                    self.a.dupe(f32, &.{0.05, note.duration - 0.1, 0.05}) catch unreachable,
                    self.a.dupe(f32, &.{0, 1, 1, 0}) catch unreachable,
                    sine.streamer()
            ));

            self.sub_streamer = envelop.streamer();
            self.frame_ct = @as(u32, @intFromFloat(note.duration * Config.SAMPLE_RATE));
        }
        var end: u32 = frame_len;
        if (self.frame_ct > frame_len) { // read as much as possible
            self.frame_ct -= frame_len; 
        } else {
            end = self.frame_ct;
            self.frame_ct = 0;
        }
        if (self.sub_streamer) |sub_streamer| _, _ = sub_streamer.read(frames[off..end]);
        off = end;

    }
    return .{ frame_len, .Continue };
}

fn reset(ptr: *anyopaque) bool {
    const self: *Player = @alignCast(@ptrCast(ptr));
    self.frame_ct = 0;
    self.sub_streamer = null;

    return true;
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

