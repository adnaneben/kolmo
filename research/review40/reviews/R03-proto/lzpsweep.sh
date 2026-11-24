#!/bin/bash
# codec-18 size per LZP setting (hash/min), one block per file; waits for the ratio sweep to end first
while pgrep -f rn.sh >/dev/null; do sleep 3; done
D=/home/user/kolmo-review40-data
for f in silesia/xml silesia/dickens silesia/reymont silesia/osdb silesia/mr silesia/nci silesia/samba silesia/webster enwik8_10m; do
  for s in "0 0 17" "32 0 17" "64 0 17" "96 0 17" "160 0 17" "255 0 17" "96 0 20" "255 0 20"; do nice ./bwt18 $D/$f $s; done
done
