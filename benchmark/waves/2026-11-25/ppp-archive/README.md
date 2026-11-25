# DDP1 archive writer and selection qualification, 25 November 2026

The control is main `a1d39559686a9ec17c8614b9154a64d7b5b98672`. The candidate source tested here is `d76d6bbfcbe0c41eedf1f2f2b0fd4025de65609d`. These results cover the archive C API and CLI on Apple arm64.

- PR #95 aligns the archive member name limit in the writer and reader. The 4,096-byte case round trips and 4,097 bytes returns a limit error. The imported dedup.c blob before the further fixes was `dec4c439362cd0b9090996f2166410c2c56be93d`.
- PR #97 counts repeated requested names exactly once per argument, while continuing to visit matching archive entries in their stored order. The candidate uses the final CLI implementation at `e13622441978736361facadd132adcc1b7b4181e`, Git blob `1735ea7a4662330e5851f5e9b5ec5b63a8518e68`. The CLI integration test and 24 selection cases cover duplicate and missing names and the 16/17-argument allocation threshold.
- Eight independent first-time archive creations exposed a ThreadSanitizer data race on main's global Gear lookup table. The candidate uses the same pthread-once primitive as the core CRC and CPU dispatch tables. In the instrumented archive implementation all eight callers emitted identical archives and recovered their source bytes, with a clean TSan exit.
- One failed reserve of the per-member chunk reference list allowed main to report a successful 4,626-byte archive. With the same injected allocation failure the candidate returned `KOLMO_ERR_ALLOC` (-4). No format change was required.
- The reader checks signed zigzag displacements in the unsigned domain and bounds byte lengths against the member's remaining size before summing. The native, Rust and Python bundled copies match. This change is supported by code review, public extraction tests and the same-byte readbacks below.

## Verified outcomes

3,714 native feature checks, 3,744 ASan/UBSan feature checks, the archive safety and features CLI suites, 357 frozen v12 frames, and 416 pinned manifest files passed. The C ABI and stored format are unchanged. Native binaries built with the repository's default Makefile. Before and after binary SHA-256 are `b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f` and `21081779da45303e2d120aa3f60cd1387b24b7d722a41848463c76eb9a716d53`.

Twelve byte-for-byte before/after comparisons covered archive levels 1, 3, 9, T1/T4 and 1/4 KiB target chunks. Forty-eight writer/reader combinations recovered the exact member names, data, permissions and modification times. Each archive included repository prose, C source, a duplicate member payload and an empty file. See `inputs.txt` and `cross-reader.csv`. These tests establish compatibility on the exercised local cells. The previous #97 timing result applies to its qualified selection workload. The combined correctness changes have no newly established throughput advantage.

## Replay

From the repository root, set `D=benchmark/waves/2026-11-25/ppp-archive`. Use the unmodified binary compiled from the pinned main commit as `$BEFORE`.

```sh
make -j2 build/kolmo build/mkarchive build/test-features-b build/test-features-b-asan build/test-v12-corpus
./build/test-features-b
./build/test-features-b-asan 7
sh tests/test_features_b_cli.sh build/kolmo
sh tests/test_archive_safety.sh build/kolmo build/mkarchive
./build/test-v12-corpus tests/v12
python3 tests/check_v12_pinned.py
python3 "$D/selection-edges.py" build/kolmo
python3 "$D/cross-reader.py" "$BEFORE" build/kolmo
clang -O1 -g -fsanitize=thread -Isrc "$D/builder-check.c" build/libkolmo.a -lpthread -lm -o build/builder-check
./build/builder-check
./build/builder-check inject
git show a1d39559686a9ec17c8614b9154a64d7b5b98672:src/core/dedup.c > build/pre-ddp.c
clang -O1 -g -fsanitize=thread -Isrc -iquote src/core -Ibuild -DARCHIVE_IMPL='"pre-ddp.c"' "$D/builder-check.c" build/libkolmo.a -lpthread -lm -o build/builder-check-before
TSAN_OPTIONS=halt_on_error=1 ./build/builder-check-before
./build/builder-check-before inject
```

The two before checks are expected to fail. TSan instruments the archive source in the harness. The statically linked codec dependency uses the stock optimized build. The allocator test models a failed first reserve of the member reference buffer at its internal interface; it does not exhaust operating-system memory.

## Independent stream PR #93 validation

A separate checkout applied [#93](https://github.com/adnaneben/kolmo/pull/93) to the same pinned main. Its public header, API implementation, decoder, stream implementation, Makefile and dense-stream test matched their Git blob SHA-1s from PR head `39eed4db533224bae935754c2a6f1e5e99847bb9`. They are independent of the DDP1 archive candidate above.

The native Apple arm64 build passed the four dense-block admission boundaries, clipped-range and mixed CZN1/CZNS checks, and all 357 frozen frames. An additional 32 ordinary readbacks compared main and candidate across enwik8 and Silesia prefixes at 4/16 MiB, T1/T4 and 2 MiB/unrestricted budgets; all recovered SHA-256 exactly. See `stream-93-readbacks.csv`. The stock CLI and dense-stream boundary executable cross-built as x86-64 Mach-O using Apple clang. Native x86 execution and independent hosted timing are outstanding; this machine cannot run the x86 binaries. Keep #93 separate pending that qualification. Its documented memory budget covers codec work and large index structures. It does not cover the entire input member or establish an operating-system RSS ceiling.
