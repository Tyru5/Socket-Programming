# Modernizing the 2016 chat program

This document explains what changed when the CS457 Project 1 chat program
was brought up to date in 2026, and why. The original code is on `master`.
These changes are on the `modernize` branch.

For how to build, run, and test the program, see [README.md](README.md). The
original 2016 write-up is still in [README.txt](README.txt).

## Summary

The original program compiled cleanly and worked when both sides behaved,
but it crashed on its first message under AddressSanitizer. It also had
several bugs that a remote peer or a normal user action could trigger. The
modernization:

1. Fixes every memory-safety bug found in the review.
2. Makes the chat full duplex, so either side can send at any time.
3. Puts the wire format in network byte order and validates every frame.
4. Adds IPv6 and hostname support and drops deprecated networking calls.
5. Makes Ctrl-C, Ctrl-D, and a vanished peer exit cleanly.
6. Adds integration tests that run in a normal build and in a sanitizer
   build.

The program is still one C file with no dependencies beyond the C library
and BSD sockets, and the command line still works the same way:
`./chat` waits for a friend and `./chat -s host -p port` connects to one.

## Bugs in the original, and how each was fixed

Line numbers refer to `chat.c` on `master`. Bugs 1 to 6 were reproduced by
running the program; bugs 7 to 10 were found by reading the code.

### 1. Heap overflow on every message sent

`create_packet` (lines 374–378) allocated `strlen(input)` bytes and then
`strcpy`'d the string into them. `strcpy` also writes the terminating NUL,
so every send wrote one byte past the allocation. AddressSanitizer reported
this on the first message.

**Fix:** messages are no longer copied into a heap buffer at all. The
sender builds the frame in a fixed stack array of `4 + 140` bytes
(`send_message`) after checking the length, so there is nothing to
overflow and nothing to free.

### 2. Received text was not NUL-terminated, and its length was trusted

`de_serialize` (lines 417–420) read `string_length` from the network,
allocated exactly that many bytes, copied them, and the caller printed the
result with `printf("%s")`, which reads until it finds a NUL that was never
written. Worse, the length was never checked: a peer could claim up to
32,767 bytes and the copy would read far past the 145-byte receive buffer,
and a negative length turned into a huge `malloc`.

**Fix:** the receiver rejects any frame whose length is over 140 or whose
version is not 457, and ends the conversation with a protocol error. Text
is printed with `printf("%.*s", len, text)`, which uses the length instead
of looking for a NUL.

*Why end the conversation instead of skipping the bad frame?* After a bad
header the receiver no longer knows where the next frame starts, so any
attempt to carry on would interpret random bytes as headers. Failing loudly
is the only safe choice for a stream protocol without a resync marker.

### 3. A peer hanging up was treated as a message

When the other side closes the connection, `recv` returns 0. The original
code (lines 200–207 and 307–315) only checked for `-1`, so it went on to
deserialize a zeroed buffer and print it, which was the second ASan crash.

**Fix:** a return of 0 prints `Friend disconnected.` and exits with status
0. If the peer hung up halfway through a frame, it also reports
`connection closed mid-message` and exits with status 1.

### 4. TCP was treated as if it delivered whole messages

The original called `recv` once per message and assumed that one call
returned exactly one 145-byte packet. TCP is a byte stream, so a single
read can return part of a frame, or several frames at once. On a local
network this usually happens to work; over the internet it does not.

**Fix:** received bytes are appended to a buffer that holds up to one full
frame. After each read the receiver pulls out every complete frame and
keeps any partial one for the next read. Sending uses `send_all`, which
loops until every byte is written, because `send` may also write fewer
bytes than asked.

### 5. Ctrl-D crashed the program

At end of input, `fgets` returns `NULL`, and the original passed that
straight to `strlen` (lines 214–216 and 292–294), which segfaults.

**Fix:** end of input means "I'm done talking". The program sends any final
line, calls `shutdown(SHUT_WR)` so the friend sees a clean hang-up, and
keeps printing the friend's messages until they hang up too. This is the
same half-close behavior as tools like `nc`, and it means piping a file
into `chat` delivers the whole file instead of cutting it off.

### 6. `SO_REUSEADDR` was never turned on

`setsockopt` was passed the global `one` (lines 53 and 134). It has no
initializer, and C sets such globals to 0, so the call *disabled* the
option. Restarting the
server right after a conversation failed with "Couldn't bind the port to
the socket!" until the old connection left `TIME_WAIT`, which takes up to a
minute or two.

**Fix:** the option value is a local `int on = 1`. A test restarts the
server on the same port immediately after it closes a connection.

### 7. The server listened only on whatever its hostname resolved to

The server looked up its own hostname with `gethostbyname` and bound to
the first address (lines 142–158). On many machines, including the Mac this
was tested on, the hostname resolves to `127.0.0.1`, so nobody else could
connect. If the lookup failed, the address buffer was never filled in and
the server bound to garbage.

**Fix:** the server binds to the wildcard address (`AI_PASSIVE`), which
accepts connections on every interface. To still tell the user where to
point their friend, it lists the machine's non-loopback IPv4 addresses
using `getifaddrs`, which reads the interfaces directly instead of relying
on DNS.

