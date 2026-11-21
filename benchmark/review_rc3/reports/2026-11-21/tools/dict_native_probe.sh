#!/bin/bash
# dict_native_probe.sh V KOLMO OUTDIR : round trips with -D dict and KOLMO_NATIVE (suspected trial-thread id mismatch)
V=$1; K=$2; O=$3; mkdir -p $O/s || exit 1; cd $O || exit 1
head -c 3000000 $V/corpus-holdout/samba | split -a 4 -b 3000 - s/x
$K -t dict.k s/x* > train.log 2>&1 || { echo "TRAIN FAIL"; cat train.log; exit 1; }
I=$V/evidence/native-matrix-c1-release/inputs; n=0; bad=0
for f in $V/corpus-holdout/reymont $I/text.txt $I/table.csv $V/corpus-holdout/samba $V/corpus-holdout/sao; do for m in ratio hc eco balanced speed; do for nat in unset lzma lz huf all all,strong; do for T in 1 4; do
  b=$(basename $f).$m.$nat.T$T; n=$((n+1))
  if [ $nat = unset ]; then env -u KOLMO_NATIVE $K -m $m -T $T -D dict.k $f $b.k > $b.enc.log 2>&1; else KOLMO_NATIVE=$nat $K -m $m -T $T -D dict.k $f $b.k > $b.enc.log 2>&1; fi
  [ $? = 0 ] || { echo "ENCFAIL $b"; bad=$((bad+1)); continue; }
  env -u KOLMO_NATIVE $K -d -T 1 -D dict.k $b.k $b.out > $b.dec.log 2>&1; rc=$?
  if cmp -s $f $b.out; then rm -f $b.out; else echo "BAD $b rc=$rc"; bad=$((bad+1)); fi
done; done; done; done
echo "rows=$n bad=$bad"
