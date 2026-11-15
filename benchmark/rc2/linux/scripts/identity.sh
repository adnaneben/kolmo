#!/bin/sh
# Step 3: frames of the release CLI and the control CLI (-DKOLMO_FORCE_GENERIC_INT_FWD) must be identical.
R=/home/user/tools/ident_rel/build/kolmo; C=/home/user/tools/ident/build/kolmo
D=/home/user/data/silesia/files; W=/home/user/tools/identwork; mkdir -p $W
fail=0
for f in $(LC_ALL=C ls $D); do
  for s in "-m balanced" "-m hc" "-m fast --typed-huf"; do
    $R $s -T1 $D/$f $W/r.kol 2>/dev/null; rr=$?
    $C $s -T1 $D/$f $W/c.kol 2>/dev/null; rc=$?
    hr=$(sha256sum < $W/r.kol | cut -c1-16); hc=$(sha256sum < $W/c.kol | cut -c1-16)
    if [ $rr -eq 0 ] && [ $rc -eq 0 ] && cmp -s $W/r.kol $W/c.kol; then v=IDENTICAL; else v=DIFFER; fail=1; fi
    echo "$f	$s	$(stat -c %s $W/r.kol)	$(stat -c %s $W/c.kol)	$hr	$hc	$v"
  done
done
echo "overall: $([ $fail = 0 ] && echo PASS || echo FAIL)"