### 8. Command-line parsing could overflow

`process_cargs` (lines 345–353) read `argv[i + 1]` even when `-p` or `-s`
was the last argument, and `strcpy`'d the `-s` value into a 16-byte buffer
*before* validating it, so a long argument overflowed the heap. `atoi`
also turned `-p abc` into port 0 without complaint.

**Fix:** arguments are parsed with `getopt`, which handles missing values
and unknown flags. The port is checked with `strtol` and must be an
integer from 1 to 65535. The host is passed to `getaddrinfo` as is, so
there is no fixed-size copy.

### 9. The Ctrl-C handler did unsafe work

`sig_handler` (lines 423–431) called `printf`, `free`, and `exit`, none of
which are safe in a signal handler; if the signal arrives while the program
is inside `malloc` or `printf`, the process can deadlock or corrupt memory.
It also closed descriptors that might still be 0, which is standard input.
The original comment says "this isn't working...".

**Fix:** the handler only sets a `volatile sig_atomic_t` flag. It is
installed with `sigaction` *without* `SA_RESTART`, so a blocking `accept`,
`connect`, or `select` returns with `EINTR` when Ctrl-C is pressed. The
main loop sees the flag, closes the socket normally, prints
`Closing connection.`, and exits with status 0. `SIGTERM` is handled the
same way.

`SIGPIPE` is ignored, because by default writing to a socket the peer has
closed kills the process silently. With it ignored, the failure shows up as
a `send` error that the program reports.

### 10. Smaller issues

- `bzero(strang, sizeof(*strang))` cleared one byte, not the buffer,
  because `sizeof(char)` is 1. The buffer is gone in the new code.
- The message limit was effectively 139 characters, because the newline
  from `fgets` counted toward the 140. The newline is no longer sent, so
  the limit is a full 140 bytes of text.
- Lines longer than the 15,000-byte input buffer were split by `fgets` and
  the remainder was treated as the next message. Now an over-long line is
  dropped whole, however long it is, with one error message.
- `free_packet` was declared twice and several variables were unused.
- All socket descriptors and the IP buffer were globals so the signal
  handler could reach them. With the handler reduced to a flag, there are
  no globals left besides that flag.

## Behavior and design changes

### Either side can send at any time

The original alternated strictly: the client sent, then blocked until the
server replied, and vice versa. If you wanted to send two messages in a
row, you couldn't. That was likely what the assignment asked for, but it
is the main thing that made the program feel dated.

The new `chat()` loop waits on both standard input and the socket with
`select()`, and handles whichever is ready. There are no threads, so there
is no locking.

*Why `select()` and not `poll()`?* `poll()` is usually the more modern
choice, and it was the first version written. On macOS, though, `poll()`
never reports end of input on a named pipe (FIFO), so `chat < fifo` would
hang forever after the writer closed. This was confirmed with a small test
program; `select()` reports it correctly. With only two descriptors,
`select()`'s limits do not matter, so it is the simpler portable choice.
A comment in the code records this so nobody "upgrades" it back.

When both standard input and standard output are terminals, the program
shows a `You: ` prompt. When a message arrives while you're at the prompt,
it clears that line, prints `Friend: ...`, and redraws the prompt so the
two don't interleave. When input or output is redirected, it prints no
prompts, which keeps piped output clean.

### Wire format: same fields, now big-endian and variable length

The packet still has the three fields from the assignment: a 16-bit version
(457), a 16-bit length, and the text. Two things changed:

- **Byte order.** The original copied `short`s into the buffer in host
  order, which is little-endian on x86 and ARM. The usual convention for
  network protocols, and what most classmates' programs likely did, is
  big-endian ("network byte order"). The new code converts with
  `htons`/`ntohs`. This is the most likely reason the original README
  reported that it would not work with other students' programs: as long
  as each field is serialized separately, whether the struct holds
  `char *message` or `char message[140]` makes no difference to the bytes
  on the wire, but byte order does.
- **Frame size.** The original always sent 145 bytes, padding the unused
  space with zeros. The new code sends only the 4-byte header plus the
  text, which is what the length field is for.

The integers are now `uint16_t` rather than `short`, because a length
can't be negative and `short` is not guaranteed to be 16 bits.

**Compatibility:** the new program cannot talk to the 2016 build. A 2016
header for version 457 arrives as version 51,457 and is rejected with a
protocol error rather than misread. One of the tests sends exactly such a
header to check this.

### IPv6 and hostnames

The original was IPv4 only and used `gethostbyname`, `inet_addr`, and
`inet_ntoa`, which are deprecated and not thread safe. The new code uses
`getaddrinfo` and `getnameinfo` throughout:

- The client accepts a hostname, an IPv4 address, or an IPv6 address for
  `-s`, and tries each address the name resolves to until one connects.
