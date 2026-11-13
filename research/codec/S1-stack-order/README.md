# Reproduce S1: codec sequence and placement

Read PROTOCOL.md and RESULTS.md. This directory is research-only and emits explicit KST1 frames, NOT Kolmo production frames. The normal source and decoder remain unchanged. No dataset is new validation.

Linux, Python standard library, GCC, Clang, liblzma development files and normal Kolmo vendor prerequisites are required. Use a disposable checkout on the authorized branch. Commands below use a new directory outside the checkout; set REPO and R3 to real absolute paths.

```sh
REPO=$PWD
R3=/absolute/path/Kolmo_R3_Fresh_Validation_43276455708.zip
WORK=/tmp/kolmo-s1-new
make -j2 build/libvendor.a CFLAGS='-O3 -DNDEBUG -fno-plt -ffp-contract=off'
python3 research/codec/S1-stack-order/prepare.py "$R3" "$WORK"
python3 research/codec/S1-stack-order/build.py --repo "$REPO" --output "$WORK/build-release"
python3 research/codec/S1-stack-order/build.py --repo "$REPO" --output "$WORK/build-asan" --sanitize
"$WORK/build-release/stack" selftest
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$WORK/build-asan/stack" selftest
python3 -m unittest discover -s research/codec/S1-stack-order -p 'test_*.py' -v
python3 research/codec/S1-stack-order/run.py --binary "$WORK/build-asan/stack" --inputs "$WORK/inputs" --manifest "$WORK/evidence/INPUTS.json" --output "$WORK/sanitized" --rounds 0 --seed 2026111300
python3 research/codec/S1-stack-order/run.py --binary "$WORK/build-release/stack" --inputs "$WORK/inputs" --manifest "$WORK/evidence/INPUTS.json" --output "$WORK/first" --rounds 7 --seed 2026111301 --reference "$WORK/sanitized"
python3 research/codec/S1-stack-order/run.py --binary "$WORK/build-release/stack" --inputs "$WORK/inputs" --manifest "$WORK/evidence/INPUTS.json" --output "$WORK/confirmation" --rounds 5 --seed 2026111302 --reference "$WORK/first"
python3 research/codec/S1-stack-order/analyze.py "$WORK"
gcc -O3 research/codec/S1-stack-order/leaf_reader.c "$WORK/build-release/bridge.o" build/libvendor.a -llzma -lpthread -lm -lstdc++ -o "$WORK/build-release/leaf_reader"
python3 research/codec/S1-stack-order/reference.py "$WORK" --reader "$WORK/build-release/leaf_reader"
```

The linked entropy reader is reused; Python supplies a separate frame parser and modular integer inverse. No Python execution or file I/O is inside native timers. Output verification is outside timers; codec CRC and allocation/free operations are inside. Timing arms are randomized; encode and decode rounds are separate. Every configured arm must succeed, not disappear from the denominator.

## KST1 frame contract

All fixed-width fields are little-endian. Input cap 16 MiB; stage cap 64 MiB; maximum two coder stages per group and one outer-frame wrapper. Base fields: magic KST1 (4), placement (1), original length (8), original CRC32C (4), parameter length (8), rest length (8), residual length (8), residual width (1), original plane flag (1), group count (4), parameters, rest bytes, then groups. Group: stage count (1), encoded length (8), nested stage payload. Stage: codec ID (1), pre-stage length (8), coded length (8), coded bytes. Codec letters H/R/Z/L are only KST1 research identifiers; zero is valid only for empty storage. Groups concatenate before layout reversal and the pinned integer inverse.

Placement 0 codes original bytes after type inspection. Placement 1 uses interleaved residual values; 2 concatenates planes; 3 codes each plane separately. Placement 4 stores original length/CRC plus inner-frame length (8), encoded length (8) and one Z-coded complete base frame. Decoder rejects trailing bytes, inconsistent lengths, illegal IDs/depth and reconstruction failures. Direct zstd control frames have no KST1 wrapper.

No codec-stage failures are silently substituted with another coder. Huffman's existing internal stored/repeated-byte chunks are retained and counted. The vectors materialize each stage, so this is not a zero-copy implementation. Hashes and the source pin make the experiment reproducible; absolute timings depend on the machine.
