#!/bin/bash
# nd_sizes.sh V : size of native-defaults vs rc4 per preset on the screening corpus (holds the timing lock: CPU-heavy)
V=$1; S=$V/scratch-nd.noindex; mkdir -p $S; ND=$V/native-defaults/build/kolmo; R4=$V/final-builds/rc4/build/kolmo; LK=$V/locks/timing
until mkdir $LK 2>/dev/null; do sleep 20; done; echo "nd-sizes $(date -u +%FT%TZ)" > $V/locks/timing.owner
for m in speed balanced eco hc ratio; do a=0; b=0
  for f in $V/corpus/*; do case $f in *MANIFEST*) continue;; esac
    $ND -T 1 -m $m $f $S/x.nd || echo "ENCFAIL nd $m $f"; $ND -d -T 1 $S/x.nd $S/x.out && cmp -s $S/x.out $f || echo "RTFAIL $m $f"
    $R4 -T 1 -m $m $f $S/x.r4
    x=$(stat -f %z $S/x.nd); y=$(stat -f %z $S/x.r4); a=$((a+x)); b=$((b+y)); [ $x != $y ] && echo "  $m $(basename $f) nd=$x rc4=$y"
  done; echo "TOTAL $m nd=$a rc4=$b $(python3 -c "print(f'{($a-$b)*100/$b:+.2f}%')")"; done
rmdir $LK
