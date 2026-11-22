#!/bin/sh
# The w4 bwt lane's whole-file sweep: rc3 base, this branch and bsc -m0 -e2 (bsclike), ROUNDS rounds per thread count,
# the three tools alternating per file inside a round (best of the rounds is kept by table.py). 1 and 4 threads are
# separate passes. Usage: run_all.sh OUTDIR BASE_KOLMO NEW_KOLMO BSCLIKE FILE...
OUT=$1 BASE=$2 NEW=$3 BSC=$4; shift 4
mkdir -p "$OUT"
H=$(dirname "$0")
for T in 1 4; do
  for r in $(seq 1 "${ROUNDS:-2}"); do
    for f in "$@"; do
      ROUNDS=1 python3 "$H/bench.py" "$OUT/t$T.jsonl" "$T" kolmo:base:"$BASE" kolmo:new:"$NEW" bsc:bsc:"$BSC" -- "$f" > /dev/null
    done
  done
done
