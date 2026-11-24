#!/bin/bash
D=/home/user/kolmo-review40-data
W=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/r07
O=$W/base.jsonl
for f in $D/silesia/dickens $D/silesia/mozilla $D/enwik8 $D/silesia.tar; do
  for m in "-m speed" "-m balanced" "-m eco" "-m hc -l 9" "-m ratio -l 1" "-m ratio"; do
    $W/sweep.sh $O $f $m
  done
done
for f in $D/silesia/dickens $D/silesia/mozilla; do $W/sweep.sh $O $f -m hc; done
$W/sweep.sh $O $D/silesia/dickens -x
$W/sweep.sh $O $D/silesia/dickens -x -l 1
echo DONE >> $O
