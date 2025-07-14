// the new version of our type system. I hope this is better!
const std = @import("std");
const Lexer = @import("lexer.zig");
const Symbol = Lexer.Symbol;
const Allocator = std.mem.Allocator;
pub const Type = u32;
pub const Decl = u32;

pub const TypeStorage = struct {
    kind: Kind,
    more: u32,
};

pub const Kind = enum(u8) {
    int,
    note,
    section,
};
pub const TypeFull = union(Kind) {
    int,            // leaf
    note,
    section,
  
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
            _ = extras;
            switch (a) {
                .int,
                .note,
                .section => return true,
            }
        }
        pub fn hash(ctx: Adapter, a: TypeFull) u32 {
            _ = ctx;
            return switch (a) {
                .int,
                .note,
                .section => std.hash.uint32(@intFromEnum(a)),
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
            .int,
            .note,
            .section => _ = try writer.write(@tagName(value)),
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
            .int,
            .note,
            .section => undefined,
       };
        gop.key_ptr.* = TypeStorage {.more = more, .kind = std.meta.activeTag(s)};
        return @intCast(gop.index);
    }
    pub fn intern_exist(self: *Self, s: TypeFull) Type {
        return self.map.getIndex(s);
    }
    // assume the i is valid
    pub fn lookup_alloc(self: Self, i: Type, a: Allocator) TypeFull {
        _ = a;
        const storage = self.map.keys()[i];
        const more = storage.more;
        _ = more;
        switch (storage.kind) {
            .int => return .int,
            .note => return .note,
            .section => return .section,
          }
    }
    // TODO add a freeze pointer modes, which whilst in this mode, any append into extras is not allowed
    pub fn lookup(self: Self, i: Type) TypeFull {
        const storage = self.map.keys()[i];
        const more = storage.more;
        const extras = self.extras.items;
        _ = more;
        _ = extras;
        switch (storage.kind) {
            .int => return .int,
            .note => return .note,
            .section => return .section,
        }
    }
    pub fn len(self: Self) usize {
        return self.map.keys().len;
    }
};

// Some commonly used type and typechecking. We cached them so when we don't have to intern them every time.
// They are initialized in TypeIntern.init
pub var int:        Type = undefined;
pub var note:       Type = undefined;
pub var section:    Type = undefined;

pub var type_pool: TypeIntern = undefined;

pub fn init(a: std.mem.Allocator) void {
    type_pool = TypeIntern.init(a);
    int = intern(TypeFull.int);
    note = intern(TypeFull.note);
    section = intern(TypeFull.section);
}

pub fn intern(s: TypeFull) Type {
    return type_pool.intern(s);
}

pub fn lookup(i: Type) TypeFull {
    return type_pool.lookup(i);
}


test TypeIntern {
    const equalDeep = std.testing.expectEqualDeep;
    const a = std.testing.allocator;
    type_pool = TypeIntern.init(a);
    defer type_pool.deinit();

    const int_type = TypeFull.int;
    const float_type = TypeFull.float;

    const t1 = type_pool.intern(int_type);
    const t2 = type_pool.intern(float_type);

    const int_type2 = type_pool.lookup(t1, a);
    const float_type2 = type_pool.lookup(t2, a);

    try equalDeep(int_type, int_type2);
    try equalDeep(float_type, float_type2);

    const int4_type = TypeFull {.array = .{.el = t1, .size = 4}}; 
    const int6_type = TypeFull {.array = .{.el = t1, .size = 6}}; 

    const t3 = type_pool.intern(int4_type);
    const t4 = type_pool.intern(int6_type);
    const int4_type2 = type_pool.lookup(t3, a);
    const int6_type2 = type_pool.lookup(t4, a);

    try equalDeep(int4_type, int4_type2);
    try equalDeep(int6_type, int6_type2);
}

