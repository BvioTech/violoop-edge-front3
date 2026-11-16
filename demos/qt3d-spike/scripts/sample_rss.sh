#!/bin/bash
# Sample VmRSS of a PID every 5s until it exits, for the per-frame native
# memory leak check called for in docs/display-stack-overview.md §8.4
# (the Flutter/GTK-embedder leak on this device was ~20MB/min; this script
# produces the equivalent evidence for the Qt binary).
# Usage: sample_rss.sh <pid> <outfile>
PID="$1"
OUT="$2"
: >"$OUT"
while kill -0 "$PID" 2>/dev/null; do
    RSS=$(awk '/VmRSS/{print $2}' "/proc/$PID/status" 2>/dev/null)
    echo "$(date +%s) ${RSS:-dead}" >>"$OUT"
    sleep 5
done
