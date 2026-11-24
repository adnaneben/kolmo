#!/bin/bash
# mt.sh name file args... : run kolmo under the allocation shim (ASLR off) and report the peak live set
W=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/r07
K=${KOLMO:-/home/user/kolmo-review40-base/build/kolmo}
name=$1; f=$2; shift 2
MT_OUT=$W/mt.$name.log LD_PRELOAD=$W/mtrace.so nice setarch -R $K "$@" -T ${T:-1} $f $W/mt.$name.k
echo "== $name: $* $(basename $f) -> $(stat -c %s $W/mt.$name.k) bytes"
python3 $W/mtrace_report.py $W/mt.$name.log $K 0x555555554000
rm -f $W/mt.$name.k
