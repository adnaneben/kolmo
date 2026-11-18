#!/bin/sh
# sz.sh NAME BIN FILE: -x -T1 size, round trip checked
o=$(mktemp $SCRATCH/out/sz.XXXXXX); $2 -x -T1 $3 $o.kz >/dev/null 2>&1 && $2 -d $o.kz $o.d >/dev/null 2>&1 && cmp -s $3 $o.d && echo "$(basename $3) $1 $(stat -c %s $o.kz)" || echo "$(basename $3) $1 FAIL"; rm -f $o $o.kz $o.d
