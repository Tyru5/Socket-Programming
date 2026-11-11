// chat: a small two-person TCP chat.
// Author: Tyrus Malmstrom
// Originally CS457 Project 1 (Fall 2016); modernized in 2026.
//
// Run with no -s option to wait for a friend (server), or with -s to connect
// to one (client). Once connected, either side can send at any time.
//
// Wire format, one frame per message, all integers in network byte order:
//   uint16 version (457) | uint16 length (0-140) | length bytes of text

#define _DEFAULT_SOURCE // getifaddrs() and NI_MAXHOST on glibc

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_PORT "51717"
#define PROTOCOL_VERSION 457
#define MAX_MESSAGE 140
#define HEADER_SIZE 4

static volatile sig_atomic_t interrupted;

static void on_signal(int signo) {
  (void)signo;
  interrupted = 1;
}

static void usage(FILE *out) {
  fprintf(out, "Usage: chat [-p port]           wait for a friend (server)\n"
               "       chat -s host [-p port]   connect to a friend (client)\n"
               "\n"
               "Options:\n"
               "  -p port  TCP port (default " DEFAULT_PORT ")\n"
               "  -s host  hostname or IPv4/IPv6 address of the other node\n"
               "  -h       show this help\n");
}

static bool valid_port(const char *s) {
  char *end;
  errno = 0;
  long n = strtol(s, &end, 10);
  return errno == 0 && end != s && *end == '\0' && n >= 1 && n <= 65535;
}

// Listens on every local address. A dual-stack IPv6 socket is preferred so
// that IPv4 and IPv6 friends can both connect; plain IPv4 is the fallback.
static int listen_on(const char *port) {
  const int families[] = {AF_INET6, AF_INET};
  int saved_errno = 0;

  for (size_t f = 0; f < sizeof families / sizeof families[0]; f++) {
    struct addrinfo hints = {.ai_family = families[f],
                             .ai_socktype = SOCK_STREAM,
                             .ai_flags = AI_PASSIVE};
    struct addrinfo *res;
    if (getaddrinfo(NULL, port, &hints, &res) != 0)
      continue;

    for (struct addrinfo *ai = res; ai; ai = ai->ai_next) {
      int fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd == -1) {
        saved_errno = errno;
        continue;
      }
      int on = 1, off = 0;
      setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
      if (ai->ai_family == AF_INET6)
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof off);
      if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0 && listen(fd, 1) == 0) {
        freeaddrinfo(res);
        return fd;
      }
      saved_errno = errno;
      close(fd);
    }
    freeaddrinfo(res);
  }

  fprintf(stderr, "chat: cannot listen on port %s: %s\n", port,
          strerror(saved_errno));
  return -1;
}

// Prints this machine's IPv4 addresses so the user can tell their friend
// where to connect.
static void print_addresses(const char *port) {
  struct ifaddrs *ifs;
  int shown = 0;

  if (getifaddrs(&ifs) == 0) {
    for (struct ifaddrs *i = ifs; i; i = i->ifa_next) {
      if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET ||
          !(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK))
        continue;
      char host[NI_MAXHOST];
      if (getnameinfo(i->ifa_addr, sizeof(struct sockaddr_in), host,
                      sizeof host, NULL, 0, NI_NUMERICHOST) == 0) {
        printf("Waiting for a connection on %s port %s\n", host, port);
        shown++;
      }
    }
    freeifaddrs(ifs);
  }
  if (!shown)
    printf("Waiting for a connection on 127.0.0.1 port %s\n", port);
}

static int wait_for_friend(const char *port) {
  int listener = listen_on(port);
  if (listener == -1)
    return -1;
  print_addresses(port);

  int fd;
  while ((fd = accept(listener, NULL, NULL)) == -1) {
    if (errno != EINTR || interrupted) {
      if (!interrupted)
        perror("chat: accept");
      break;
    }
  }
  close(listener); // one conversation per run
  return fd;
}

