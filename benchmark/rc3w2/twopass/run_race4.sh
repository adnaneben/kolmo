#!/bin/sh
# Q2: the production preset set (4 presets, all with the shared prior), raced on 128 KiB and 256 KiB prefixes.
D=${1:-/home/user/data/c}; R=$(dirname "$0")/raw; OUT=$R/race4.txt
for S in 131072 262144; do for sz in ${SIZES:-1m}; do for k in text log table bin num image; do
  f=$D/${k}_$sz; m=3; case $k in bin|num|image) m=2;; esac
  echo "./build/cmrace race $f $m $S - 000001 1ca501 520401 dcb401"
done; done; done | xargs -P${P:-2} -I{} sh -c '{}' >> "$OUT" 2>&1
