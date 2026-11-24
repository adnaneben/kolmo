#!/bin/bash
# R02 indicative timings through measure.py (-T 1, 3 runs): base rc3 vs review40/R02 with KOLMO_NATIVE=rans. usage: timing.sh OUT.jsonl MODE FILE...
B=/home/user/kolmo-review40-base/build/kolmo; K=/home/user/kolmo-review40-wt/R02/build/kolmo; M=/home/user/kolmo-review40-coord/tools/measure.py
out=$1; mode=$2; shift 2; T=/home/user/kolmo-review40-coord/results/R02-work/tm; mkdir -p $T
for f in "$@"; do n=$(basename $f)
  python3 $M --runs 3 --label R02-base-$mode-compress-$n --out $out -- $B -m $mode -T 1 $f $T/$n.$mode.base.k > /dev/null
  python3 $M --runs 3 --label R02-krans-$mode-compress-$n --out $out -- env KOLMO_NATIVE=rans $K -m $mode -T 1 $f $T/$n.$mode.kr.k > /dev/null
  python3 $M --runs 3 --label R02-base-$mode-decompress-$n --out $out -- $B -d -T 1 $T/$n.$mode.base.k $T/$n.base.out > /dev/null
  python3 $M --runs 3 --label R02-krans-$mode-decompress-$n --out $out -- $K -d -T 1 $T/$n.$mode.kr.k $T/$n.kr.out > /dev/null
  cmp -s $f $T/$n.base.out && cmp -s $f $T/$n.kr.out && echo "$n $mode base $(stat -c %s $T/$n.$mode.base.k) krans $(stat -c %s $T/$n.$mode.kr.k) RT_OK" || echo "$n $mode RT_FAIL"
done
