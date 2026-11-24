#!/bin/bash
D=/home/user/kolmo-review40-data; K=/home/user/kolmo-review40-base/build/kolmo
for o in "-m ratio" "-m ratio -B 32" "-m ratio -B 16"; do
  nice $K $o -T 2 $D/enwik8 e8.tmp && nice $K -d -T 2 e8.tmp e8.dec && cmp -s e8.dec $D/enwik8 && echo "enwik8 [$o] $(stat -c %s e8.tmp) rt=ok" || echo "enwik8 [$o] FAIL"
done
nice $K -m ratio -T 2 $D/silesia/mozilla moz.k && python3 kinfo.py moz.k
(time nice xz -9 -T1 -k -c $D/silesia/mozilla | wc -c) 2>&1 | grep -v "^$"
rm -f e8.tmp e8.dec
