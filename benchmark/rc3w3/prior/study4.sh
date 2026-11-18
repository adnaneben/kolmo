#!/bin/sh
S=$SCRATCH
f=$1; t=$(basename $(dirname $f)); s4=$S/snap4/$t.snap; s3=$S/snap3n/$t.snap
run() { r=$(env $E ./build/cmprior4 eval $f "$@" | sed 's/.*size=\([0-9]*\).*enc=\([0-9.]*\) dec=\([0-9.]*\)/\1 \2 \3/'); echo "$(basename $f) $cfg $r"; }
E="MODEL=3"; cfg=m3; run 0 1023 - 000000
cfg=m3B; run 0 1023 - 000001
E="MODEL=3 QW=5 QS=4096 QP=4"; cfg=m3H; run 115 30 $s3
E="MODEL=4"; cfg=m4B; run 0 1023 - 000001
E="MODEL=4 QW=5 QS=4096 QP=4"; cfg=m4H; run 115 30 $s4
