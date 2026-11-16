#!/bin/bash
# profrep.sh KOLMO DATA OUT REPS DIR(c|d|cd) MODE...  (pre-compresses once for d)
K=$1; D=$2; O=$3; R=$4; W=$5; shift 5; mkdir -p $O/tmp
for m in "$@"; do tag=$(echo "$m" | tr -d ' -'); for f in $D/*; do b=$(basename $f)
  src=$f; if [ "$tag" = "x" ]; then head -c 2000000 $f > $O/tmp/x_$b; src=$O/tmp/x_$b; fi
  $K $m -T1 $src $O/tmp/z.$b 2>/dev/null
  for r in $(seq $R); do
    [[ $W == *c* ]] && SAMPLER_OUT=$O/$b.$tag.c.$r LD_PRELOAD=${SAMPLER:-$(dirname $0)/sampler.so} $K $m -T1 $src $O/tmp/zz 2>/dev/null
    [[ $W == *d* ]] && SAMPLER_OUT=$O/$b.$tag.d.$r LD_PRELOAD=${SAMPLER:-$(dirname $0)/sampler.so} $K -d -T1 $O/tmp/z.$b $O/tmp/u 2>/dev/null
  done
done; done
