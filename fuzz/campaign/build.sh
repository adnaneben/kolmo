#!/bin/bash -eu
# Builds every libFuzzer target for the coverage-guided campaign (run from the repo root).
#   MODE=fuzz (default): -fsanitize=fuzzer,address,undefined -> build/fuzz/<target>
#   MODE=cov:  -fsanitize=fuzzer -fprofile-instr-generate -fcoverage-mapping -> build/fuzzcov/<target>
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
for t in fuzz_decompress fuzz_roundtrip fuzz_nplane fuzz_limited fuzz_stream fuzz_dict; do
  b $t -Ifuzz fuzz/$t.c src/kolmo.c $LIBS
done
b fuzz_krc   -I$XD fuzz/fuzz_krc.c $XD/krc.c -lpthread -lm
b fuzz_kbcx  -I$XD fuzz/fuzz_kbcx.c $XD/kbc.c $XD/kbx.c $XD/krc.c -lpthread -lm
b fuzz_fastq -I$XD fuzz/fuzz_fastq.c $XD/frontier_fastq.c $XD/frontier_entropy.c $XD/frontier_runs.c -lm
