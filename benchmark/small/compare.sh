#!/bin/bash
# Time two Kolmo trees on the small-input benchmark with their runs interleaved, so that machine load falls on both
# alike: for every kind and size, BASE then NEW, the order swapped at each step. Rivals run in NEW only.
# usage: benchmark/small/compare.sh BASE_TREE NEW_TREE OUTDIR [smallbench options, e.g. -T 1,10]
# Writes OUTDIR/base.tsv and OUTDIR/new.tsv; summarize.py OUTDIR/base.tsv OUTDIR/new.tsv makes the tables.
# PREBUILT=1: use OUTDIR/smallbench-base and OUTDIR/smallbench-new as they are (BASE and NEW are then ignored).
# Load gate: a step starts only when the 1-minute load is at most MAXLOAD (default 12; it waits up to WAIT seconds,
# default 900, polling every POLL seconds), and its rows are kept only when the load is still at most MAXLOAD when it
# ends. Up to three attempts per step; a step with no clean attempt keeps no row, is listed in OUTDIR/incomplete.txt,
# and the script then exits 3. Every attempt is logged in OUTDIR/load.log. LOAD_CMD overrides the load reading and
# SETS_FILE / SIZES the sets and sizes (tests: benchmark/small/test_compare.sh).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); BASE=$1; NEW=$2; OUT=$3; shift 3
V=${V:-/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD}
mkdir -p "$OUT"
if [ -z "${PREBUILT:-}" ]; then
    SMALLBENCH_BIN="$OUT/smallbench-base" "$HERE/run.sh" "$BASE"
    SMALLBENCH_BIN="$OUT/smallbench-new" "$HERE/run.sh" "$NEW"
fi
load() {
    if [ -n "${LOAD_CMD:-}" ]; then sh -c "$LOAD_CMD"; return; fi
    sysctl -n vm.loadavg 2>/dev/null | awk '{print $2}' || cut -d' ' -f1 /proc/loadavg
}
le() { awk -v l="$1" -v m="$MAXLOAD" 'BEGIN{exit !(l + 0 <= m + 0)}'; }   # le LOAD: LOAD <= MAXLOAD
MAXLOAD=${MAXLOAD:-12}; WAIT=${WAIT:-900}; POLL=${POLL:-15}
: > "$OUT/base.tsv"; : > "$OUT/new.tsv"; : > "$OUT/incomplete.txt"; step=0
for set in $(grep -v '^#' "${SETS_FILE:-$HERE/sets.txt}" | sed "s|=|=$V/|"); do
    for size in ${SIZES:-64 256 1024 4096 16384 65536}; do
        for who in base new; do
            [ $((step % 2)) = 1 ] && who=$([ $who = base ] && echo new || echo base)
            if [ $who = base ]; then c=kolmo; else c=kolmo,zstd,brotli,lz4; fi
            ok=0
            for try in 1 2 3; do
                waited=0; l0=$(load)
                while ! le "$l0" && [ "$waited" -lt "$WAIT" ]; do sleep "$POLL"; waited=$((waited + POLL)); l0=$(load); done
                if ! le "$l0"; then echo "$who ${set%%=*} $size try$try start load $l0 > $MAXLOAD: not run" >> "$OUT/load.log"; continue; fi
                "$OUT/smallbench-$who" -s $size -c $c "$@" "$set" > "$OUT/part.tsv"; l1=$(load)
                echo "$who ${set%%=*} $size try$try load $l0 -> $l1" >> "$OUT/load.log"
                if le "$l1"; then ok=1; break; fi
            done
            if [ $ok = 0 ]; then echo "$who ${set%%=*} $size" >> "$OUT/incomplete.txt"; continue; fi
            if [ -s "$OUT/$who.tsv" ]; then tail -n +2 "$OUT/part.tsv" >> "$OUT/$who.tsv"; else cat "$OUT/part.tsv" > "$OUT/$who.tsv"; fi
        done
        step=$((step + 1))
    done
done
rm -f "$OUT/part.tsv"
if [ -s "$OUT/incomplete.txt" ]; then
    echo "compare.sh: incomplete: $(wc -l < "$OUT/incomplete.txt" | tr -d ' ') steps had no attempt within load $MAXLOAD (incomplete.txt)" >&2
    exit 3
fi
