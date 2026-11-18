#!/bin/bash
# Campaign 3 runner: each target in libFuzzer fork mode (crashes, OOMs and timeouts are recorded and the run goes on).
# Usage (repo root): W=workdir SECS=3900 TARGETS="fuzz_a fuzz_b" fuzz/campaign3/run.sh
# Corpus dirs given to each target: $W/corpus/<t> (written), $W/seeds/<base> (make_seeds.sh), and $EXTRA/<t> when set
# (campaign 2's minimised corpora: <base> or <base>_nocm). <base> is the target name without _nocm.
set -u
W=${W:?}; SECS=${SECS:-3900}; FORK=${FORK:-1}; EXTRA=${EXTRA:-}
B=$PWD/build/fuzz
export ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
for t in ${TARGETS:?}; do
  base=${t%_nocm}
  case $base in fuzz_roundtrip|fuzz_nplane|fuzz_codecs) ML=${ML_RT:-1048576};; *) ML=${ML_DEC:-4194304};; esac
  mkdir -p "$W/corpus/$t" "$W/artifacts/$t" "$W/logs"
  dirs=("$W/corpus/$t")
  [ -d "$W/seeds/$base" ] && dirs+=("$W/seeds/$base")
  [ -n "$EXTRA" ] && [ -d "$EXTRA/$t" ] && dirs+=("$EXTRA/$t")
  [ -n "$EXTRA" ] && [ "$t" != "$base" ] && [ -d "$EXTRA/$base" ] && dirs+=("$EXTRA/$base")
  ( cd "$W/artifacts/$t" && exec "$B/$t" -fork=$FORK -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 \
      -max_total_time=$SECS -max_len=$ML -rss_limit_mb=4096 -timeout=25 -print_final_stats=1 \
      -artifact_prefix="$W/artifacts/$t/" "${dirs[@]}" ) > "$W/logs/$t.log" 2>&1 &
  echo "$t pid $!"
done
date -u +%FT%TZ > "$W/started.$$"
wait
date -u +%FT%TZ > "$W/finished.$$"
