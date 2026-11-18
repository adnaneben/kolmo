#!/bin/sh
S=$SCRATCH
f=$1; t=$(basename $(dirname $f)); s4=$S/snap4/$t.snap; s3=$S/snap3n/$t.snap
run() { r=$(env $E ./build/cmprior4 eval $f "$@" | sed 's/.*size=\([0-9]*\).*enc=\([0-9.]*\) dec=\([0-9.]*\)/\1 \2 \3/'); echo "$(basename $f) $cfg $r"; }
E="MODEL=3"; cfg=m3; run 0 1023 - 000000
cfg=m3B; run 0 1023 - 000001
E="MODEL=3 QW=8 QS=512 QP=6"; cfg=m3H8; run 115 30 $s3
E="MODEL=4"; cfg=m4; run 0 1023 - 000000
cfg=m4B; run 0 1023 - 000001
E="MODEL=4 QW=8 QS=512 QP=6"; cfg=m4H8; run 115 30 $s4
E="MODEL=4 QW=7 QS=1024 QP=6"; cfg=m4H7; run 115 30 $s4
E="MODEL=4 QW=6 QS=2048 QP=6"; cfg=m4H6; run 115 30 $s4
E="MODEL=4 QW=5 QS=4096 QP=6"; cfg=m4H5; run 115 30 $s4
E="MODEL=4 QW=8 QS=512 QP=6"; cfg=m4nosm; run 99 30 $s4
E="MODEL=4 QW=8 QS=512 QP=6"; cfg=m4noo1; run 51 30 $s4
E="MODEL=4 QW=8 QS=512 QP=4"; cfg=m4QP4; run 115 30 $s4
