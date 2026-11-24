#!/bin/bash
# R02 size table: base rc3 binary vs review40/R02 with KOLMO_NATIVE=rans, every frame decoded (opt-in unset) and compared with cmp.
# usage: sizes.sh OUT.tsv MODES FILE...   (appends: file mode raw base krans round_trip base_refuses)
B=/home/user/kolmo-review40-base/build/kolmo; K=${K:-/home/user/kolmo-review40-wt/R02/build/kolmo}
out=$1; modes=$2; shift 2; T=$(mktemp -d /home/user/kolmo-review40-coord/results/R02-work/tmp.XXXXXX)
for f in "$@"; do for m in $modes; do
  nice $B -m $m -T 2 "$f" $T/b.k 2>/dev/null; bs=$(stat -c %s $T/b.k)
  KOLMO_NATIVE=rans nice $K -m $m -T 2 "$f" $T/k.k 2>/dev/null; ks=$(stat -c %s $T/k.k)
  nice $K -d -T 2 $T/k.k $T/k.out 2>/dev/null; if cmp -s "$f" $T/k.out; then rt=RT_OK; else rt=RT_FAIL; fi
  if cmp -s $T/b.k $T/k.k; then ref=same_frame; else if nice $B -d $T/k.k $T/b.out >/dev/null 2>&1; then ref=rc3_decodes; else ref=rc3_refuses; fi; fi
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "$(basename "$f")" $m $(stat -c %s "$f") $bs $ks $rt $ref >> $out
  rm -f $T/*
done; done; rmdir $T
