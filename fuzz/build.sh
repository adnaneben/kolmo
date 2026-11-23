#!/bin/bash -eu
# libFuzzer build of Kolmo's fuzz targets, used by OSS-Fuzz (packaging/oss-fuzz/build.sh calls it) and by the
# continuous fuzzing workflow (.github/workflows/fuzz.yml). Run from the repo root with the OSS-Fuzz environment:
#   CC, CXX            clang, clang++
#   CFLAGS             sanitizer and coverage flags, e.g. -fsanitize=address,undefined,fuzzer-no-link
#   LIB_FUZZING_ENGINE the engine to link, e.g. -fsanitize=fuzzer
#   OUT                where the targets, seed corpora and options files go
#   SANITIZER          (optional) "memory" builds the C references only (NO_ASM=1): MSan cannot see the assembly
#                      kernels' stores and would report their output as uninitialised
# Everything is compiled with -ffp-contract=off, as in every other build. No codec library is needed: every coder is
# Kolmo's own (container version 12). The separately compiled coders (build/libkolmoarch.a) are built with $CFLAGS
# too, so the sanitizer and coverage flags reach KLZ, KLZH2, KLZMA, khuf, krans and the block-sorting code.
#
# Targets (the decoders a user's input reaches, and the round trips):
#   fuzz_decompress      kolmo_decompress on arbitrary frames (header CRC fixed up after mutation)
#   fuzz_limited         kolmo_decompress_limited and kolmo_read_range
#   fuzz_stream          the CZNS streaming decoder
#   fuzz_stream_limited  the streaming decoder with output and memory limits
#   fuzz_roundtrip       every level must round-trip arbitrary input
#   fuzz_native          Kolmo's own coders on raw streams (KLZ, KLZH, KLZH2, KLZMA, khuf, krans)
#   fuzz_nplane          the plane-context graph leaf (codec 15)
#   fuzz_codecs          the block and leaf decoders called directly (codecs 16-27, graph blocks)
#   fuzz_ext             KEXT trailer, IDX1 index and queries, DDP1 archives
#   fuzz_profile         the --target parser and cost evaluator
# The last three include src/kolmo.c themselves and are built with KOLMO_FUZZ_NO_CM (the context-mixing decoders
# return at once; their setup costs seconds per input and would read as timeouts). The research decoders
# (fuzz_fastq, fuzz_kbcx, fuzz_krc) are not part of the shipped library and are not built here.
#
# Seed corpora: version-12 frames of every mode (fuzz/corpus/v12, regenerate with fuzz/make_v12_seeds.c) and the
# frozen frames of every release (tests/v12, COMPATIBILITY.md) for the frame targets; the raw inputs of tests/v12 for
# the round trip; fuzz/corpus/native for fuzz_native.
NO_ASM=; DEFS=
if [ "${SANITIZER:-}" = memory ]; then NO_ASM=1; DEFS=-DKOLMO_NO_ASM; fi
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu)
FLAGS="$CFLAGS -ffp-contract=off $DEFS -Isrc -Ifuzz"
make -j"$JOBS" build/libkolmoarch.a ARCH= NO_ASM="$NO_ASM" CFLAGS="$CFLAGS"
$CC $FLAGS -c src/kolmo.c -o kolmo_fuzz.o
# Linked with $CXX only because the fuzzing engine (libFuzzer) is C++; Kolmo itself has no C++ code.
for t in fuzz_decompress fuzz_limited fuzz_stream fuzz_stream_limited fuzz_roundtrip fuzz_nplane; do
  $CC $FLAGS -c fuzz/$t.c -o $t.o
  $CXX $CFLAGS $LIB_FUZZING_ENGINE $t.o kolmo_fuzz.o build/libkolmoarch.a -lpthread -lm -o "$OUT/$t"
done
$CC $FLAGS -c fuzz/fuzz_native.c -o fuzz_native.o
$CXX $CFLAGS $LIB_FUZZING_ENGINE fuzz_native.o build/libkolmoarch.a -lpthread -lm -o "$OUT/fuzz_native"
for t in fuzz_codecs fuzz_ext fuzz_profile; do
  $CC $FLAGS -DKOLMO_FUZZ_NO_CM -c fuzz/$t.c -o $t.o
  $CXX $CFLAGS $LIB_FUZZING_ENGINE $t.o build/libkolmoarch.a -lpthread -lm -o "$OUT/$t"
done

# Seed corpora (zip -j stores flat names).
seeds() {   # seeds NAME FILE... : $OUT/NAME_seed_corpus.zip
  local z="$OUT/$1_seed_corpus.zip"; shift
  rm -f "$z"
  [ $# -gt 0 ] && printf '%s\n' "$@" | zip -q -j -@ "$z"
  return 0
}
mapfile -t FRAMES < <(find fuzz/corpus/v12 tests/v12 -type f -name '*.kolmo' 2>/dev/null | sort)
mapfile -t RAW < <(find tests/v12 -path '*/in/*' -type f -size -256k 2>/dev/null | sort)
mapfile -t NATIVE < <(find fuzz/corpus/native -type f 2>/dev/null | sort)
# the release folders of tests/v12 repeat the input names, so each copy is named after its path
tmp=$(mktemp -d)
for f in "${FRAMES[@]}"; do cp "$f" "$tmp/${f//\//_}"; done
mapfile -t FRAMES_FLAT < <(find "$tmp" -type f | sort)
seeds fuzz_decompress "${FRAMES_FLAT[@]}"
seeds fuzz_limited "${FRAMES_FLAT[@]}"
seeds fuzz_stream "${FRAMES_FLAT[@]}"            # single frames are rejected early; the CZNS ones reach the walk
seeds fuzz_stream_limited "${FRAMES_FLAT[@]}"
seeds fuzz_roundtrip "${RAW[@]}"
seeds fuzz_native "${NATIVE[@]}"
rm -rf "$tmp"
# fuzz_nplane, fuzz_codecs, fuzz_ext and fuzz_profile take their own input layouts and start from random inputs (their
# round-trip modes make valid streams). Every target: inputs up to 1 MiB, 2.5 GiB of memory, 60 s per input.
for t in fuzz_decompress fuzz_limited fuzz_stream fuzz_stream_limited fuzz_roundtrip fuzz_native fuzz_nplane \
         fuzz_codecs fuzz_ext fuzz_profile; do
  printf '[libfuzzer]\nmax_len = 1048576\nrss_limit_mb = 2560\ntimeout = 60\n' > "$OUT/$t.options"
done
