#!/bin/bash
# Item 5, mitigation: encode and decode wall time and peak RSS of main and of each merge variant at -9, -T 1 and -T 10.
# Variants (bin/kolmo-cap, research knobs): full = #117 as it was (no cap); cap32 / cap16 / cap12 = the merged unit at
# most 32 / 16 / 12 MiB; capmax = at most the largest unmerged segment of the input. Frames do not depend on -T.
# Columns: file, variant, op (enc|dec), threads, repeat, wall s, peak RSS bytes, frame bytes, 1-min load before, after.
# Run as: $V/tools/timed.sh item5b $D/timing2.sh   (one timed workload at a time)
# Load guard: each row waits for a 1-minute load <= 12 first and is retried (up to 6 times) when the load before or
# after it is above 12; a cell that already has 2 rows within the limit in timing2.tsv is skipped (resumable).
V=$HOME/Kolmo-Reviews/review-20261119T083610Z.J62kQD; D=$V/review40b/work/item5; B=$D/bin
out=$D/timing2.tsv; tmp=$D/tmp; mkdir -p $tmp
load() { sysctl -n vm.loadavg | awk '{print $2}'; }
waitload() { local i=0; while awk "BEGIN{exit !($(load) > 12)}" && [ $i -lt 120 ]; do sleep 15; i=$((i+1)); done; }
valid() {  # file variant op threads -> number of rows within the load limit
  [ -f $out ] && awk -F'\t' -v f=$1 -v v=$2 -v o=$3 -v t=$4 '$1==f && $2==v && $3==o && $4==t && $9<=12 && $10<=12' $out | wc -l | tr -d ' ' || echo 0; }
ok_last() { tail -1 $out | awk -F'\t' '{exit !($9<=12 && $10<=12)}'; }
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
    env "${env[@]}" $k -9 -T 1 -f -c $V/$f > $fr                # the frame to decode (untimed; frames do not depend on -T)
    for t in 1 10; do
      for op in enc dec; do
        rep=$(valid $name $v $op $t)
        while [ "$rep" -lt 2 ]; do
          for try in 1 2 3 4 5 6; do
            waitload; l0=$(load)
            if [ $op = enc ]; then env "${env[@]}" /usr/bin/time -l $k -9 -T $t -f -c $V/$f > $tmp/t2.enc 2> $tmp/t2.err
            else /usr/bin/time -l $B/kolmo-base -d -T $t -c $fr > /dev/null 2> $tmp/t2.err; fi
            row $name $v $op $t $((rep + 1)) $(stat -f%z $fr) $l0
            ok_last && break
          done
          rep=$((rep + 1))
        done
      done
    done
    rm -f $fr $tmp/t2.enc
  done
done