static int connect_to(const char *host, const char *port) {
  struct addrinfo hints = {.ai_family = AF_UNSPEC, .ai_socktype = SOCK_STREAM};
  struct addrinfo *res;
  int err = getaddrinfo(host, port, &hints, &res);
  if (err != 0) {
    fprintf(stderr, "chat: %s: %s\n", host, gai_strerror(err));
    return -1;
  }

  int fd = -1, saved_errno = 0;
  for (struct addrinfo *ai = res; ai && !interrupted; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd == -1) {
      saved_errno = errno;
      continue;
    }
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0)
      break;
    saved_errno = errno;
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);

  if (fd == -1 && !interrupted)
    fprintf(stderr, "chat: cannot connect to %s port %s: %s\n", host, port,
            strerror(saved_errno));
  return fd;
}

static void print_peer(int sock) {
  struct sockaddr_storage addr;
  socklen_t len = sizeof addr;
  char host[NI_MAXHOST] = "unknown";

  if (getpeername(sock, (struct sockaddr *)&addr, &len) == 0)
    getnameinfo((struct sockaddr *)&addr, len, host, sizeof host, NULL, 0,
                NI_NUMERICHOST);
  // The dual-stack server sees IPv4 friends as "::ffff:a.b.c.d".
  const char *shown = strncmp(host, "::ffff:", 7) == 0 ? host + 7 : host;
  printf("Connected to %s. Type a message and press Enter.\n", shown);
}

static bool send_all(int fd, const void *buf, size_t len) {
  const char *p = buf;
  while (len > 0) {
    ssize_t n = send(fd, p, len, 0);
    if (n == -1) {
      if (errno == EINTR)
        continue;
      return false;
    }
    p += n;
    len -= (size_t)n;
  }
  return true;
}

static bool send_message(int sock, const char *text, size_t len) {
  unsigned char frame[HEADER_SIZE + MAX_MESSAGE];
  uint16_t version = htons(PROTOCOL_VERSION);
  uint16_t length = htons((uint16_t)len);

  memcpy(frame, &version, sizeof version);
  memcpy(frame + sizeof version, &length, sizeof length);
  memcpy(frame + HEADER_SIZE, text, len);
  return send_all(sock, frame, HEADER_SIZE + len);
}

// Sends each complete line in buf as one message and keeps the trailing
// partial line for the next read. At end of input, a final line without a
// newline is sent too. Lines longer than MAX_MESSAGE are dropped whole.
static bool send_lines(int sock, char *buf, size_t *len, bool *too_long,
                       bool eof) {
  size_t start = 0;
  for (;;) {
    char *nl = memchr(buf + start, '\n', *len - start);
    if (!nl && !eof)
      break;
    size_t end = nl ? (size_t)(nl - buf) : *len;
    size_t n = end - start;

    if (*too_long || n > MAX_MESSAGE)
      fprintf(stderr, "Error: message too long (max %d bytes).\n", MAX_MESSAGE);
    else if (n > 0 && !send_message(sock, buf + start, n))
      return false;
    *too_long = false;

    start = nl ? end + 1 : end;
    if (!nl)
      break;
  }

  memmove(buf, buf + start, *len - start);
  *len -= start;
  if (*len > MAX_MESSAGE) { // already too long: discard until the newline
    *too_long = true;
    *len = 0;
  }
  return true;
}

// Prints a received message, replacing control characters so a friend can't
// send terminal escape sequences.
static void print_message(const unsigned char *text, size_t len, bool prompt) {
  char clean[MAX_MESSAGE];
  for (size_t i = 0; i < len; i++) {
    unsigned char c = text[i];
    clean[i] = (c < 0x20 && c != '\t') || c == 0x7f ? '?' : (char)c;
  }

  if (prompt)
    printf("\r\033[2K"); // replace the "You: " prompt line
  printf("Friend: %.*s\n", (int)len, clean);
  if (prompt)
    printf("You: ");
  fflush(stdout);
}

