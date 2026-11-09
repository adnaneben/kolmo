#!/bin/sh
# Regression gate: a change may not lose a record or an earlier improvement.
#   gate.sh NEW_BINARY RECORD_BINARY [ultra]
# 1. identity: every lzbench ladder setting on every Silesia file must produce the same bytes as RECORD_BINARY (the
#    build that set the records), and NEW must decode its own output; with "ultra", ultra and ultra -l 1 too, and
#    enwik8 at ultra. Identical bytes = identical ratios, so the lzbench ladder, ultra 17.01% / ultra -l 1 17.99% and
#    enwik8 18.96 MB are kept exactly.
# 2. floors: structured files must reach at least the recorded ratio at each setting (floors.txt: file mode bytes).
# Run alone: ultra needs about 4.5 GB, and two ultra jobs at once can fail for memory.
# Environment: SIL (Silesia dir), DATA (panel dir), JSON (json dir), ENWIK8 (file). Exit status = number of failures.
NEW=$1; REC=$2; ULTRA=$3; W=${TMPDIR:-/tmp}/gate.$$; mkdir -p $W; F=0
SIL=${SIL:-/tmp/lzb/sil}; HERE=$(cd "$(dirname "$0")" && pwd)
LADDER=${LADDER:-"rt|fast -l 1 -q|fast -l 2 -q|fast -q|fast|hc -l 9|hc -l 12|max -l 1|eco|hc|max"}
[ "$ULTRA" = ultra ] && LADDER="$LADDER|ultra -l 1|ultra"; [ "$ULTRA" = ultraonly ] && LADDER=${ULADDER:-"ultra -l 1|ultra"}
IFS='|'; for s in $LADDER; do
  IFS=' '; set -- $s; [ $# -eq 0 ] && { IFS='|'; continue; }; m=$1; shift
  for f in $SIL/*; do
    rm -f $W/n.czn $W/r.czn $W/n.out
    $NEW -m $m "$@" -T1 "$f" $W/n.czn 2>/dev/null || { echo "RUN FAIL (new, exit $?): $(basename $f) [$s]"; F=$((F+1)); continue; }
    $REC -m $m "$@" -T1 "$f" $W/r.czn 2>/dev/null || { echo "RUN FAIL (record, exit $?): $(basename $f) [$s]"; F=$((F+1)); continue; }
    if ! cmp -s $W/n.czn $W/r.czn; then echo "IDENTITY FAIL: $(basename $f) [$s] $(stat -c%s $W/n.czn) vs $(stat -c%s $W/r.czn)"; F=$((F+1)); fi
    $NEW -d -T1 $W/n.czn $W/n.out 2>/dev/null && cmp -s "$f" $W/n.out || { echo "ROUNDTRIP FAIL: $(basename $f) [$s]"; F=$((F+1)); }
  done; echo "identity [$s]: done"; IFS='|'
done; IFS=' '
if [ "${ULTRA#ultra}" != "$ULTRA" ] && [ -n "$ENWIK8" ]; then
  $NEW -m ultra -T1 $ENWIK8 $W/n.czn; $REC -m ultra -T1 $ENWIK8 $W/r.czn
  cmp -s $W/n.czn $W/r.czn && echo "enwik8 ultra: identical ($(stat -c%s $W/n.czn) bytes)" || { echo "IDENTITY FAIL: enwik8 ultra"; F=$((F+1)); }
fi
[ "$ULTRA" = ultraonly ] || while read file mode bytes; do
  case "$file" in ''|'#'*) continue;; esac
  p=$(eval echo "$file"); [ -f "$p" ] || { echo "floor skipped (missing): $file"; continue; }
  IFS=' '; set -- $mode; m=$(echo $mode | tr _ ' '); set -- $m; mm=$1; shift
  rm -f $W/f.czn $W/f.out
  $NEW -m $mm "$@" -T1 "$p" $W/f.czn 2>/dev/null || { echo "RUN FAIL (exit $?): $(basename $p) [$m]"; F=$((F+1)); continue; }
  z=$(stat -c%s $W/f.czn)
  $NEW -d -T1 $W/f.czn $W/f.out 2>/dev/null && cmp -s "$p" $W/f.out || { echo "ROUNDTRIP FAIL: $file [$m]"; F=$((F+1)); }
  if [ "$z" -gt "$bytes" ]; then echo "FLOOR FAIL: $(basename $p) [$m] $z > $bytes"; F=$((F+1)); else echo "floor ok: $(basename $p) [$m] $z <= $bytes"; fi
done < $HERE/floors.txt
rm -rf $W; echo "gate: $F failure(s)"; exit $F
