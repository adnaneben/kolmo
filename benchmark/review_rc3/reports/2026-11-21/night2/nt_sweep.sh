#!/bin/bash
# nt_sweep.sh CODEC BLOCK "ENV1" "ENV2" ... : size (and compress MB/s) of CODEC under each env setting (dev measurement)
c=$1; bk=$2; shift 2
B="/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/tools/nt_bench.sh ./build/natbench $c $bk 1"
echo "base $($B | tail -1 | awk '{print $6, $10}')"
for v in "$@"; do echo "$v $(env $v $B | tail -1 | awk '{print $6, $10}')"; done
