#!/bin/bash -eu
# OSS-Fuzz build script. Builds each fuzz target with the provided $CC/$CXX/$CFLAGS/$LIB_FUZZING_ENGINE. Run from the
# repo root. Everything is compiled with -ffp-contract=off, as in every other build. No codec library is needed: every
# coder is Kolmo's own (container version 12). The separately compiled coders (build/libkolmoarch.a) are built with
# $CFLAGS too, so the sanitizer and coverage flags reach KLZ, KLZH2, KLZMA, khuf, krans and the block-sorting code.
# Seed corpora: fuzz/corpus/v12 (frames of every mode; regenerate with fuzz/make_v12_seeds.c) and tests/v12 (the frozen
# frames) for the frame targets, fuzz/corpus/native for fuzz_native.
make -j"$(nproc)" build/libkolmoarch.a ARCH= CFLAGS="$CFLAGS"
$CC $CFLAGS -ffp-contract=off -Isrc -c src/kolmo.c -o kolmo_fuzz.o
# Linked with $CXX only because the fuzzing engine (libFuzzer) is C++; Kolmo itself has no C++ code.
for t in fuzz_decompress fuzz_roundtrip fuzz_limited; do
  $CC $CFLAGS -ffp-contract=off -Isrc -Ifuzz -c fuzz/$t.c -o $t.o
  $CXX $CFLAGS $LIB_FUZZING_ENGINE $t.o kolmo_fuzz.o build/libkolmoarch.a -lpthread -lm -o "$OUT/$t"
done
$CC $CFLAGS -ffp-contract=off -Isrc -c fuzz/fuzz_native.c -o fuzz_native.o
$CXX $CFLAGS $LIB_FUZZING_ENGINE fuzz_native.o build/libkolmoarch.a -lpthread -lm -o "$OUT/fuzz_native"
if [ -d fuzz/corpus/v12 ]; then
  (cd fuzz/corpus/v12 && zip -q -j "$OUT/fuzz_decompress_seed_corpus.zip" ./* ../../../tests/v12/*.kolmo)
  cp "$OUT/fuzz_decompress_seed_corpus.zip" "$OUT/fuzz_limited_seed_corpus.zip"
fi
if [ -d fuzz/corpus/native ]; then (cd fuzz/corpus/native && zip -q -j "$OUT/fuzz_native_seed_corpus.zip" ./*); fi
