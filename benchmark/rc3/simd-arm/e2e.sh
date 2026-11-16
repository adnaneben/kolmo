#!/bin/bash
# End to end, one thread: this branch (NEW proploop) against the branch point (BASE proploop: the structure lane's head,
# 9ef225d), 15 interleaved rounds per case; frames compared byte for byte on every decode case.
#   benchmark/rc3/simd-arm/e2e.sh NEW BASE WORK DATA SYNTH OUT.jsonl
NEW=$1; BASE=$2; WORK=$3; D=$4; S=$5; OUT=$6
AB="python3 -I $(dirname "$0")/ab.py"
FILES="$D/dickens $D/owid-co2-data.csv $D/loghub_all.log $D/api.json $S/rgb8.ppm $S/clip.y4m $D/x-ray $S/img16.pgm $D/mr $S/music.wav $S/walk_i32.npy $D/owid_energy_f64.npy"
for f in $FILES; do
  for m in speed balanced eco hc ratio; do
    $AB "$NEW" "$WORK" 15 0.3 "$m:d:$f" --base "$BASE" --json "$OUT"
    if [ "$m" = hc ] || [ "$m" = ratio ]; then secs=0; else secs=0.3; fi
    $AB "$NEW" "$WORK" 15 $secs "$m:c:$f" --base "$BASE" --json "$OUT"
  done
done
$AB "$NEW" "$WORK" 15 0 "x:c:$S/dickens2m" "x:d:$S/dickens2m" --base "$BASE" --json "$OUT"
