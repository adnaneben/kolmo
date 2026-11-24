#!/bin/bash
# R02 final size table: base rc3 binary vs review40/R02 (binary copied to final/kolmo.r02).
# For every file and mode: the base frame, the R02 frame WITHOUT the opt-in (must be the same bytes: default unchanged),
# the R02 frame with KOLMO_NATIVE=rans, decoded by R02 with the opt-in unset and compared with cmp, and what the rc3
# binary does with that frame (same_frame / rc3_decodes / rc3_refuses).
# usage: sizes.sh OUT.tsv "MODES" FILE...   (appends: file mode raw base default_check krans round_trip rc3)
B=/home/user/kolmo-review40-base/build/kolmo; K=${K:-/home/user/kolmo-review40-coord/results/R02-work/final/kolmo.r02}   # K=... to test another binary (plan.tsv: kolmo.r02plan)
out=$1; modes=$2; shift 2; T=$(mktemp -d /home/user/kolmo-review40-coord/results/R02-work/tmp.XXXXXX)
for f in "$@"; do for m in $modes; do
  if [ "$m" = x ]; then M="-x"; else M="-m $m"; fi
  nice $B $M -T 2 "$f" $T/b.k 2>/dev/null; bs=$(stat -c %s $T/b.k)
  nice $K $M -T 2 "$f" $T/d.k 2>/dev/null; if cmp -s $T/b.k $T/d.k; then df=default_same; else df=DEFAULT_DIFFERS; fi
  KOLMO_NATIVE=rans nice $K $M -T 2 "$f" $T/k.k 2>/dev/null; ks=$(stat -c %s $T/k.k)
  nice $K -d -T 2 $T/k.k $T/k.out 2>/dev/null; if cmp -s "$f" $T/k.out; then rt=RT_OK; else rt=RT_FAIL; fi
  if cmp -s $T/b.k $T/k.k; then ref=same_frame; else if nice $B -d $T/k.k $T/b.out >/dev/null 2>&1; then ref=rc3_decodes; else ref=rc3_refuses; fi; fi
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "$(basename "$f")" $m $(stat -c %s "$f") $bs $df $ks $rt $ref >> $out
  rm -f $T/*
done; done; rmdir $T
