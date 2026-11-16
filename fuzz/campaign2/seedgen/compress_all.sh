#!/bin/bash
# compress each raw input of $1 into $2 with several modes
K=/home/user/kolmo/build/kolmo; RAW=$1; OUT=$2; N=${N:-2}
OPTS=("-m balanced" "-m ratio" "-m speed" "-m hc" "-q" "-m max" "-m eco")
for f in "$RAW"/*; do for o in "${OPTS[@]}"; do
  tag=$(printf "%s" "$o" | tr -d ' -'); out="$OUT/${tag}__$(basename "$f").kolmo"
  [ -f "$out" ] && continue
  ( timeout 600 $K $o "$f" "$out" 2>/dev/null || rm -f "$out" ) &
  while [ "$(jobs -rp | wc -l)" -ge "$N" ]; do wait -n; done
done; done; wait
