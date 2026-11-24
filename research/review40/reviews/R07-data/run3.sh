#!/bin/bash
W=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/r07
D=/home/user/kolmo-review40-data; M=/home/user/kolmo-review40-coord/tools/measure.py
cd $W
while ! grep -q DONE fp2.jsonl 2>/dev/null; do sleep 5; done
python3 $M --runs 1 --label "rival|xz -9 -T1|silesia.tar" --out rival.jsonl -- nice sh -c "xz -9 -T1 -c $D/silesia.tar > $W/x.xz"
echo "{\"label\":\"size|xz -9 silesia.tar\",\"bytes\":$(stat -c %s x.xz)}" >> rival.jsonl
python3 $M --runs 1 --label "rival|xz -9 -T1|dickens" --out rival.jsonl -- nice sh -c "xz -9 -T1 -c $D/silesia/dickens > $W/x.xz"
echo "{\"label\":\"size|xz -9 dickens\",\"bytes\":$(stat -c %s x.xz)}" >> rival.jsonl
python3 $M --runs 1 --label "rival|bzip2 -9|silesia.tar" --out rival.jsonl -- nice sh -c "bzip2 -9 -c $D/silesia.tar > $W/x.bz2"
echo "{\"label\":\"size|bzip2 -9 silesia.tar\",\"bytes\":$(stat -c %s x.bz2)}" >> rival.jsonl
rm -f x.xz x.bz2; echo DONE >> rival.jsonl
