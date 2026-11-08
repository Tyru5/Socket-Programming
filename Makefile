# Makefile for chat (originally CS457 Project 1, 2016)

CFLAGS   ?= -std=c17 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
SANITIZE  = -fsanitize=address,undefined -fno-omit-frame-pointer

all: chat

chat: chat.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) $< -o $@

# Same program built with AddressSanitizer and UndefinedBehaviorSanitizer.
chat-asan: chat.c
	$(CC) $(CFLAGS) -O1 $(SANITIZE) $< -o $@

test: chat
	./test.sh ./chat

test-asan: chat-asan
	./test.sh ./chat-asan

clean:
	rm -rf chat chat-asan *.o *.dSYM

.PHONY: all test test-asan clean
