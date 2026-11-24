#!/bin/bash
# R10: LZMA2 literal/position context parameters (lc, lp, pb) at -m ratio through the existing encoder hook
# (-l (lc+1)*100 + lp*10 + pb, src/codecs/lzma.c). rc3 base binary, -T 1; sizes are exact; every output is decoded and compared.
K=/home/user/kolmo-review40-base/build/kolmo
O=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/out
while pgrep -f run_tax.sh > /dev/null; do sleep 5; done
for f in "$@"; do
  b=$(basename "$f")
  nice $K -m ratio -T 1 "$f" "$O/$b.def" && echo "file=$b lc=default size=$(stat -c %s $O/$b.def)" >> lclppb.txt
  for c in "3 0 2" "3 0 0" "3 0 1" "0 0 0" "0 0 2" "0 2 2" "2 2 2" "0 1 1" "3 1 1" "4 0 0" "4 0 2" "1 0 0" "0 2 0" "0 1 0" "2 0 0"; do
    set -- $c; L=$(( ($1 + 1) * 100 + $2 * 10 + $3 ))
    nice $K -m ratio -l $L -T 1 "$f" "$O/$b.l" || { echo "file=$b lc=$1 lp=$2 pb=$3 FAILED" >> lclppb.txt; continue; }
    nice $K -d -T 1 "$O/$b.l" "$O/$b.dec" && cmp -s "$f" "$O/$b.dec" && rt=ok || rt=BAD
    echo "file=$b lc=$1 lp=$2 pb=$3 level=$L size=$(stat -c %s $O/$b.l) roundtrip=$rt" >> lclppb.txt
    rm -f "$O/$b.l" "$O/$b.dec"
  done
  rm -f "$O/$b.def"
done
echo DONE >> lclppb.txt
