#!/bin/bash
# usage: sweep.sh OUT file mode-args...   -> one compress (T1) + one decompress (T1), cmp
K=/home/user/kolmo-review40-base/build/kolmo
M=/home/user/kolmo-review40-coord/tools/measure.py
W=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/r07
out=$1; f=$2; shift 2
b=$(basename $f); tag=$(echo "$*" | tr -d ' /=')
python3 $M --runs 1 --label "enc|$b|$*" --out $out -- nice $K "$@" -T 1 $f $W/$b.$tag.k >/dev/null
python3 $M --runs 1 --label "dec|$b|$*" --out $out -- nice $K -d -T 1 $W/$b.$tag.k $W/$b.$tag.back >/dev/null
sz=$(stat -c %s $W/$b.$tag.k); if cmp -s $f $W/$b.$tag.back; then rt=OK; else rt=FAIL; fi
echo "{\"label\":\"size|$b|$*\",\"bytes\":$sz,\"roundtrip\":\"$rt\"}" >> $out
rm -f $W/$b.$tag.k $W/$b.$tag.back