// Relays stdin to the friend and the friend's messages to stdout until one
// side hangs up. Returns the process exit status.
static int chat(int sock) {
  bool prompt = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
  unsigned char in[HEADER_SIZE + MAX_MESSAGE]; // one frame at most
  size_t in_len = 0;
  char line[512];
  size_t line_len = 0;
  bool too_long = false, stdin_open = true;

  print_peer(sock);
  if (prompt) {
    printf("You: ");
    fflush(stdout);
  }

  while (!interrupted) {
    // select() rather than poll(): macOS poll() never reports end of input
    // on a FIFO, so `chat < fifo` would never notice it.
    fd_set ready;
    FD_ZERO(&ready);
    FD_SET(sock, &ready);
    if (stdin_open)
      FD_SET(STDIN_FILENO, &ready);
    if (select(sock + 1, &ready, NULL, NULL, NULL) == -1) {
      if (errno == EINTR)
        continue;
      perror("chat: select");
      return 1;
    }

    if (FD_ISSET(sock, &ready)) {
      ssize_t n = recv(sock, in + in_len, sizeof in - in_len, 0);
      if (n == -1) {
        if (errno == EINTR)
          continue;
        perror("chat: recv");
        return 1;
      }
      if (n == 0) {
        if (prompt)
          printf("\r\033[2K");
        printf("Friend disconnected.\n");
        if (in_len == 0)
          return 0;
        fprintf(stderr, "chat: connection closed mid-message\n");
        return 1;
      }
      in_len += (size_t)n;

      // TCP is a byte stream: a read can hold part of a frame or several.
      while (in_len >= HEADER_SIZE) {
        uint16_t version, length;
        memcpy(&version, in, sizeof version);
        memcpy(&length, in + sizeof version, sizeof length);
        version = ntohs(version);
        length = ntohs(length);
        if (version != PROTOCOL_VERSION || length > MAX_MESSAGE) {
          fprintf(stderr, "chat: protocol error (version %u, length %u)\n",
                  version, length);
          return 1;
        }

        size_t frame = HEADER_SIZE + length;
        if (in_len < frame)
          break;
        print_message(in + HEADER_SIZE, length, prompt);
        memmove(in, in + frame, in_len - frame);
        in_len -= frame;
      }
    }

    if (stdin_open && FD_ISSET(STDIN_FILENO, &ready)) {
      ssize_t n = read(STDIN_FILENO, line + line_len, sizeof line - line_len);
      if (n == -1) {
        if (errno == EINTR)
          continue;
        perror("chat: read");
        return 1;
      }
      line_len += (size_t)n;
      bool eof = n == 0;
      if (!send_lines(sock, line, &line_len, &too_long, eof)) {
        perror("chat: send");
        return 1;
      }
      if (eof) {
        // Done sending; keep reading until the friend hangs up too.
        stdin_open = false;
        shutdown(sock, SHUT_WR);
      } else if (prompt) {
        printf("You: ");
        fflush(stdout);
      }
    }
  }
  return 0;
}

int main(int argc, char *argv[]) {
  const char *port = DEFAULT_PORT, *host = NULL;
  int opt;

  while ((opt = getopt(argc, argv, "p:s:h")) != -1) {
    switch (opt) {
    case 'p':
      port = optarg;
      break;
    case 's':
      host = optarg;
      break;
    case 'h':
      usage(stdout);
      return 0;
    default:
      usage(stderr);
      return 1;
    }
  }
  if (optind < argc) {
    usage(stderr);
    return 1;
  }
  if (!valid_port(port)) {
    fprintf(stderr, "chat: invalid port '%s' (expected 1-65535)\n", port);
    return 1;
  }

  // No SA_RESTART, so Ctrl-C interrupts accept(), connect() and select().
  struct sigaction sa = {.sa_handler = on_signal};
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
  // A vanished friend should surface as a send() error, not kill us.
  signal(SIGPIPE, SIG_IGN);

  setvbuf(stdout, NULL, _IOLBF, 0);
  printf("Welcome to Chat!\n");

  int sock = host ? connect_to(host, port) : wait_for_friend(port);
  int status = sock == -1 ? 1 : chat(sock);
  if (sock != -1)
    close(sock);

  if (interrupted) {
    printf("\nClosing connection.\n");
    status = 0;
  }
  return status;
}
