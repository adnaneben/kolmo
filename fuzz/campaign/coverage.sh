#!/bin/bash
# Replays each target's final corpus through its coverage build (MODE=cov fuzz/campaign/build.sh) and writes
# llvm-cov reports: $OUT/report.txt (per-file line/branch/function coverage of src/kolmo.c, src/*.inc and the
# research decoders), $OUT/functions.txt (per-function), $OUT/zero_functions.txt (functions never entered).
# Usage: CORPUS=dir-with-one-subdir-per-target OUT=dir fuzz/campaign/coverage.sh   (from repo root)
set -u
C=${CORPUS:?}; OUT=${OUT:?}; B=$PWD/build/fuzzcov; mkdir -p "$OUT/raw"
for t in fuzz_decompress fuzz_roundtrip fuzz_nplane fuzz_limited fuzz_stream fuzz_dict fuzz_krc fuzz_kbcx fuzz_fastq; do
  [ -d "$C/$t" ] || continue
  # one process per corpus chunk so that a single bad input cannot end the replay early
  ls "$C/$t" | split -l 200 - "$OUT/raw/$t.chunk."
  for ch in "$OUT/raw/$t.chunk."*; do
    ( cd "$C/$t" && LLVM_PROFILE_FILE="$OUT/raw/$t.%p.profraw" timeout 3600 "$B/$t" -runs=1 -timeout=600 -rss_limit_mb=8192 $(cat "$ch") \
        > "$OUT/raw/$(basename "$ch").log" 2>&1 ) &
    while [ "$(jobs -rp | wc -l)" -ge "$(nproc)" ]; do wait -n; done
  done
done; wait
llvm-profdata merge -sparse "$OUT"/raw/fuzz_*.profraw -o "$OUT/all.profdata"
OBJS=""; first=""
for t in fuzz_decompress fuzz_roundtrip fuzz_nplane fuzz_limited fuzz_stream fuzz_dict fuzz_krc fuzz_kbcx fuzz_fastq; do
  if [ -z "$first" ]; then first="$B/$t"; else OBJS="$OBJS -object $B/$t"; fi
done
SRCS="src/kolmo.c src/*.inc src/experimental/krc.c src/experimental/kbc.c src/experimental/kbx.c src/experimental/frontier_fastq.c src/experimental/frontier_entropy.c src/experimental/frontier_runs.c"
llvm-cov report "$first" $OBJS -instr-profile="$OUT/all.profdata" $SRCS > "$OUT/report.txt"
llvm-cov report "$first" $OBJS -instr-profile="$OUT/all.profdata" -show-functions $SRCS > "$OUT/functions.txt"
awk 'NF>=7 && $0 !~ /^(Name|---|File|TOTAL)/ && $4=="0.00%"' "$OUT/functions.txt" > "$OUT/zero_functions.txt"
cat "$OUT/report.txt"
