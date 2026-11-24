#!/bin/bash
# R10: cost and gain of the -x CM preset race on small inputs: research builds with the race (rc3 behaviour) and without
# (-DCM_RACE_MAX=0: the prior with default rates, no race). -T 1. Sizes exact; wall times indicative (shared machine).
A=/home/user/kolmo-review40-wt/R10/build/kolmo-xpstat
B=/home/user/kolmo-review40-wt/R10/build/kolmo-xpstat-norace
OUT=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/out/race.tmp.kolmo
DEC=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/out/race.tmp.dec
while pgrep -f 'run_tax.sh|run_lclppb.sh' > /dev/null; do sleep 5; done
for f in "$@"; do
  b=$(basename "$f")
  for v in race norace; do
    K=$A; [ $v = norace ] && K=$B
    s=$(date +%s.%N); nice $K -x -T 1 "$f" $OUT 2> race_${v}_$b.log; e=$(date +%s.%N)
    nice $K -d -T 1 $OUT $DEC 2>/dev/null && cmp -s "$f" $DEC && rt=ok || rt=BAD
    echo "file=$b variant=$v wall_s=$(echo "$e - $s" | bc) in=$(stat -c %s "$f") out=$(stat -c %s $OUT) roundtrip=$rt $(grep '^compress' race_${v}_$b.log | cut -d' ' -f4-)" >> race.txt
  done
done
echo DONE >> race.txt
