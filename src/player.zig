const std = @import("std");
const Allocator = std.mem.Allocator;

const Zynth = @import("zynth");
const Streamer = Zynth.Streamer;
const Eval = @import("evaluator.zig");
const Config = Zynth.Config;

const Player = @This();

evaluator: *Eval.Evaluator,
volume: f32,
frame_ct: u32 = 0, // frame since last source played
peak: ?Eval.Note = null,

streamers: std.DoublyLinkedList = .{},

a: std.mem.Allocator,


tmp: [4096]f32 = undefined,

const StreamerNode = struct {
    streamer: Streamer,
    node: std.DoublyLinkedList.Node = .{},

    //pub fn new(streamer: Streamer, a: std.mem.Allocator) *StreamerNode {
    //    const node = a.create(StreamerNode) catch unreachable;
    //    node.streamer = streamer;
    //    node.node = .{};
    //    return node;
    //}
};

pub fn init(eval: *Eval.Evaluator, volume: f32, a: Allocator) Player {
    var player = Player { .evaluator = eval, .volume = volume, .a = a };
    for (eval.sec_evals) |sec_prog| {
        const inst_streamer = sec_prog.eval.instrument.streamer;
        player.add_streamer(inst_streamer);
    }
    return player;
}

pub fn deinit(self: *Player) void {
    var it = self.streamers.first;
    while (it) |node| {
        const next = node.next;
        const sn: *StreamerNode = @fieldParentPtr("node", node);
        self.a.destroy(sn);

        it = next;
    }
}

fn read(ptr: *anyopaque, frames: []f32) struct { u32, Streamer.Status } {
    const self: *Player = @alignCast(@ptrCast(ptr));
  
    const frame_len: u32 = @intCast(frames.len);
    var off: u32 = 0;
    while (off < frame_len) {
        if (self.frame_ct == 0) {
            if (self.peak) |note| {
                if (note.is_eof()) return .{ off, .Stop };
                note.inst.play(note);
                                self.frame_ct = @as(u32, @intFromFloat(self.peak.?.gap * Config.SAMPLE_RATE));
                self.peak = self.evaluator.eval();
            } else {
                self.peak = self.evaluator.eval();
            }
        }
        var end: u32 = frame_len;
        if (self.frame_ct > frame_len - off) { // read as much as possible
            self.frame_ct -= frame_len - off; 
        } else {
            end = off + self.frame_ct;
            self.frame_ct = 0;
        }
        self.mixer_read(frames[off..end]);
        off = end;
    }
     
    return .{ frame_len, .Continue };
}

fn add_streamer(self: *Player, new_streamer: Streamer) void {
    const node = self.a.create(StreamerNode) catch unreachable;
    node.streamer = new_streamer;
    node.node = .{};
    self.streamers.prepend(&node.node);
}

fn mixer_read(self: *Player, frames: []f32) void {
    var it = self.streamers.first;
    while (it) |node| : (it = node.next) {
        const sn: *StreamerNode = @fieldParentPtr("node", node);
        std.debug.assert(self.tmp.len >= frames.len);
        @memset(self.tmp[0..frames.len], 0);
        const len, const status = sn.streamer.read(self.tmp[0..frames.len]);
        for (0..len) |frame_i|
            frames[frame_i] += self.tmp[frame_i];
        if (status == .Stop) {
            it = node.prev;
            self.streamers.remove(node);
            self.a.destroy(sn);
        }
    }

} 

fn reset(ptr: *anyopaque) bool {
    const self: *Player = @alignCast(@ptrCast(ptr));
    self.frame_ct = 0;
    self.peak = null;
    self.evaluator.reset();
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
   
