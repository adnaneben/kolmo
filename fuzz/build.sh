#!/bin/bash -eu
# OSS-Fuzz build script. Builds the vendored libraries and each fuzz target with the provided
# $CC/$CXX/$CFLAGS/$LIB_FUZZING_ENGINE. Run from the repo root.
make -j"$(nproc)" build/libvendor.a ARCH=
for t in fuzz_decompress fuzz_roundtrip; do
  $CC $CFLAGS -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 \
    -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include \
    -c fuzz/$t.c -o $t.o
  $CC $CFLAGS -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 \
    -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include \
    -c src/czn.c -o czn_fuzz.o
  $CXX $CFLAGS $LIB_FUZZING_ENGINE $t.o czn_fuzz.o build/libvendor.a \
    -llzma -lpthread -lm -o "$OUT/$t"
done
