#!/bin/sh
# Q2: race of presets (all with the shared prior) on a sample, against the default and the oracle among presets.
D=${1:-/home/user/data/c}; R=$(dirname "$0")/raw; OUT=$R/race.txt; SAMPLE=${SAMPLE:-262144}
PRESETS="000000 000001 90b401 dcb401 520401 c8a101 091601 d2a901 1ca501"
for sz in ${SIZES:-100k 1m}; do for k in text log table bin num image; do
  f=$D/${k}_$sz; [ -f "$f" ] || continue; m=3; p=$R/prior3.inc; case $k in bin|num|image) m=2; p=$R/prior2.inc;; esac
  echo "./build/cmrace race $f $m $SAMPLE $p $PRESETS"
done; done | xargs -P${P:-2} -I{} sh -c '{}' >> "$OUT" 2>&1
