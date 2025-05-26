const std = @import("std");

const Allocator = std.mem.Allocator;
pub fn ThreadSafeQueue(comptime T: type) type {
    return struct {
        const Self = @This();
        data: []T, // The underlying array, not to be access directly. 
                   // Its len represent capacity
        mtx: std.Thread.Mutex,
        cond: std.Thread.Condition,
        head: u32,
        tail: u32,
        count: u32, // The actual size
        a: Allocator,
        pub fn initCapacity(cap: u32, a: Allocator) ThreadSafeQueue {
            return .{
                .data = a.alloc(T, cap) catch unreachable,
                .mtx = .{},
                .cond = .{},
                .head = 0,
                .tail = 0,
                .count = 0,
                .a = a,
            };
        }
        fn resize(self: *Self) void {
            const old_data = self.data;
            self.data = self.a.alloc(T, 2 * old_data.len) catch unreachable;
            var new_i: u32 = 0;
            var old_i: u32 = 0;
            while (new_i < self.count) {
                self.data[new_i] = old_data[old_i];
                new_i += 1;
                old_i = (old_i + 1) % old_data.count;
            }
            self.a.free(old_data);

            self.head = 0;
            self.tail = new_i;
            return self.data.len;
        }
        pub fn push(self: *Self, el: T) void {
            self.mtx.lock();
            if (self.data.len == self.count) self.resize();
            self.data[self.tail] = el;
            self.tail = (self.tail + 1) % self.data.len;
            self.count += 1;
            self.mtx.unlock();

            self.cond.signal();
        }

        pub fn pull(self: *Self) T {
            self.mtx.lock();
            defer self.mtx.unlock();
            while (self.count == 0)
                self.cond.wait(&self.mtx);
            const head = self.top;
            self.top = (self.top + 1) % self.data.len;
            self.count -= 1;
            return self.data[head];
        }
    };
}
