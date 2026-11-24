#!/bin/bash
# usage: sizes.sh MODEARGS file...   prints: file bytes kolmo-bytes roundtrip   (sizes exact; no timing)
K=/home/user/kolmo-review40-base/build/kolmo
O=/home/user/kolmo-review40-data/R13/out
mode="$1"; shift
for f in "$@"; do
  b=$(basename "$f"); t=$O/$b.$$.k; d=$O/$b.$$.d
  nice $K $mode -T 2 "$f" "$t" >/dev/null 2>&1 || { echo "$b FAIL-compress"; continue; }
  nice $K -d -T 2 "$t" "$d" >/dev/null 2>&1
  if cmp -s "$f" "$d"; then rt=ok; else rt=MISMATCH; fi
  echo "$b $(stat -c %s "$f") $(stat -c %s "$t") $rt"
  rm -f "$t" "$d"
done
