#!/usr/bin/env bash
# Writes a few valid datagrams as seed files for the fuzzer. Each file is one
# mode byte (0 = line A, 1 = line B, 2 = snapshot stream) then the datagram.
# Usage: make_seeds.sh <output directory>
set -euo pipefail
out="${1:?usage: make_seeds.sh <output directory>}"
mkdir -p "$out"

# Writes the bytes given as hex text ("41 00 ...") to a file.
write_hex() {
  local file="$1" hex="${2// /}"
  printf "$(printf '%s' "$hex" | sed 's/../\\x&/g')" > "$file"
}

session="53 45 53 53 49 4f 4e 30 30 31"
add="41 00 01 00 00 1f 1a ce d9 f0 00 00 00 00 00 00 00 00 65 42 00 00 00 64 41 41 50 4c 20 20 20 20 00 16 ed 24"
execute="45 00 01 00 00 1f 1a ce d9 f3 e8 00 00 00 00 00 00 00 65 00 00 00 28 00 00 00 00 00 00 1b 59"

# Mode 0, golden packet with two messages.
write_hex "$out/main_two_messages" "00 $session 00 00 00 00 00 00 00 01 00 02 00 24 $add 00 1f $execute"
# Mode 1, heartbeat.
write_hex "$out/main_heartbeat" "01 $session 00 00 00 00 00 00 00 03 00 00"
# Mode 0, end of session.
write_hex "$out/main_end_of_session" "00 $session 00 00 00 00 00 00 00 03 ff ff"
# Mode 2, a snapshot packet with an end-of-snapshot message (G and 20 characters).
write_hex "$out/snapshot_end" "02 53 4e 41 50 53 45 53 53 30 31 00 00 00 00 00 00 00 01 00 01 00 15 47 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 20 38"
echo "wrote $(ls "$out" | wc -l) seed files to $out"
