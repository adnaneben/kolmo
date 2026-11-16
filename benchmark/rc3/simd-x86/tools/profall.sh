#!/bin/bash
# usage: profall.sh KOLMO DATA OUTDIR MODE... ; one sampler file per (file, mode, c|d)
K=$1; D=$2; O=$3; shift 3; mkdir -p $O/tmp
for m in "$@"; do for f in $D/*; do b=$(basename $f); tag=$(echo "$m" | tr -d ' -'); 
  src=$f
  if [ "$tag" = "x" ]; then head -c 3000000 $f > $O/tmp/x_$b; src=$O/tmp/x_$b; fi
  s=$(date +%s.%N)
  SAMPLER_OUT=$O/$b.$tag.c LD_PRELOAD=${SAMPLER:-$(dirname $0)/sampler.so} $K $m -T1 $src $O/tmp/z 2>/dev/null
  t1=$(date +%s.%N)
  SAMPLER_OUT=$O/$b.$tag.d LD_PRELOAD=${SAMPLER:-$(dirname $0)/sampler.so} $K -d -T1 $O/tmp/z $O/tmp/u 2>/dev/null
  t2=$(date +%s.%N)
  cmp -s $src $O/tmp/u || echo "ROUNDTRIP FAIL $b $m"
  echo "$b $tag $(stat -c %s $src) $(stat -c %s $O/tmp/z) $(echo "$t1-$s" | bc) $(echo "$t2-$t1" | bc)"
done; done
