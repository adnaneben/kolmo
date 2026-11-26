#!/bin/bash
# Ultra+ timed run: -U and --archive-max, encode and decode, under $V/tools/timed.sh (machine-wide timing lock), each row
# only when the 1-minute load is below 12 at its start and end (otherwise retried after 5 min, at most 12 times).
# usage: timed_run.sh OUT.tsv KOLMO_MAIN KOLMO_UP FILE...   rows: file arm phase wall_s user_s rss_bytes load_start load_end bytes
V=/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD
out=$1; km=$2; ku=$3; shift 3; t=$(mktemp -d)
load() { sysctl -n vm.loadavg | awk '{print $2}'; }
row() {   # arm phase cmd...
  local arm=$1 ph=$2; shift 2
  for try in $(seq 12); do
    l0=$(load); if awk "BEGIN{exit !($l0 < 12)}"; then
      $V/tools/timed.sh ultra-plus /usr/bin/time -lp "$@" 2> $t/err
      l1=$(load); if awk "BEGIN{exit !($l1 < 12)}"; then
        printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "$f" $arm $ph $(awk '/^real/{print $2}' $t/err) $(awk '/^user/{print $2}' $t/err) $(awk '/maximum resident/{print $1}' $t/err) $l0 $l1 $(stat -f %z $t/z) >> $out; return 0; fi
    fi; sleep 300
  done; printf "%s\t%s\t%s\tINCOMPLETE\n" "$f" $arm $ph >> $out
}
for r in 1 2 3; do for f in "$@"; do
  row ultra enc sh -c "$km -U -T 1 -c '$f' > $t/z"; row ultra dec sh -c "$km -d -T 1 -c $t/z > $t/o"; cmp -s "$f" $t/o || echo "$f ultra RT FAIL" >> $out
  row archive-max enc sh -c "$ku --archive-max -T 1 -c '$f' > $t/z"; row archive-max dec sh -c "$ku -d -T 1 -c $t/z > $t/o"; cmp -s "$f" $t/o || echo "$f archive-max RT FAIL" >> $out
done; done; rm -rf $t
