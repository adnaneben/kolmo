#!/bin/bash
# Replays each target's final corpus (and its seeds) through its coverage build (MODE=cov fuzz/campaign3/build.sh)
# and writes llvm-cov reports: $OUT/report.txt (per file, all targets), $OUT/functions.txt, $OUT/zero_functions.txt,
# and $OUT/report_rc3.txt: the files rc3 added or rewrote (codecs 18-26, the BWT pipeline, extensions, profiles).
# Usage (repo root): W=campaign-workdir OUT=dir [EXTRA=dir] fuzz/campaign3/coverage.sh
#   replays $W/corpus/<t>, $W/seeds/<base> and, with EXTRA (campaign 2's corpora, read in place by run.sh), $EXTRA/<t>
#   and $EXTRA/<base>
set -u
W=${W:?}; OUT=${OUT:?}; EXTRA=${EXTRA:-}; B=$PWD/build/fuzzcov; mkdir -p "$OUT/raw"
ALL=${TARGETS:-"fuzz_decompress fuzz_decompress_nocm fuzz_limited_nocm fuzz_stream_nocm fuzz_stream_limited_nocm
     fuzz_roundtrip fuzz_roundtrip_nocm fuzz_nplane fuzz_codecs fuzz_codecs_nocm fuzz_ext_nocm fuzz_profile_nocm fuzz_krc fuzz_kbcx fuzz_fastq"}
for t in $ALL; do
  base=${t%_nocm}
  k=0; dirs=("$W/corpus/$t" "$W/seeds/$base")
  [ -n "$EXTRA" ] && dirs+=("$EXTRA/$t") && [ "$t" != "$base" ] && dirs+=("$EXTRA/$base")
  for d in "${dirs[@]}"; do
    [ -d "$d" ] || continue
    echo "$d" > "$OUT/raw/$t.d$k.dir"
    ls "$d" | split -l 100 - "$OUT/raw/$t.d$k.chunk."   # one process per chunk: one slow input cannot end the replay
    k=$((k+1))
  done
  for ch in "$OUT/raw/$t".d*.chunk.*; do
    [ -e "$ch" ] || continue
    case $ch in *.log) continue;; esac
    d=$(cat "${ch%%.chunk.*}.dir")
    ( cd "$d" && LLVM_PROFILE_FILE="$OUT/raw/$t.%p.profraw" timeout 3600 "$B/$t" -runs=1 -timeout=600 -rss_limit_mb=8192 $(cat "$ch") \
        > "$ch.log" 2>&1 ) &
    while [ "$(jobs -rp | wc -l)" -ge "${JOBS:-$(nproc)}" ]; do wait -n; done
  done
done; wait
SRCS="$(find src -name '*.c' -o -name '*.h' | grep -v -e src/experimental -e src/arch/metal -e src/arch/aarch64 -e src/core/hetero | sort)
      src/experimental/krc.c src/experimental/kbc.c src/experimental/kbx.c src/experimental/frontier_fastq.c
      src/experimental/frontier_entropy.c src/experimental/frontier_runs.c"
RC3="src/codecs/bwt.c src/codecs/bwt_stream.c src/algo/lzp.c src/algo/qlfc.c src/algo/qlfc_model.h src/algo/sais/sais.c src/algo/par.c
     src/codecs/audio.c src/codecs/video.c src/codecs/planes_leaf.c src/codecs/hufzstd_leaf.c src/codecs/zrun_leaf.c src/algo/zerorun.c
     src/codecs/cm_par.c src/codecs/gr6.c src/core/extension.c src/core/index.c src/core/dedup.c src/transforms/table_cols.c
     src/plan/profile.c src/plan/target.c src/api/features.c src/core/decode.c src/core/stream.c"
llvm-profdata merge -sparse "$OUT"/raw/fuzz_*.profraw -o "$OUT/all.profdata"
first=""; objs=""; for t in $ALL; do if [ -z "$first" ]; then first="$B/$t"; else objs="$objs -object $B/$t"; fi; done
llvm-cov report "$first" $objs -instr-profile="$OUT/all.profdata" $SRCS > "$OUT/report.txt"
llvm-cov report "$first" $objs -instr-profile="$OUT/all.profdata" $RC3 > "$OUT/report_rc3.txt"
llvm-cov report "$first" $objs -instr-profile="$OUT/all.profdata" -show-functions $SRCS > "$OUT/functions.txt"
python3 fuzz/campaign3/zero_functions.py "$OUT/functions.txt" > "$OUT/zero_functions.txt"
tail -1 "$OUT/report.txt"; cat "$OUT/report_rc3.txt"
