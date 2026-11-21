# TEST_RESULTS — candidate C0 = 2293c857158e55cdc009abc9654a5e5dbb676f6d (code identical to rc3 5071f388)

Machine: Apple M5, 10 cores (4P+6E), 24 GiB, macOS 27.2, Apple clang 21.0.0, GNU Make 3.81, cargo 1.99.1, Python 3.9.6.
Every step ran through tools/run_logged.py (argv, cwd, env, stdout, stderr, real exit code; own log dir; KOLMO_* cleared; TMPDIR per step).

| step | exit | seconds | result line |
|---|---|---|---|
| 0000-build-all | 0 | 13.26 |  |
| 0001-check | 0 | 543.21 | ALL PASSED |
| 0002-check-rc | 0 | 1423.05 | test_profile: all checks passed |
| 0003-check-sanitizers | 0 | 48.8 | PARALLEL TESTS PASSED (144 concurrent table, archive and plain round trips) |
| 0004-check-dna-cm-tar | 0 | 29.25 | Tar file/memory archives, member mapping fallback and cleanup passed |
| 0005-experimental | 0 | 219.53 | KFAM TESTS PASSED |
| 0006-lib | 0 | 12.24 |  |
| 0007-py-binding | 0 | 3.65 | ALL PASSED |
| 0008-py-concurrent-dict | 0 | 0.08 | DICTIONARY CONCURRENCY PASSED (80 mixed dictionary/plain round trips) |
| 0009-cargo-test | 0 | 17.37 | test result: ok. 1 passed; 0 failed; 0 ignored; 0 measured; 0 filtered out; finished in 0. |
| 0010-determinism-portable | 0 | 114.23 | 122 frames checked; OK |
| 0011-bindings-diff-python | 0 | 0.1 |  |
| 0012-bindings-diff-rust | 0 | 0.06 |  |
| 0013-cref-build | 0 | 14.08 |  |
| 0014-cref-test-dispatch | 0 | 0.72 |  |
| 0015-cref-test-asm | 0 | 0.24 | ASM KERNEL TESTS PASSED (0 assembly kernels checked: NO_ASM build or no kernels for this I |
| 0016-determinism-c-reference | 0 | 110.96 | 122 frames checked; OK |

Python binding: KOLMO_LIB is the first path the binding tries (bindings/python/kolmo.py:18); the loaded library was
validate-portable-c0/build/libkolmo.dylib, sha256 9e725cf732ae42bf…; confirmed by loading it and printing the handle path.

## Sanitizer coverage (what is and is not instrumented)
- `check-rc` / `check-sanitizers` *-asan targets: test drivers and kolmo.c are built with ASan+UBSan, but most link
  build/libkolmoarch.a and build/libvendor.a compiled with the normal -O3 flags (uninstrumented SIMD/arch kernels and
  zstd/brotli/lz4/xz). test-format2-asan, test-meta-asan and test-features-b-asan compile $(ARCHSRC) with SANFLAGS.
- Added: validate-fullasan-c0/build/kolmo — every object (kolmo, arch, vendor) compiled with
  -O1 -g -fsanitize=address,undefined (100/100 compile lines), running the C5 native/corruption matrix (quick set)
  with ASAN exitcode 99 / UBSAN exitcode 98 so sanitizer reports cannot pass as clean rejections.
- LeakSanitizer is not available on Darwin arm64; leak checks are NOT covered on this Mac (CI's Linux job has them).

## Additional correctness evidence (C0)
- C5 native/mode matrix (portable build): 5,590 rows, 0 failures (round trips with KOLMO_NATIVE unset at decode,
  1 vs 4 thread identity, 3,354+ truncation/bit-flip probes rejected or exact).
- Format compatibility matrix: 7,344 cross-generation decodes, 0 silent misdecodes, 0 crashes.
- Release (-O3 -mcpu=native) screening binaries: determinism 122/122 frames for rc3, a17d6657, W4 bwt/cm/docs/entropy/lz.

## Not covered / pending
- Remote CI on the fixed workflow (needs the review branch pushed; not done — see INTEGRATION_PLAN).
- x86-64 (Linux/Intel macOS) execution on this machine: no Rosetta.
- GPU=1 build: errors in rc3 by design (C7); GPU research is out of the T1 scoreboard.

Assembly coverage: the portable build's check-rc ran test-asm twice ("1 assembly kernel checked": hist_aarch64.S, the
only .S kernel for arm64); the NO_ASM C-reference lane checked 0 kernels by design and matched the same 122
determinism digests, so the C references and the assembly path produce identical frames on this Mac.

Fully instrumented CLI result (validate-fullasan-c0, sha256 7b8e4e28…): quick C5 matrix (KOLMO_NATIVE unset/all/all,strong,
all modes and -x levels, 1 and 4 threads) 4,342 rows, 0 failures, 0 sanitizer exits (codes 98/99); 3,354
truncation/bit-flip probes: 3,261 rejected (exit 1), 93 decoded to the exact input, none crashed or misdecoded.

---

# TEST_RESULTS addendum, 2026-11-20 — candidate C1 = 302e31a6a0c9be45f49201bfb1a45221b186bbf1

C1 is fb58d3d6 (review fixes) + a17d6657 (W3 decmap/phrase) + 5ce318ad (W4 bwt), on the local branch `review/rc3-integration-J62kQD`. The C0 results above are unchanged.

## Phase E on C1 (Apple M5, Apple clang; portable `ARCH=` and C-reference `NO_ASM=1`)
All 17 steps exited 0. Log: `logs/phase-e-c1/`, summary `logs/phase-e-c1.summary`.
- Builds: `make all`, `make lib` (libkolmo sha256 5b7bc851…), and the NO_ASM C-reference build.
- Checks: `make check`, `check-rc`, `check-sanitizers`, frontier-dna / cm-compact / tar-files, and experimental / kfam / kaud / kbin.
- Bindings: Python binding test, concurrent-dict test, `cargo test` (Rust), and binding source diffs (Python and Rust sources match `src/` and `vendor/`).
- Determinism: 10/10 frame digests are identical to C0 in both the portable and C-reference builds (`determinism-portable-c1`, `determinism-c-reference-c1`). C-reference `test-dispatch` and `test-asm` pass.

## Release build of C1 (`-O3 -mcpu=native`, `final-builds/c1/build/kolmo`, sha256 badac9ea…)
- **Native/mode matrix:** 5,590 rows, 0 failures (`native-matrix-c1-release`). All 1,560 frame hashes are identical to C0.
- **Compatibility matrix:** 9,516 cross-generation decodes, 0 silent misdecodes, 0 crashes (`compat-matrix-c1`; see FORMAT_COMPATIBILITY_MATRIX addendum).
- **Holdout benchmark:** 1,785 accepted trials, every decode byte-verified, all outputs deterministic.

## Remote CI (GitHub Actions) on the review branch
CI ran on fb58d3d6, the review branch head; this is **not** C1.
- Run 44539684922 on 2bdbbb72: fuzz-smoke failed to compile `fuzz/fuzz_codecs.c` because `kv_inverse` and `gr6_compress` had stale signatures. The bug was already in rc3; fixed in fb58d3d6.
- Run 44556681566 on fb58d3d6, attempt 1: macOS was cancelled (runner capacity).
- Run 44556681566, attempt 2: all 5 jobs passed (build-test on ubuntu-latest, ubuntu-24.04-arm and macos-latest; sanitizers; fuzz-smoke). https://github.com/adnaneben/kolmo/actions/runs/44556681566

## Findings (not changed by the review)
- `src/plan/planner.c:158`: clang `-Wnonnull` warning on `memmem` when it may receive NULL. Review it before release.

## Still open
- **CI on C1 itself:** the integration branch is not pushed; that needs owner approval.
- x86-64 execution, including decmap's AVX2 path, which is reached only on x86, and Linux huge pages for W4 bwt.
- GPU=1 builds.

## CI on C1 itself (added 2026-11-21)
GitHub Actions run 44810049864 on review/rc3-integration-J62kQD @ 302e31a6 (C1): **all 5 jobs success** — build-test
ubuntu-latest (x86-64), ubuntu-24.04-arm, macos-latest; sanitizers; fuzz-smoke.
https://github.com/adnaneben/kolmo/actions/runs/44810049864 . This closes the "CI on the final code" gate for C1 and gives
x86-64 build+test coverage (AVX2 decmap path compiled and tested by `make check` on ubuntu-latest).

# TEST_RESULTS addendum, 2026-11-21 — candidate C2 = 25bedda80b7320268e3c35963a3e5f8b9be2cecb
C2 = C1 302e31a6 + 91ae8cee (KOLMO_NATIVE v11 only when a native id is written) + b902e04a (W4 docs encoder tuning,
no async writer) + 69f391f8 (brotli mmap allocator) + a555c05d (W4 cm, CM model nibble 9, reserved-nibble rules).
Phase E (`logs/phase-e-c2/`, summary `logs/phase-e-c2.summary`): **all 17 steps exit 0** — make all/check/check-rc/
check-sanitizers/dna-cm-tar/experimental/lib, Python binding, concurrent dict, cargo test, bindings source diffs,
determinism (portable and C-reference), NO_ASM C-reference build with test-dispatch and test-asm.
Release binary: `final-builds/c2/build/kolmo`, sha256 31b9e558…, -O3 -mcpu=native (`final-builds/BINARIES.sha256`).

## CI on C2 and its parts (GitHub Actions, 2026-11-21) — all 5 jobs success on each
(build-test ubuntu-latest x86-64 / ubuntu-24.04-arm / macos-latest, sanitizers, fuzz-smoke)
| branch | sha | run |
|---|---|---|
| review/c2-integration-J62kQD (**C2**) | 25bedda8 | https://github.com/adnaneben/kolmo/actions/runs/44818901610 |
| review/c2-cm-J62kQD | a555c05d | https://github.com/adnaneben/kolmo/actions/runs/44818775152 |
| review/c2-mem-J62kQD | 69f391f8 | https://github.com/adnaneben/kolmo/actions/runs/44817351203 |
| review/c2-native-J62kQD | 91ae8cee | https://github.com/adnaneben/kolmo/actions/runs/44814515869 |
| review/c2-docs-J62kQD | b902e04a | https://github.com/adnaneben/kolmo/actions/runs/44814513359 |
| review/rc3-audit-review-… (review branch) | ff1a11f8 | https://github.com/adnaneben/kolmo/actions/runs/44810046960 |

# TEST_RESULTS addendum, 2026-11-21 — C3 image branch = cbb40b5d (C2 + kimg, codec 31); not the recommended candidate
Phase E (`logs/phase-e-c3/`): **all 17 steps exit 0**, including check-sanitizers, bindings, cargo, determinism in the
portable and C-reference builds, and NO_ASM test-dispatch/test-asm.
Compatibility (`kimg-compat-c3`): C3 decodes all 972 C2 compat frames with the same outcome as C2. The 18 kimg frames round-trip exactly.
210/210 old-decoder attempts are refused, with 0 misdecodes and 0 crashes.
CI run 44849402705: **all 5 jobs success** (https://github.com/adnaneben/kolmo/actions/runs/44849402705).
