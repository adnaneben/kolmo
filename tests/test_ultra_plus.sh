#!/bin/sh
# Ultra+ (--archive-max) version rule: frames are version 13 only when a version-12 reader cannot decode them.
# usage: tests/test_ultra_plus.sh NEW_KOLMO STOCK_V12_KOLMO FILE...
# For each FILE: --archive-max -T 1 round trip with NEW_KOLMO; a version-12 frame must decode with the stock reader to
# the same bytes; a version-13 frame must be refused by it. Graph inputs (numeric arrays, 16-bit images) exercise leaf
# trials where UPX can win and later lose (PR #122 review).
set -u
new=$1; old=$2; shift 2; t=$(mktemp -d); fail=0
for f in "$@"; do
  "$new" --archive-max -T 1 -c "$f" > "$t/z" || { echo "FAIL compress $f"; fail=1; continue; }
  v=$(od -An -tu1 -j4 -N1 "$t/z" | tr -d ' ')
  "$new" -d -c "$t/z" > "$t/o" && cmp -s "$f" "$t/o" || { echo "FAIL round trip $f"; fail=1; continue; }
  if "$old" -d -c "$t/z" > "$t/o2" 2>/dev/null && cmp -s "$f" "$t/o2"; then oldok=1; else oldok=0; fi
  if [ "$v" = 12 ] && [ $oldok = 0 ]; then echo "FAIL v12 frame not readable by the v12 reader: $f"; fail=1; fi
  if [ "$v" = 13 ] && [ $oldok = 1 ]; then echo "FAIL v13 frame that a v12 reader decodes: $f"; fail=1; fi
  echo "ok v$v $(wc -c < "$t/z" | tr -d ' ') $f"
done
rm -rf "$t"; [ $fail = 0 ] && echo "ULTRA+ VERSION RULE PASSED" ; exit $fail
