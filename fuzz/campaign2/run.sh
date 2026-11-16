#!/bin/bash
# Campaign 2 runner: each target in libFuzzer fork mode (crashes, OOMs and timeouts are recorded and the run goes on).
# Usage (repo root): W=workdir SEEDS=seeddir SECS=3600 TARGETS="fuzz_a fuzz_b" fuzz/campaign2/run.sh
# $SEEDS/<target>/ holds the seeds of each target (see fuzz/campaign2/make_seeds.sh).
set -u
W=${W:?}; SEEDS=${SEEDS:?}; SECS=${SECS:-3600}; FORK=${FORK:-1}
B=$PWD/build/fuzz
export ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
for t in ${TARGETS:?}; do
  case $t in fuzz_roundtrip*|fuzz_nplane) ML=${ML_RT:-1048576};; *) ML=${ML_DEC:-4194304};; esac
  mkdir -p "$W/corpus/$t" "$W/artifacts/$t" "$W/logs"
  ( cd "$W/artifacts/$t" && exec "$B/$t" -fork=$FORK -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 \
      -max_total_time=$SECS -max_len=$ML -rss_limit_mb=4096 -timeout=25 -print_final_stats=1 \
      -artifact_prefix="$W/artifacts/$t/" "$W/corpus/$t" "$SEEDS/$t" ) \
      > "$W/logs/$t.log" 2>&1 &
  echo "$t pid $!"
done
date -u +%FT%TZ > "$W/started.$$"
wait
date -u +%FT%TZ > "$W/finished.$$"
