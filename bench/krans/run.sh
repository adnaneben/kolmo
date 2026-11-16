#!/bin/sh
# KRANS benchmark matrix: every krans configuration below and the rival coders, over <streams>/{raw,lo,hi}/*.
# usage: bench/krans/run.sh <streams dir> <out dir> [krans binary] [krans-rivals binary]   (results: <out>/*.jsonl)
set -eu
D=${1:?streams dir}; O=${2:?out dir}; K=${3:-build/krans}; R=${4:-build/krans-rivals}; RUNS=${RUNS:-5}
mkdir -p "$O"
run() { tag=$1; shift; for s in raw lo hi; do "$K" -b -r "$RUNS" "$@" "$D/$s"/* | sed "s/^{/{\"stream\":\"$s\",\"config\":\"$tag\",/"; done >> "$O/krans.jsonl"; }
: > "$O/krans.jsonl"
for w in 8 16 32; do for p in 12 14; do run "o0-w$w-p$p" -o 0 -w $w -p $p; done; done
for w in 8 16 32; do run "o1-w$w-p10" -o 1 -w $w -p 10; done
run "o1-w32-p12" -o 1 -w 32 -p 12
for w in 8 16 32; do run "auto-w$w" -o a -w $w; done
for b in 64 1024; do run "auto-w32-B$b" -o a -w 32 -B $b; done
for b in 64 128 1024; do run "o0-w32-p12-B$b" -o 0 -w 32 -p 12 -B $b; done
run "auto-w32-T4" -o a -w 32 -T 4
if [ -x "$R" ]; then
  : > "$O/rivals.jsonl"
  for s in raw lo hi; do "$R" -r "$RUNS" "$D/$s"/* | sed "s/^{/{\"stream\":\"$s\",/" >> "$O/rivals.jsonl"; done
fi
