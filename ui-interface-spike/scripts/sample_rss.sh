#!/bin/bash
# Identical to ../../qt3d-spike/scripts/sample_rss.sh, copied rather than
# referenced so this spike directory stays self-contained. Samples VmRSS of
# a PID every 5s until it exits - used for validation item F's memory
# trend check in ../docs/findings.md.
# Usage: sample_rss.sh <pid> <outfile>
PID="$1"
OUT="$2"
: >"$OUT"
while kill -0 "$PID" 2>/dev/null; do
    RSS=$(awk '/VmRSS/{print $2}' "/proc/$PID/status" 2>/dev/null)
    echo "$(date +%s) ${RSS:-dead}" >>"$OUT"
    sleep 5
done
