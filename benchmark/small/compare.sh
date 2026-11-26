#!/bin/bash
# Time two Kolmo trees on the small-input benchmark with their runs interleaved, so that machine load falls on both
# alike: for every kind and size, BASE then NEW, the order swapped at each step. Rivals run in NEW only.
# usage: benchmark/small/compare.sh BASE_TREE NEW_TREE OUTDIR [smallbench options, e.g. -T 1,10]
# Writes OUTDIR/base.tsv and OUTDIR/new.tsv; summarize.py OUTDIR/base.tsv OUTDIR/new.tsv makes the tables.
# PREBUILT=1: use OUTDIR/smallbench-base and OUTDIR/smallbench-new as they are (BASE and NEW are then ignored).
# OUTDIR/load.log gets the 1-minute load average before and after every step; a step that ends above MAXLOAD
# (default 12) is run again, up to three times.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); BASE=$1; NEW=$2; OUT=$3; shift 3
V=${V:-/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD}
mkdir -p "$OUT"
if [ -z "${PREBUILT:-}" ]; then
    SMALLBENCH_BIN="$OUT/smallbench-base" "$HERE/run.sh" "$BASE"
    SMALLBENCH_BIN="$OUT/smallbench-new" "$HERE/run.sh" "$NEW"
fi
load() { sysctl -n vm.loadavg 2>/dev/null | awk '{print $2}' || cut -d' ' -f1 /proc/loadavg; }
MAXLOAD=${MAXLOAD:-12}
: > "$OUT/base.tsv"; : > "$OUT/new.tsv"; step=0
for set in $(grep -v '^#' "$HERE/sets.txt" | sed "s|=|=$V/|"); do
    for size in 64 256 1024 4096 16384 65536; do
        for who in base new; do
            [ $((step % 2)) = 1 ] && who=$([ $who = base ] && echo new || echo base)
            if [ $who = base ]; then c=kolmo; else c=kolmo,zstd,brotli,lz4; fi
            for try in 1 2 3; do
                l0=$(load); "$OUT/smallbench-$who" -s $size -c $c "$@" "$set" > "$OUT/part.tsv"; l1=$(load)
                echo "$who ${set%%=*} $size try$try load $l0 -> $l1" >> "$OUT/load.log"
                awk -v l="$l1" -v m="$MAXLOAD" 'BEGIN{exit !(l <= m)}' && break
            done
            if [ -s "$OUT/$who.tsv" ]; then tail -n +2 "$OUT/part.tsv" >> "$OUT/$who.tsv"; else cat "$OUT/part.tsv" > "$OUT/$who.tsv"; fi
        done
        step=$((step + 1))
    done
done
rm -f "$OUT/part.tsv"
