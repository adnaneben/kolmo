#!/bin/sh
# Q1: plan oracle on every file and level (one process at a time; sizes are what count, times are indicative).
D=${1:-/home/user/data/c}; OUT=$(dirname "$0")/raw/oracle.txt
for sz in 100k 1m 10m 8m 100m; do for k in text table log bin num image mixed; do
  f=$D/${k}_$sz; [ -f "$f" ] || continue
  for m in speed balanced hc ratio x; do
    if [ "$m" = x ] && [ "$sz" = 100m ]; then continue; fi
    grep -q "^SUMMARY $f $m " "$OUT" 2>/dev/null && continue
    ./build/planoracle "$f" $m 1 >> "$OUT" 2>&1
  done
done; done
