#!/bin/sh
# Q3: weight initialisation (two-pass means, shared prior) on 100 KB, 1 MB and 10 MB.
D=${1:-/home/user/data}; OUT=$(dirname "$0")/raw/winit.txt
for sz in 100k 1m 10m; do for k in text log table bin num image; do
  f=$D/c/${k}_$sz; [ -f "$f" ] || continue; m=3; case $k in bin|num|image) m=2;; esac
  ./build/cmwinit "$f" $m $D/prior/$k >> "$OUT" 2>&1
done; done
