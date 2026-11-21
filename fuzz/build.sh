#!/bin/bash -eu
# OSS-Fuzz build script. Builds the vendored libraries and each fuzz target with the provided
# $CC/$CXX/$CFLAGS/$LIB_FUZZING_ENGINE. Run from the repo root. Everything is compiled with -ffp-contract=off
# (the Makefile adds it to the vendored codecs), as in every other build.
make -j"$(nproc)" build/libvendor.a build/libkolmoarch.a ARCH=
for t in fuzz_decompress fuzz_roundtrip; do
  $CC $CFLAGS -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 \
    -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz \
    -c fuzz/$t.c -o $t.o
  $CC $CFLAGS -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 \
    -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz \
    -c src/kolmo.c -o kolmo_fuzz.o
  # Linked with $CXX only because the fuzzing engine (libFuzzer) is C++; Kolmo itself has no C++ code.
  $CXX $CFLAGS $LIB_FUZZING_ENGINE $t.o kolmo_fuzz.o build/libkolmoarch.a build/libvendor.a \
    -lpthread -lm -o "$OUT/$t"
done
