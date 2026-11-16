#!/bin/bash
# Replays each target's corpus through its coverage build (MODE=cov fuzz/campaign/build.sh) and writes llvm-cov
# reports: $OUT/report.txt (per file), $OUT/functions.txt (per function), $OUT/zero_functions.txt; and, for the frame
# decoders only (decompress, limited, dict, stream, stream_limited, both builds), $OUT/report_decoders.txt.
# Usage: CORPUS=dir-with-one-subdir-per-target OUT=dir fuzz/campaign2/coverage.sh   (from repo root)
set -u
C=${CORPUS:?}; OUT=${OUT:?}; B=$PWD/build/fuzzcov; mkdir -p "$OUT/raw"
ALL="fuzz_decompress fuzz_decompress_nocm fuzz_limited fuzz_limited_nocm fuzz_dict fuzz_dict_nocm fuzz_stream_nocm
     fuzz_stream_limited_nocm fuzz_roundtrip fuzz_roundtrip_nocm fuzz_nplane fuzz_krc fuzz_kbcx fuzz_fastq"
DEC="fuzz_decompress fuzz_decompress_nocm fuzz_limited fuzz_limited_nocm fuzz_dict fuzz_dict_nocm fuzz_stream_nocm fuzz_stream_limited_nocm"
for t in $ALL; do
  [ -d "$C/$t" ] || continue
  ls "$C/$t" | split -l 100 - "$OUT/raw/$t.chunk."       # one process per chunk: one bad input cannot end the replay
  for ch in "$OUT/raw/$t.chunk."*; do
    ( cd "$C/$t" && LLVM_PROFILE_FILE="$OUT/raw/$t.%p.profraw" timeout 3600 "$B/$t" -runs=1 -timeout=600 -rss_limit_mb=8192 $(cat "$ch") \
        > "$OUT/raw/$(basename "$ch").log" 2>&1 ) &
    while [ "$(jobs -rp | wc -l)" -ge "$(nproc)" ]; do wait -n; done
  done
done; wait
SRCS="src/kolmo.c src/*.inc src/experimental/krc.c src/experimental/kbc.c src/experimental/kbx.c src/experimental/frontier_fastq.c src/experimental/frontier_entropy.c src/experimental/frontier_runs.c"
llvm-profdata merge -sparse "$OUT"/raw/fuzz_*.profraw -o "$OUT/all.profdata"
first=""; objs=""; for t in $ALL; do if [ -z "$first" ]; then first="$B/$t"; else objs="$objs -object $B/$t"; fi; done
llvm-cov report "$first" $objs -instr-profile="$OUT/all.profdata" $SRCS > "$OUT/report.txt"
llvm-cov report "$first" $objs -instr-profile="$OUT/all.profdata" -show-functions $SRCS > "$OUT/functions.txt"
python3 fuzz/campaign/zero_functions.py "$OUT/functions.txt" > "$OUT/zero_functions.txt"
first=""; objs=""; profs=""; for t in $DEC; do ls "$OUT"/raw/$t.*.profraw >/dev/null 2>&1 || continue; profs="$profs $(ls "$OUT"/raw/$t.*.profraw | tr '\n' ' ')"; if [ -z "$first" ]; then first="$B/$t"; else objs="$objs -object $B/$t"; fi; done
llvm-profdata merge -sparse $profs -o "$OUT/dec.profdata"
llvm-cov report "$first" $objs -instr-profile="$OUT/dec.profdata" $SRCS > "$OUT/report_decoders.txt"
llvm-cov report "$first" $objs -instr-profile="$OUT/dec.profdata" -show-functions $SRCS > "$OUT/functions_decoders.txt"
python3 fuzz/campaign/zero_functions.py "$OUT/functions_decoders.txt" > "$OUT/zero_functions_decoders.txt"
cat "$OUT/report.txt"
