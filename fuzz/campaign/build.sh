#!/bin/bash -eu
# Builds every libFuzzer target (run from the repo root).
#   MODE=fuzz (default): -fsanitize=fuzzer,address,undefined -> build/fuzz/<target>
#   MODE=cov:  -fsanitize=fuzzer -fprofile-instr-generate -fcoverage-mapping -> build/fuzzcov/<target>
# Campaign 2: every frame-decoder target is built twice: <target> (as in campaign 1, context mixing included) and
# <target>_nocm with -DKOLMO_FUZZ_NO_CM, which makes the context-mixing decoders (cm_decompress, cm_decompress_img in
# src/cm.inc) return an error at once, so the fuzzer is not held at ~1 exec/s by the ~300 ms model setup.
# Defines and include paths are those of the CI fuzz-smoke job (.github/workflows/ci.yml).
MODE=${MODE:-fuzz}
[ -f build/libvendor.a ] || make -j"$(nproc)" ARCH= build/libvendor.a
DEFS="-DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include"
if [ "$MODE" = cov ]; then
  OUT=build/fuzzcov; CF="-g -O1 -fsanitize=fuzzer -fprofile-instr-generate -fcoverage-mapping -ffp-contract=off"
else
  OUT=build/fuzz; CF="-g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -ffp-contract=off"
fi
mkdir -p "$OUT"
LIBS="build/libvendor.a -llzma -lpthread -lm -lstdc++"
XD=src/experimental
b() { local t=$1; shift; echo "build $OUT/$t"; clang -Wno-psabi $CF $DEFS "$@" -o "$OUT/$t"; }
J=${JOBS:-$(nproc)}
w() { while [ "$(jobs -rp | wc -l)" -ge "$J" ]; do wait -n; done; }
for t in fuzz_decompress fuzz_limited fuzz_dict fuzz_roundtrip; do
  b $t -Ifuzz fuzz/$t.c src/kolmo.c $LIBS & w
  b ${t}_nocm -DKOLMO_FUZZ_NO_CM -Ifuzz fuzz/$t.c src/kolmo.c $LIBS & w
done
# the stream decoders only call kolmo_decompress(_limited) per frame: fuzzed fast only (context-mixing frames are
# covered by the plain frame-decoder targets); fuzz_stream with context mixing is built for replaying campaign 1's corpus
for t in fuzz_stream fuzz_stream_limited; do b ${t}_nocm -DKOLMO_FUZZ_NO_CM -Ifuzz fuzz/$t.c src/kolmo.c $LIBS & w; done
b fuzz_stream -Ifuzz fuzz/fuzz_stream.c src/kolmo.c $LIBS & w
b fuzz_nplane -Ifuzz fuzz/fuzz_nplane.c src/kolmo.c $LIBS & w
b fuzz_krc   -I$XD fuzz/fuzz_krc.c $XD/krc.c -lpthread -lm & w
b fuzz_kbcx  -I$XD fuzz/fuzz_kbcx.c $XD/kbc.c $XD/kbx.c $XD/krc.c -lpthread -lm & w
b fuzz_fastq -I$XD fuzz/fuzz_fastq.c $XD/frontier_fastq.c $XD/frontier_entropy.c $XD/frontier_runs.c -lm & w
wait
