# Socket-Programming

A small two-person chat over TCP, written in C with plain BSD sockets.

It started as CS457 Project 1 (fall 2016; the original
write-up is in [README.txt](README.txt) and the graded submission in
`P1MalmstromTyrus.tar`). In 2026 it was modernized: either side can now send
at any time, IPv6 works, and the memory-safety bugs from the original are
fixed and covered by tests. [MODERNIZATION.md](MODERNIZATION.md) explains
every change and the reasoning behind it.

## Build and run

```sh
make
```

Wait for a friend (server). It prints the addresses your friend can use:

```sh
./chat              # listens on port 51717
./chat -p 6000      # or pick a port
```

Connect to a friend (client), by IPv4 or IPv6 address or hostname:

```sh
./chat -s 192.168.1.20
./chat -s my-laptop.local -p 6000
```

Type a line and press Enter to send it. Messages are limited to 140 bytes.
Press Ctrl-D to hang up, or Ctrl-C to leave immediately.

## Protocol

Each message is one frame. Integers are in network byte order (big-endian):

| Field   | Size     | Value                    |
|---------|----------|--------------------------|
| version | 2 bytes  | always 457               |
| length  | 2 bytes  | 0–140                    |
| text    | `length` | message bytes, no NUL    |

A frame with a different version or a length over 140 ends the
conversation with a protocol error. Control characters in received text are
shown as `?` so a peer can't send terminal escape sequences.

## Tests

```sh
make test        # integration tests over loopback
make test-asan   # the same tests against an AddressSanitizer/UBSan build
```

`test.sh` starts real servers and clients and checks message exchange in
both directions, IPv6, the 140-byte limit, frames split across TCP reads,
malformed frames, hang-ups, Ctrl-C, port reuse after a restart, and argument
validation.
