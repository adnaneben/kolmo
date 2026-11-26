#!/bin/bash
# Item 5, mitigation: encode and decode wall time and peak RSS of main and of each merge variant at -9, -T 1 and -T 10.
# Variants (bin/kolmo-cap, research knobs): full = #117 as it was (no cap); cap32 / cap16 / cap12 = the merged unit at
# most 32 / 16 / 12 MiB; capmax = at most the largest unmerged segment of the input. Frames do not depend on -T.
# Columns: file, variant, op (enc|dec), threads, repeat, wall s, peak RSS bytes, frame bytes, 1-min load before, after.
# Run as: $V/tools/timed.sh item5b $D/timing2.sh   (one timed workload at a time; only below load 12)
V=$HOME/Kolmo-Reviews/review-20261119T083610Z.J62kQD; D=$V/review40b/work/item5; B=$D/bin
out=$D/timing2.tsv; tmp=$D/tmp; mkdir -p $tmp
load() { sysctl -n vm.loadavg | awk '{print $2}'; }
row() {   # file variant op threads rep frame-bytes  (time output in $tmp/t2.err)
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "$1" "$2" "$3" "$4" "$5" "$(awk '/real/{print $1}' $tmp/t2.err)" \
    "$(awk '/maximum resident/{print $1}' $tmp/t2.err)" "$6" "$7" "$(load)" >> $out
}
for f in corpus-holdout/mozilla corpus-holdout/samba; do
  name=$(basename $f)
  for v in base full cap32 cap16 cap12 capmax; do
    case $v in
      base) k=$B/kolmo-base; env=();; full) k=$B/kolmo-cap; env=();; cap32) k=$B/kolmo-cap; env=(XCAP=32);;
      cap16) k=$B/kolmo-cap; env=(XCAP=16);; cap12) k=$B/kolmo-cap; env=(XCAP=12);; capmax) k=$B/kolmo-cap; env=(XCAPMAX=0);;
    esac
    fr=$tmp/t2.$name.$v.kolmo
    for t in 1 10; do
      for rep in 1 2; do
        l0=$(load); env "${env[@]}" /usr/bin/time -l $k -9 -T $t -f -c $V/$f > $fr 2> $tmp/t2.err
        row $name $v enc $t $rep $(stat -f%z $fr) $l0
        l0=$(load); /usr/bin/time -l $B/kolmo-base -d -T $t -c $fr > /dev/null 2> $tmp/t2.err
        row $name $v dec $t $rep $(stat -f%z $fr) $l0
      done
    done
    rm -f $fr
  done
done
