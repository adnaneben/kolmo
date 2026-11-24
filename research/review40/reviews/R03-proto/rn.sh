#!/bin/bash
# ratio vs ratio -n (no transforms) vs BWT-every-block, exact sizes, round trip checked by cmp
D=/home/user/kolmo-review40-data; K=/home/user/kolmo-review40-base/build/kolmo
for f in samba mozilla ooffice osdb mr x-ray sao nci xml dickens reymont webster; do
  for o in "-m ratio" "-m ratio -n" "-m max -l 1 -n"; do
    nice $K $o -T 2 $D/silesia/$f rn.tmp && nice $K -d -T 2 rn.tmp rn.dec && cmp -s rn.dec $D/silesia/$f && echo "$f [$o] $(stat -c %s rn.tmp) rt=ok" || echo "$f [$o] FAIL"
  done
done
rm -f rn.tmp rn.dec
