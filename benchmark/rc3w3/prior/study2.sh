#!/bin/sh
S=$SCRATCH
f=$1; t=$(basename $(dirname $f)); sn=$S/snap/$t.snap
run() { r=$(env $E ./build/cmprior eval $f "$@" | sed 's/.*size=\([0-9]*\).*enc=\([0-9.]*\) dec=\([0-9.]*\)/\1 \2 \3/'); echo "$(basename $f) $cfg $r"; }
E=; cfg=A; run 0 1023 - 000000
cfg=H; run 119 30 $sn
cfg=Hw1; run 374 30 $sn
cfg=Hw2; run 630 30 $sn
cfg=Hw3; run 1142 30 $sn
cfg=Hwf; run 2166 30 $sn
cfg=Hmean; run 118 30 $sn
cfg=Hnosm; run 103 30 $sn
cfg=Hnoa1; run 115 30 $sn
E="QW=16 QS=4"; cfg=Q16; run 119 30 $sn
E="QW=8 QS=1024"; cfg=Q8a; run 119 30 $sn
E="QW=8 QS=512"; cfg=Q8b; run 119 30 $sn
E="QP=8"; cfg=QP8; run 119 30 $sn
E="QP=6"; cfg=QP6; run 119 30 $sn
E="QW=8 QS=1024 QP=6"; cfg=Q86; run 119 30 $sn
