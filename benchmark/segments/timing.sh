#!/bin/bash
# Item 5 timing: wall time and peak RSS of base and new, -T 1 and -T 10, at levels 8-9 (the merge and its probe).
# Run as: $V/tools/timed.sh item5 $D/timing.sh   (one timed workload at a time on the machine)
V=$HOME/Kolmo-Reviews/review-20261119T083610Z.J62kQD; D=$V/review40b/work/item5; B=$D/bin
out=$D/timing.tsv
run() {   # tag bin threads args...
  local tag=$1 b=$2 t=$3; shift 3
  for rep in 1 2; do
    /usr/bin/time -l "$B/kolmo-$b" -T $t "$@" > /dev/null 2> $D/tmp/time.err
    local wall=$(awk '/real/{print $1}' $D/tmp/time.err) rss=$(awk '/maximum resident/{print $1}' $D/tmp/time.err)
    printf "%s\t%s\t%s\t%s\t%s\t%s\n" "$tag" "$b" "$t" "$rep" "$wall" "$rss" >> $out
  done
}
for t in 1 10; do
  for b in base new; do
    run "mozilla -9" $b $t -9 -f -c $V/corpus-holdout/mozilla
    run "samba -9" $b $t -9 -f -c $V/corpus-holdout/samba
    run "silesia.tar -9" $b $t -9 -f -c $V/mkt/data/silesia.tar
    run "mozilla -8" $b $t -8 -f -c $V/corpus-holdout/mozilla
  done
done
