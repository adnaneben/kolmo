#!/bin/sh
# one line per (file, config)
S=$SCRATCH
f=$1; t=$(basename $(dirname $f)); sn=$S/snap/$t.snap; tr=$S/corpus/train/$t/train.bin
run() { r=$(./build/cmprior eval $f "$@" | sed 's/.*size=\([0-9]*\).*enc=\([0-9.]*\) dec=\([0-9.]*\)/\1 \2 \3/'); echo "$(basename $f) $cfg $r"; }
cfg=A; run 0 1023 - 000000
cfg=B; run 0 1023 - 000001
cfg=C; run 2 1023 $sn
cfg=D; run 1 1023 $sn
cfg=E; run 3 1023 $sn
cfg=F; run 55 1023 $sn
cfg=G; run 55 30 $sn
cfg=H; run 119 30 $sn
cfg=I; run 127 30 $sn
cfg=J; run 255 30 $sn
cfg=K; run 0 1023 - 000000 $tr 65536
cfg=L; run 119 30 $sn 000000 $tr 65536
