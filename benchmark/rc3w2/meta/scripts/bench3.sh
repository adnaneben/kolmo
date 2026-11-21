#!/bin/bash
# usage: bench3.sh KOLMO OUTCSV mode files...  : file,mode,in,out,median compress s (3 runs),median decompress s (3 runs)
K=$1; OUT=$2; M=$3; shift 3
T=/tmp/claude-0/benchtmp; mkdir -p $T
med() { printf "%s\n" "$@" | sort -n | sed -n 2p; }
for f in "$@"; do
  n=$(stat -c %s "$f"); cs=(); ds=()
  for r in 1 2 3; do
    s=$(date +%s%N); $K -m $M -T 4 "$f" $T/c >/dev/null 2>&1 || echo "FAIL c $f"; e=$(date +%s%N); cs+=($(( (e-s)/1000 )))
    s=$(date +%s%N); $K -d -T 4 $T/c $T/d >/dev/null 2>&1; e=$(date +%s%N); ds+=($(( (e-s)/1000 )))
  done
  cmp -s "$f" $T/d || echo "ROUNDTRIP FAIL $f"
  echo "$(basename $f),$M,$n,$(stat -c %s $T/c),$(med ${cs[@]}),$(med ${ds[@]})" >> $OUT
done
