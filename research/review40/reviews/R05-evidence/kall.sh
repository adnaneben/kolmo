#!/bin/bash
# usage: kall.sh OUTFILE mode-args... -- files
K=/home/user/kolmo-review40-base/build/kolmo
out=$1; shift; args=(); while [ "$1" != "--" ]; do args+=("$1"); shift; done; shift
T=$(mktemp -d)
for f in "$@"; do
  nice $K "${args[@]}" -T 1 "$f" $T/a.k 2>/dev/null && nice $K -d -T 1 $T/a.k $T/a.d && cmp -s $T/a.d "$f" && echo "$(basename $f) ${args[*]} $(stat -c %s $T/a.k) RT=ok" >> $out || echo "$(basename $f) ${args[*]} FAIL" >> $out
done
rm -rf $T
