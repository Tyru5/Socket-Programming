//! By convention, root.zig is the root source file when making a package.
const std = @import("std");
const Io = std.Io;

pub fn printMessage() void {
    std.debug.print("This should work....\n", .{});
}
