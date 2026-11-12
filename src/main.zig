const std = @import("std");
const Io = std.Io;

const zig_rewrite = @import("zig_rewrite");
const tsting = @import("testing.zig");

const Mode = enum { p, server, client };

pub fn main(init: std.process.Init) !void {
    const io = init.io;

    // This is appropriate for anything that lives as long as the process.
    const arena: std.mem.Allocator = init.arena.allocator();

    // Accessing command line arguments:
    const args = try init.minimal.args.toSlice(arena);

    var stdout_buffer: [1024]u8 = undefined;
    var stdout_file_writer: Io.File.Writer = .init(.stdout(), io, &stdout_buffer);
    const stdout_writer = &stdout_file_writer.interface;

    zig_rewrite.printMessage();
    tsting.testing();

    const name: []const u8 = "Tyrus Malmstrom";
    std.debug.print("The length of this is: {d}\n", .{name.len});

    if (args.len < 2) {
        return error.MissingArg;
    }

    const mode = std.meta.stringToEnum(Mode, args[1]) orelse {
        std.debug.print("unknown mode: {s}\n", .{args[1]});
        return error.BadArg;
    };

    switch (mode) {
        .p => {
            // logic here...
            std.debug.print("here\n", .{});
        },
        .server => {
            // logic here...
            std.debug.print("server\n", .{});
        },
        .client => {
            // logic here...
            std.debug.print("client\n", .{});
        },
    }

    try stdout_writer.flush(); // Don't forget to flush!
}
