#!/bin/bash -eu
# OSS-Fuzz build script. Builds each fuzz target with the provided $CC/$CXX/$CFLAGS/$LIB_FUZZING_ENGINE. Run from the
# repo root. Everything is compiled with -ffp-contract=off, as in every other build. No codec library is needed: every
# coder is Kolmo's own (container version 12).
make -j"$(nproc)" build/libkolmoarch.a ARCH=
for t in fuzz_decompress fuzz_roundtrip; do
  $CC $CFLAGS -ffp-contract=off -Isrc -c fuzz/$t.c -o $t.o
  $CC $CFLAGS -ffp-contract=off -Isrc -c src/kolmo.c -o kolmo_fuzz.o
  # Linked with $CXX only because the fuzzing engine (libFuzzer) is C++; Kolmo itself has no C++ code.
  $CXX $CFLAGS $LIB_FUZZING_ENGINE $t.o kolmo_fuzz.o build/libkolmoarch.a \
    -lpthread -lm -o "$OUT/$t"
done
