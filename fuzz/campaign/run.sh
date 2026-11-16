#!/bin/bash
# Coverage-guided campaign: every target at once, each in libFuzzer fork mode so crashes, OOMs and timeouts are
# recorded and the run continues. TARGETS="fuzz_a fuzz_b" limits the set. Usage: W=workdir SEEDS=seeddir SECS=6000 fuzz/campaign/run.sh   (from repo root)
set -u
W=${W:?}; SEEDS=${SEEDS:?}; SECS=${SECS:-6000}; FORK=${FORK:-1}
B=$PWD/build/fuzz
export ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=0:abort_on_error=0
export UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1
declare -A SEED=( [fuzz_decompress]=kolmo [fuzz_limited]=kolmo [fuzz_dict]=dict [fuzz_stream]=stream
                  [fuzz_roundtrip]=roundtrip [fuzz_nplane]=nplane [fuzz_krc]=krc [fuzz_kbcx]=kbcx [fuzz_fastq]=fastq )
for t in ${TARGETS:-${!SEED[@]}}; do
  case $t in fuzz_roundtrip|fuzz_nplane) ML=131072;; *) ML=1048576;; esac
  mkdir -p "$W/corpus/$t" "$W/artifacts/$t" "$W/logs"
  ( cd "$W/artifacts/$t" && exec "$B/$t" -fork=$FORK -ignore_crashes=1 -ignore_ooms=1 -ignore_timeouts=1 \
      -max_total_time=$SECS -max_len=$ML -rss_limit_mb=4096 -timeout=25 -print_final_stats=1 \
      -artifact_prefix="$W/artifacts/$t/" "$W/corpus/$t" "$SEEDS/${SEED[$t]}" ) \
      > "$W/logs/$t.log" 2>&1 &
  echo "$t pid $!"
done
date -u +%FT%TZ > "$W/started.$$"
wait
date -u +%FT%TZ > "$W/finished.$$"
