// the new version of our type system. I hope this is better!
const std = @import("std");
const Lexer = @import("lexer.zig");
const Symbol = Lexer.Symbol;
const Allocator = std.mem.Allocator;
pub const Type = packed struct {
    t: u32,
    pub var @"void":    Type = undefined;
    pub var int:        Type = undefined;
    pub var mode:       Type = undefined;
    pub var pitch:      Type = undefined;
    pub var fraction:   Type = undefined;
    pub var note:       Type = undefined;
    pub var chord:      Type = undefined;
    pub var section:    Type = undefined;
};
pub const Decl = u32;

pub const TypeStorage = struct {
    kind: Kind,
    more: u32,
};

pub const Kind = enum(u8) {
    void,
    int,
    mode,
    pitch,
    fraction,
    note,
    section,
    list,
};
pub const TypeFull = union(Kind) {
    void,
    int,
    mode,
    pitch,
    fraction,
    note,
    section,
    list: Type,
  
    //pub const Ptr = struct {
    //    el: Type
    //};
    //pub const Array = struct {
    //    el: Type,
    //    size: u32,
    //};
    //pub const Tuple = struct {
    //    els: []const Type,
    //};
    //pub const Named = struct {
    //    els: []const Type,
    //    syms: []const Symbol,
    //};
    //pub const Function = struct {
    //    args: []const Type,
    //    ret: Type,
    //};
    pub const Adapter = struct {
        extras: *std.ArrayList(u32),
        pub fn eql(ctx: Adapter, a: TypeFull, b: TypeStorage, b_idx: usize) bool {
            _ = b_idx;
            if (std.meta.activeTag(a) != b.kind) return false;
            const extras = ctx.extras.items;
            switch (a) {
                .void,
                .int,
                .mode,
                .pitch,
                .fraction,
                .note,
                .section => return true,
                .list => |el_ty| return el_ty.t == extras[b.more],
            }
        }
        pub fn hash(ctx: Adapter, a: TypeFull) u32 {
            _ = ctx;
            return switch (a) {
                .void,
                .int,
                .mode,
                .pitch,
                .fraction,
                .note,
                .section => std.hash.uint32(@intFromEnum(a)),
                .list => |el_ty| @truncate(std.hash.Wyhash.hash(0, std.mem.asBytes(&el_ty))),
                //inline .ptr, .array => |x| @truncate(std.hash.Wyhash.hash(0, std.mem.asBytes(&x))),
                //.tuple => |tuple| blk: {
                //    var hasher = std.hash.Wyhash.init(0);
                //    std.hash.autoHash(&hasher, std.meta.activeTag(a));
                //    for (tuple.els) |t| {
                //        std.hash.autoHash(&hasher, t);
                //    }
                //    break :blk @truncate(hasher.final());
                //},
                //.named => |named| blk: {
                //    var hasher = std.hash.Wyhash.init(0);
                //    std.hash.autoHash(&hasher, std.meta.activeTag(a));
                //    for (named.syms, named.els) |sym, t| {
                //        std.hash.autoHash(&hasher, sym);
                //        std.hash.autoHash(&hasher, t);
                //    }
                //    break :blk @truncate(hasher.final());

                //},
                //.function => |function| blk: {
                //    var hasher = std.hash.Wyhash.init(0);
                //    std.hash.autoHash(&hasher, std.meta.activeTag(a));
                //    std.hash.autoHash(&hasher, function.ret);
                //    for (function.args) |t| {
                //        std.hash.autoHash(&hasher, t);
                //    }
                //    break :blk @truncate(hasher.final());
                //},
            };
        }
    };
    pub fn format(value: TypeFull, comptime _: []const u8, _: std.fmt.FormatOptions, writer: anytype) !void {
        switch (value) {
            .void,
            .int,
            .mode,
            .pitch,
            .fraction,
            .note,
            .section => _ = try writer.write(@tagName(value)),
            .list => |el_ty| {
                _ = try writer.print("[{}]", .{lookup(el_ty)});
            },
        }
    }
};
pub const TypeIntern = struct {
    const Self = @This();
    map: std.AutoArrayHashMap(TypeStorage, void),
    extras: std.ArrayList(u32),
    pub fn get_new_extra(self: TypeIntern) u32 {
        return @intCast(self.extras.items.len);
    }
    pub fn init(a: Allocator) Self {
        return TypeIntern { .map = std.AutoArrayHashMap(TypeStorage, void).init(a), .extras = std.ArrayList(u32).init(a) };
    }
    pub fn deinit(self: *Self) void {
        self.map.deinit();
        self.extras.deinit();
    }
    pub fn intern(self: *Self, s: TypeFull) Type {
        const gop = self.map.getOrPutAdapted(s, TypeFull.Adapter {.extras = &self.extras}) catch unreachable; // ignore out of memory
        const more = switch (s) {
            .void,
            .int,
            .mode,
            .pitch,
            .note,
            .fraction,
            .section => undefined,
            .list => |el_ty| blk: {
                const extra_idx = self.get_new_extra();
                self.extras.append(el_ty.t) catch unreachable;
                break :blk extra_idx;
            }
       };
        gop.key_ptr.* = TypeStorage {.more = more, .kind = std.meta.activeTag(s)};
        return Type {.t = @intCast(gop.index) };
    }
    pub fn intern_exist(self: *Self, s: TypeFull) Type {
        return self.map.getIndex(s);
    }
    // assume the i is valid
    //pub fn lookup_alloc(self: Self, i: Type, a: Allocator) TypeFull {
    //    _ = a;
    //    const storage = self.map.keys()[i];
    //    const more = storage.more;
    //    _ = more;
    //    switch (storage.kind) {
    //        .void => return .void,
    //        .int => return .int,
    //        .fraction => return .fraction,
    //        .note => return .note,
    //        .section => return .section,
    //      }
    //}
    pub fn lookup(self: Self, i: Type) TypeFull {
        const storage = self.map.keys()[i.t];
        const more = storage.more;
        const extras = self.extras.items;
        switch (storage.kind) {
            .void => return .void,
            .int => return .int,
            .pitch => return .pitch,
            .mode => return .mode,
            .fraction => return .fraction,
            .note => return .note,
            .section => return .section,
            .list => return .{.list = Type {.t=extras[more]}},
        }
    }
    pub fn len(self: Self) usize {
        return self.map.keys().len;
    }
};

// Some commonly used type and typechecking. We cached them so when we don't have to intern them every time.
// They are initialized in TypeIntern.init


pub var type_pool: TypeIntern = undefined;

pub fn init(a: std.mem.Allocator) void {
    type_pool = TypeIntern.init(a);
    Type.@"void" = intern(TypeFull.void);
    Type.int = intern(TypeFull.int);
    Type.pitch = intern(TypeFull.pitch);
    Type.mode = intern(TypeFull.mode);
    Type.fraction = intern(TypeFull.fraction);
    Type.note = intern(TypeFull.note);
    Type.chord = intern(TypeFull {.list = Type.note });
    Type.section = intern(TypeFull.section);
}

pub fn intern(s: TypeFull) Type {
    return type_pool.intern(s);
}

pub fn lookup(i: Type) TypeFull {
    return type_pool.lookup(i);
}
