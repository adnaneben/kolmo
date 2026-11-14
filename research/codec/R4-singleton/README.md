# R4: singleton rANS decoding as a compatible implementation layer

Research-only development experiment, 13 November 2026. This directory does not edit production code, change a wire ID, or promote a mode. It stages one decoder shortcut in temporary copies of the pinned full source. See `RESULTS.md` and `EVIDENCE.json` for executed evidence and limitations.

## Scope

The existing normalized rANS table can represent one symbol with frequency 65,536. R1/R2/R3 already used that representation. The pinned decoder nevertheless fills its 64 KiB slot table and executes one inverse step for every output symbol. R4 recognizes this case during the existing frequency-table parse, validates the same terminal-state and payload-consumption conditions, and emits the repeated byte with `memset`.

No compressed byte changes. The generic nonsingleton loop is retained. Allocation is deliberately unchanged so this test does not conflate decoding work with a second memory-management change. The same inactive direct-allocation test hook is present in both staged builds. The separate `candidate.diff` shows only the intended optimization, not the test hook.

For frequency M=2^16 and cumulative start zero, the inverse update is

```
X' = M * floor(X/M) + (X mod M) = X.
```

A valid singleton chunk therefore consumes exactly its four 64-bit states (32 bytes), each equal to R64_L; it consumes no renormalization words. The shortcut checks both conditions, as well as the original chunk/table bounds, before filling output. Rejected inputs need not leave identical partial bytes in the destination; no accepted output or status classification changes in the tested cases.

## Reproduce

Linux, GCC, Clang, Python standard library, and the repository's normal build prerequisites are required. Use an isolated disposable checkout with the verified repaired core blob `e06467c8872d494b97270811bedf6fd0bac2aa31` and graph blob `91ad067dc583b9d9e6f060f214a0f7c402bb4a31`.

```sh
make -j2 CFLAGS='-O2 -fno-plt' build/kolmo build/libvendor.a
python3 research/codec/R4-singleton/build.py --source . --output /tmp/r4-build --variant base
python3 research/codec/R4-singleton/build.py --source . --output /tmp/r4-build --variant fast
python3 research/codec/R4-singleton/build.py --source . --output /tmp/r4-build --variant asan
/tmp/r4-build/probe-base selftest /tmp/r4-base-golden
/tmp/r4-build/probe-fast selftest /tmp/r4-fast-golden
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  /tmp/r4-build/probe-asan selftest /tmp/r4-asan-golden
python3 -m unittest discover -s research/codec/R4-singleton -p 'test_*.py' -v
python3 research/codec/R4-singleton/run.py --source . --build /tmp/r4-build \
  --artifact /absolute/path/Kolmo_R3_Fresh_Validation_43276455708.zip \
  --output /tmp/r4-new-replay
```

The replay output directory must be new. The required archived R3 evidence digest is checked before reading data. `run.py` verifies the archived manifest, wraps the existing residual payload and KG_INT parameters in an ordinary version-7 CZN1 frame, and uses the unchanged production CLI to reconstruct the complete derived input. It does not rerun or retune prediction or compression. Replays are development evidence, not new-source validation.

The core/harness are sanitizer-instrumented; the linked vendor archive is not. The generated candidate decoder exists only in the temporary build directory. The normal production executable remains unmodified.

## Retained evidence

`results/timings.csv.xz` contains all 504 timed batches (7 and 5 rounds, 7 RANS inputs, 2 cache regimes, 3 arms). `timings_schema.json` maps compact integer columns and includes both compressed and uncompressed digests. `summary.json` gives every per-input median, including identical-baseline controls. Each batch reports nanoseconds; divide by its batch count for per-decode values. No timing sample was removed.

The seventh/eighth distinction matters: all eight archived inputs passed full-frame/CLI correctness, but only the seven RANS-coded inputs entered this decoder's timing comparison. HUF decoding was not changed. Displacement is a fixed 32 MiB sweep before a batch, not guaranteed cold LLC/storage or a sweep before each repeated decode. Full decoding includes graph reconstruction, metadata, checksums and core allocations; leaf timing does not.

Verify the retained timing ledger and every saved median without rerunning benchmarks:

```sh
python3 research/codec/R4-singleton/verify_saved.py
```
