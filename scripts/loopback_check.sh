#!/usr/bin/env bash
# Plays a simulated day through real multicast sockets between two processes and checks it.
#
#   scripts/loopback_check.sh BIN_DIR
#
# BIN_DIR holds fh_mktrace, fh_send, fh_live and fh_replay (the tools/ build directory).
# The day is slowed down tenfold and fh_live's windows are scaled to match, so that the
# scheduling jitter of an ordinary machine is small next to them. Exit status 0 means:
# fh_live judged the day against the true feed and passed, and the trace it recorded replays
# to the same verdict offline.
set -u
bin=${1:?usage: loopback_check.sh BIN_DIR}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

port=$((38000 + ($$ % 1000) * 3))
scale=10

"$bin/fh_mktrace" --seed 3 --tier clean "$work/day.fhtr" || exit 1

"$bin/fh_live" --port "$port" --seconds 8 --scale "$scale" \
    --truth "$work/day.fhtr" --record "$work/live.fhtr" >"$work/live.out" 2>"$work/live.err" &
live=$!
sleep 1   # let it join its groups

"$bin/fh_send" "$work/day.fhtr" --port "$port" --scale "$scale" || { kill "$live" 2>/dev/null; exit 1; }
wait "$live"
live_status=$?
cat "$work/live.out"
[ "$live_status" -eq 0 ] || { echo "fh_live failed with status $live_status"; cat "$work/live.err"; exit 1; }

"$bin/fh_replay" "$work/live.fhtr" | tail -2
"$bin/fh_replay" "$work/live.fhtr" >/dev/null || { echo "the recorded trace does not replay clean"; exit 1; }
echo "loopback check passed"