- The server opens one IPv6 socket with `IPV6_V6ONLY` turned off, which
  accepts both IPv4 and IPv6 clients. If the machine has no IPv6, it falls
  back to a plain IPv4 socket. IPv4 clients show up as `::ffff:a.b.c.d`
  on a dual-stack socket, so the program strips that prefix when printing
  who connected.

### Command line

| | 2016 | 2026 |
|---|---|---|
| Wait for a friend | `./chat` | `./chat` or `./chat -p port` |
| Connect | `./chat -p port -s ip` (both required) | `./chat -s host [-p port]` |
| Default port | 51717, server only, hard-coded | 51717 for both sides |
| Valid ports | 1024–65535 | 1–65535 |
| Bad arguments | sometimes help, sometimes a crash | usage on stderr, exit 1 |

The server used to ignore `-p` and always listen on 51717. Now both sides
honor it, and the client defaults to 51717 so `./chat -s host` just works.

Ports below 1024 are now allowed. Connecting to them is legitimate, and
whether an unprivileged server may listen on one is the operating system's
call: Linux refuses by default, while recent macOS allows it. If the system
refuses, the program prints the real reason (`Permission denied`) instead
of a generic message.

### Printing untrusted text

The friend's message goes straight to your terminal, so a malicious peer
could send escape sequences that clear the screen, move the cursor, or
worse on some terminals. Control characters other than tab are now printed
as `?`.

### Error messages and exit codes

All errors go to standard error with the reason from `strerror` or
`gai_strerror`, such as `chat: cannot connect to 10.0.0.5 port 51717:
Connection refused`, instead of fixed messages like "Error connecting to
the server". The program exits 0 when the conversation ends normally or
with Ctrl-C, and 1 on any error, so scripts can tell the difference.

### Build

The Makefile was rewritten:

- It compiles with `-std=c17` and `-Wall -Wextra -Wpedantic -Wshadow
  -Wstrict-prototypes`. The code builds with no warnings on Apple clang 21.
- `CFLAGS` uses `?=` so you can override it, and `CC`, `CPPFLAGS`, and
  `LDFLAGS` are respected.
- The separate compile and link steps were merged into one rule, since
  there is only one source file.
- `make chat-asan` builds with AddressSanitizer and UndefinedBehaviorSanitizer.
- `make test` and `make test-asan` run the tests.
- The course-specific `package` target was removed. It produced the graded
  tarball, which is kept in the repository as history.

`#define _DEFAULT_SOURCE` at the top of `chat.c` makes `getifaddrs` and
`NI_MAXHOST` visible on glibc in strict C17 mode. macOS doesn't need it.

A `.gitignore` now keeps `chat`, `chat-asan`, object files, and debug
symbol folders out of the repository.

## Tests

[`test.sh`](test.sh) runs real servers and clients over loopback on a
random high port and checks their output. It covers:

| Test | What it guards against |
|---|---|
| Messages flow both ways | basic send and receive, clean hang-up (bugs 3, 5) |
| Several messages in a row, from both sides | the old turn-taking |
| IPv6 client reaches the dual-stack server | IPv6 and hostname support |
| 140-byte limit | off-by-one in the limit; 141-byte and 5,000-byte lines dropped, connection survives (bugs 1, 10) |
| Control characters neutralized | escape sequences from a peer |
| Server restarts on the same port | `SO_REUSEADDR` (bug 6) |
| Split frames reassembled, bad frames rejected | partial reads and length checks (bugs 2, 4); 2016-format headers rejected |
| Ctrl-C while waiting and while chatting | signal handling (bug 9) |
| Command-line validation | bad ports, missing values, extra arguments (bug 8) |

The tests drive the program through named pipes instead of a terminal, so
they need nothing beyond bash and standard Unix tools. The "split frames"
test writes raw bytes with bash's `/dev/tcp`, which is the only way to
control exactly how bytes arrive.

`make test-asan` runs the same tests against the sanitizer build, so any
out-of-bounds access or undefined behavior that the tests reach fails the
run.

To make sure the tests can actually fail, three deliberate bugs were
introduced one at a time: removing `SO_REUSEADDR`, removing the byte-order
conversion, and loosening the 140-byte limit by one. Each made at least one
test fail. The terminal behavior (prompt redraw and Ctrl-D) was checked
separately by driving both sides through pseudo-terminals.

## What did not change

- The program is still a single file, `chat.c`, with no dependencies.
- It is still a one-to-one chat. The server accepts one friend per run
  and stops listening once they connect.
- `README.txt` and `P1MalmstromTyrus.tar` are untouched, as a record of the
  original assignment.

## Known limitations

- Traffic is unencrypted and unauthenticated. Anyone on the path can read
  it, and anyone who reaches the port first becomes your friend. Adding TLS
  would mean a dependency such as OpenSSL, which is beyond the scope of
  this cleanup.
- The 140 limit counts bytes, not characters, so a message in a language
  that uses multi-byte UTF-8 characters fits fewer than 140 characters.
  This matches the original protocol's length field.
- The server prints only its IPv4 addresses, although it accepts IPv6
  connections too.
- The changes were built and tested on macOS with clang only. The code
  uses only POSIX interfaces and should build on Linux, but that has not
  been run yet.
