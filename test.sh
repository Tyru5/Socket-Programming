#!/usr/bin/env bash
# Integration tests for chat: runs real servers and clients over loopback.
# Usage: ./test.sh [path-to-chat]

CHAT=${1:-./chat}
WORK=$(mktemp -d)
PORT=$((20000 + RANDOM % 30000))
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
failures=0
SPID= CPID=

cleanup() {
  [ -n "$SPID" ] && kill "$SPID" 2>/dev/null
  [ -n "$CPID" ] && kill "$CPID" 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT
trap '' PIPE # a crashed chat must show up as a failure, not kill this script

dump() { sed 's/^/    | /' "$1"; }

fail() {
  echo "FAIL $1"
  failures=$((failures + 1))
  ok=0
}

# wait_for FILE TEXT: wait up to 5 s for TEXT to appear in FILE.
wait_for() {
  for _ in $(seq 50); do
    grep -qF -- "$2" "$1" 2>/dev/null && return 0
    sleep 0.1
  done
  return 1
}

# expect FILE TEXT: fail the current test unless TEXT appears in FILE.
expect() {
  wait_for "$1" "$2" && return 0
  fail "$test_name: expected '$2' in $(basename "$1"):"
  dump "$1"
  return 1
}

# wait_exit PID: wait up to 5 s for PID to exit; returns its status.
wait_exit() {
  for _ in $(seq 50); do
    if ! kill -0 "$1" 2>/dev/null; then
      wait "$1"
      return $?
    fi
    sleep 0.1
  done
  kill "$1" 2>/dev/null
  wait "$1" 2>/dev/null
  return 124
}

begin() {
  test_name=$1
  ok=1
  PORT=$((PORT + 1))
}

finish() { [ "$ok" = 1 ] && echo "ok   $test_name"; }

# start_server: server reads stdin from a FIFO we write through fd 3.
start_server() {
  rm -f "$WORK/s_in" "$WORK/s_out"
  mkfifo "$WORK/s_in"
  "$CHAT" -p "$PORT" <"$WORK/s_in" >"$WORK/s_out" 2>&1 &
  SPID=$!
  exec 3>"$WORK/s_in"
  expect "$WORK/s_out" "Waiting for a connection"
}

# start_pair [HOST]: server plus a client writing through fd 4.
start_pair() {
  start_server || return 1
  rm -f "$WORK/c_in" "$WORK/c_out"
  mkfifo "$WORK/c_in"
  # 3>&-: the client must not hold the server's input open.
  "$CHAT" -s "${1:-127.0.0.1}" -p "$PORT" <"$WORK/c_in" >"$WORK/c_out" 2>&1 3>&- &
  CPID=$!
  exec 4>"$WORK/c_in"
  expect "$WORK/c_out" "Connected to" && expect "$WORK/s_out" "Connected to"
}

# end_pair client|server: that side's input ends; both must exit cleanly.
end_pair() {
  local first=$1 other=server
  [ "$first" = server ] && other=client
  if [ "$first" = client ]; then exec 4>&-; else exec 3>&-; fi
  expect "$WORK/${other:0:1}_out" "Friend disconnected."
  wait_exit "$CPID" || fail "$test_name: client exited with $?"
  wait_exit "$SPID" || fail "$test_name: server exited with $?"
  exec 3>&- 4>&-
  SPID= CPID=
}

begin "messages flow both ways"
if start_pair; then
  echo "hello from client" >&4
  expect "$WORK/s_out" "Friend: hello from client"
  echo "hi from server" >&3
  expect "$WORK/c_out" "Friend: hi from server"
  end_pair client
fi
finish

begin "either side can send several messages in a row"
if start_pair localhost; then
  printf 'one\ntwo\n' >&4
  echo "three" >&4
  expect "$WORK/s_out" "Friend: three"
  grep -qx "Friend: one" "$WORK/s_out" && grep -qx "Friend: two" "$WORK/s_out" ||
    fail "$test_name: lost a message"
  printf 'four\nfive\n' >&3
  expect "$WORK/c_out" "Friend: five"
  end_pair server
fi
finish

begin "IPv6 client reaches the dual-stack server"
if start_pair ::1; then
  echo "over v6" >&4
  expect "$WORK/s_out" "Friend: over v6"
  end_pair client
fi
finish

begin "140-byte limit"
if start_pair; then
  max=$(head -c 140 /dev/zero | tr '\0' a)
  echo "$max" >&4
  echo "${max}b" >&4                           # 141 bytes
  head -c 5000 /dev/zero | tr '\0' c >&4; echo >&4 # spans several reads
  echo "still connected" >&4
  expect "$WORK/s_out" "Friend: still connected"
  grep -qx "Friend: $max" "$WORK/s_out" || fail "$test_name: 140-byte message not delivered"
  grep -qE "b$|cccc" "$WORK/s_out" && fail "$test_name: oversized message was delivered"
  [ "$(grep -c "too long" "$WORK/c_out")" = 2 ] || fail "$test_name: expected 2 'too long' errors"
  end_pair client
fi
finish

begin "control characters from a friend are neutralized"
if start_pair; then
  printf 'evil \033[2Jclear\n' >&4
  expect "$WORK/s_out" "Friend: evil ?[2Jclear"
  end_pair client
fi
finish

begin "server restarts on the same port after closing first"
if start_pair; then
  end_pair server # server's side of the connection is now in TIME_WAIT
  start_server && kill -INT "$SPID" && wait_exit "$SPID"
  exec 3>&-
  SPID=
fi
finish

begin "split frames are reassembled and bad frames rejected"
if start_server; then
  exec 5<>"/dev/tcp/127.0.0.1/$PORT"
  printf '\001\311\000\005hel' >&5 # version 457, length 5, first 3 bytes
  sleep 0.3
  printf 'lo' >&5
  expect "$WORK/s_out" "Friend: hello"
  printf '\311\001\005\000hello' >&5 # little-endian header, as in 2016
  wait_exit "$SPID"
  [ $? = 1 ] || fail "$test_name: server should exit 1 on a bad frame"
  expect "$WORK/s_out" "protocol error (version 51457, length 1280)"
  exec 5>&- 3>&-
  SPID=
fi
finish

begin "Ctrl-C closes cleanly while waiting and while chatting"
if start_server; then
  kill -INT "$SPID"
  wait_exit "$SPID" || fail "$test_name: waiting server exited with $?"
  expect "$WORK/s_out" "Closing connection."
  exec 3>&-
  SPID=
fi
if start_pair; then
  kill -INT "$CPID"
  wait_exit "$CPID" || fail "$test_name: chatting client exited with $?"
  expect "$WORK/c_out" "Closing connection."
  expect "$WORK/s_out" "Friend disconnected."
  wait_exit "$SPID" || fail "$test_name: server exited with $?"
  exec 3>&- 4>&-
  SPID= CPID=
fi
finish

begin "command-line validation"
"$CHAT" -h | grep -q "Usage:" || fail "$test_name: -h should print usage"
for args in "-p 0" "-p 65536" "-p abc" "-p" "-s" "extra" "-s 127.0.0.1 -p $PORT"; do
  # shellcheck disable=SC2086 # split args on purpose
  "$CHAT" $args >"$WORK/out" 2>&1 </dev/null
  status=$?
  [ "$status" = 1 ] || { fail "$test_name: 'chat $args' exited $status, want 1"; dump "$WORK/out"; }
done
finish

echo
if [ "$failures" = 0 ]; then
  echo "All tests passed."
else
  echo "$failures failure(s)."
  exit 1
fi
