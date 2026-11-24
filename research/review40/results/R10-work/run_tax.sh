#!/bin/bash
# R10 trial-tax experiment: every coder call of the instrumented research build (KOLMO_XPSTAT), one file at a time, -T 1
K=/home/user/kolmo-review40-wt/R10/build/kolmo-xpstat
O=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/out
mode="$1"; shift; tag="$1"; shift
for f in "$@"; do
  b=$(basename "$f")
  s=$(date +%s.%N)
  nice $K $mode -T 1 "$f" "$O/$b.$tag.kolmo" 2> "tax_${tag}_$b.log"
  e=$(date +%s.%N)
  echo "file=$b tag=$tag wall_s=$(echo "$e - $s" | bc) in=$(stat -c %s "$f") out=$(stat -c %s "$O/$b.$tag.kolmo")" >> tax_${tag}_summary.txt
  rm -f "$O/$b.$tag.kolmo"
done
