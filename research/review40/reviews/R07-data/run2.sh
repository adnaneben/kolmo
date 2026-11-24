#!/bin/bash
W=/tmp/claude-0/-home-user-kolmo/4d24f553-2fec-54f1-9707-fa6eff481a2f/scratchpad/r07
D=/home/user/kolmo-review40-data; K=/home/user/kolmo-review40-base/build/kolmo; M=/home/user/kolmo-review40-coord/tools/measure.py
cd $W
for spec in "3 9" "2 0" "4 1"; do set -- $spec
  python3 $M --runs 1 --label "fdpath|silesia.tar|mode$1 level$2" --out fp2.jsonl -- nice ./filepath $1 $2 1 $D/silesia.tar fp2.$1.k >/dev/null
  $K -d -T 1 fp2.$1.k fp2.back && cmp fp2.back $D/silesia.tar && echo "{\"label\":\"size|fdpath silesia.tar mode$1 level$2\",\"bytes\":$(stat -c %s fp2.$1.k),\"sha\":\"$(sha256sum fp2.$1.k | cut -c1-16)\",\"roundtrip\":\"OK\"}" >> fp2.jsonl
done
# the CLI's frames for the same settings (sha only)
$K -m hc -l 9 -T 1 $D/silesia.tar cli.3.k; $K -m eco -T 1 $D/silesia.tar cli.2.k; $K -m ratio -l 1 -T 1 $D/silesia.tar cli.4.k
sha256sum fp2.3.k cli.3.k fp2.2.k cli.2.k fp2.4.k cli.4.k > fp2.sha
rm -f fp2.*.k cli.*.k fp2.back
echo DONE >> fp2.jsonl
