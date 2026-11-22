#!/bin/bash -eu
# Campaign 3 (rc3): builds every libFuzzer target (run from the repo root, Linux, clang with libclang-rt).
#   MODE=fuzz (default): -fsanitize=fuzzer,address,undefined           -> build/fuzz/<target>
#   MODE=cov:  -fsanitize=fuzzer -fprofile-instr-generate -fcoverage-mapping -> build/fuzzcov/<target>
# Differences from campaign 2 (fuzz/campaign/build.sh on claude/rc2-fuzz2-20261116):
#   - rc3 layout: src/kolmo.c includes src/{arch,core,plan,transforms,codecs,algo,api}; the parts compiled on their own
#     (the Makefile's ARCHSRC: the dispatched kernels, LZP/QLFC/libsais subset/par, the codec-18 stream, the .S
#     kernels) are compiled here WITH the sanitizers and coverage, since they hold decoders (codec 18's whole pipeline);
#   - libbsc is gone (no C++ in the library); since version 12 no vendored codec either;
#   - new targets: fuzz_codecs (codecs 17-26 directly), fuzz_ext (KEXT, IDX1 queries, DDP1 archives),
#     fuzz_profile (KPF1, profile frames, --target parser).
# Every frame-decoder target is also built as <target>_nocm (-DKOLMO_FUZZ_NO_CM: the context-mixing decoders return an
# error at once, so the fuzzer is not held at ~1 exec/s by the model setup; src/codecs/cm.c, cm_image.c).
# Since container version 12 no codec library is linked (zstd, Brotli, LZ4, liblzma and xxHash were removed): every
# decoder is Kolmo's own and instrumented. fuzz_dict was removed with the dictionaries.
# libFuzzer is C++: the fuzz binaries alone link its runtime (clang's -fsanitize=fuzzer adds it).
MODE=${MODE:-fuzz}
CC=${FUZZ_CC:-clang}
DEFS="-Isrc"
if [ "$MODE" = cov ]; then
  OUT=build/fuzzcov; CF="-g -O1 -fprofile-instr-generate -fcoverage-mapping -ffp-contract=off"; FZ="-fsanitize=fuzzer"
else
  OUT=build/fuzz; CF="-g -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -ffp-contract=off"
  FZ="-fsanitize=fuzzer"
fi
mkdir -p "$OUT/arch"
# the separately compiled library parts, from the Makefile itself
ARCHSRC=$(printf 'fz_p:\n\t@echo $(ARCHSRC)\n' | make -s -f Makefile -f - fz_p)
J=${JOBS:-$(nproc)}
w() { while [ "$(jobs -rp | wc -l)" -ge "$J" ]; do wait -n; done; }
AOBJ=""
for s in $ARCHSRC; do
  o="$OUT/arch/$(echo "${s%.*}" | tr / _).o"; AOBJ="$AOBJ $o"
  # -fsanitize=fuzzer-no-link: coverage instrumentation without a second main
  $CC $CF -fsanitize=fuzzer-no-link $DEFS -c "$s" -o "$o" & w
done
wait
LIBS="$AOBJ -lpthread -lm"
XD=src/experimental
b() { local t=$1; shift; echo "build $OUT/$t"; $CC $CF $FZ $DEFS "$@" -o "$OUT/$t"; }
# public-API harnesses: linked with src/kolmo.c
for t in fuzz_decompress fuzz_limited fuzz_roundtrip fuzz_stream fuzz_stream_limited; do
  b $t -Ifuzz fuzz/$t.c src/kolmo.c $LIBS & w
  b ${t}_nocm -DKOLMO_FUZZ_NO_CM -Ifuzz fuzz/$t.c src/kolmo.c $LIBS & w
done
b fuzz_nplane -Ifuzz fuzz/fuzz_nplane.c src/kolmo.c $LIBS & w
# harnesses that include src/kolmo.c (static decoders)
for t in fuzz_codecs fuzz_ext fuzz_profile; do
  b $t -Ifuzz fuzz/$t.c $LIBS & w
  b ${t}_nocm -DKOLMO_FUZZ_NO_CM -Ifuzz fuzz/$t.c $LIBS & w
done
# experimental research decoders (not part of the library)
b fuzz_krc   -I$XD fuzz/fuzz_krc.c $XD/krc.c -lpthread -lm & w
b fuzz_kbcx  -I$XD fuzz/fuzz_kbcx.c $XD/kbc.c $XD/kbx.c $XD/krc.c -lpthread -lm & w
b fuzz_fastq -I$XD fuzz/fuzz_fastq.c $XD/frontier_fastq.c $XD/frontier_entropy.c $XD/frontier_runs.c -lm & w
wait
ls "$OUT" | grep -v '^arch$'
