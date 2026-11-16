#!/bin/bash
# Every 5 minutes: free memory and the latest libFuzzer stat line of each target (coverage over time).
W=${W:?}
while pgrep -f build/fuzz/fuzz_ >/dev/null; do
  { echo "## $(date -u +%FT%TZ) mem_avail_MB=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)"
    for l in "$W"/logs/*.log; do echo "$(basename "$l" .log) $(grep -aE '^#[0-9]+: cov:' "$l" | tail -1)"; done; } >> "$W/timeline.txt"
  sleep 300
done
