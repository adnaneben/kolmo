# Assembly build control and parity on Apple M5

On seven pinned project files, the documented C-only build still used hardware CRC, a NEON decimal formatter and a NEON word scanner. The fix in `819f33d77deb3e258dcfdbf6bf6c1152dacbabe5`, on #107 `31fd3acd922490143059ac2438e988e38d89be5f`, brings direct intrinsic kernels under the same compile-time switch as separately compiled assembly. Every pinned input restored byte for byte.

| Count across seven valid frames, single-threaded | #106 default | #106 with KOLMO_CPU=c | #106 with private force-C switch | #106 built NO_ASM=1 | Fixed NO_ASM=1 |
|---|---:|---:|---:|---:|---:|
| Bytes through ARM CRC | 3,080 | 3,080 | 3,080 | 3,080 | 0 |
| Bytes through portable CRC | 0 | 0 | 0 | 0 | 3,080 |
| Values inside NEON decimal vector body | 69,806 | 69,806 | 69,806 | 69,806 | 0 |
| Scalar decimal formatter calls | 2,334 | 2,334 | 2,334 | 2,334 | 72,140 |
| 16-byte NEON iterations in the word remainder scanner | 340 | 340 | 16,874 | 16,874 | 0 |
| Decoded bytes produced by the bulk word NEON call | 98,012 | 98,012 | 0 | 0 | 0 |

## Implementation and controls

- Exact #106 source is `8d23095b2f09d5c9e3c3425123bea63641ca5d71`. Only core/blocks.c, core/compress.c and an optional FASTQ test changed between #106 and the #107 base. The observed before counts above use #106. A separate compile guard test also reproduced the bug on unmodified #107 for both arm64 and x86_64.
- On ARM the stock x86 dispatch table reports the C level. Setting `KOLMO_CPU=c` affects that table. It does not control the independent ARM code. The seven original numeric/word/image frames processed 29,999 values through the graph INT4 NEON function family (which may contain scalar tails), 12,000 DEC64 values and 9,216 IMG8 payload bytes through their NEON functions. The private `kolmo_force_c` test variable switched these three and bulk word to the C path. Decimal, the remainder scanner and CRC were selected independently. See src/arch/kernels.h, src/arch/arch.h and docs/LAYOUT.md.
- Unpatched output parity alone hides that the original C-only build took intrinsic paths. CI now compiles the whole shipping src/kolmo.c with ISA intrinsic headers replaced by errors in its actual `NO_ASM=1` step. The test passes on the fixed Apple arm64 compiler and cross-preprocessing/compiling x86 baseline and x86 with requested SSE4.2/AVX2 enabled. The previous commit fails that test on both architectures.
- The x86 dispatch-table families include MED8, planes, mixer, video and IMG16. Direct gates also select SSE2 and AVX2 graph inverse, SSE2 and AVX2 scanning, AVX2 decimal and SSE4.2 CRC. Only cross-compilation of the changed source ran locally on x86; there is no x86 runtime on this machine. The x86 timing and forced-path study in benchmark/rc3/simd-x86/RESULTS.md is historical. No new x86 timings or speed claims follow from this experiment.
- A clean regular build preserves the accelerator-enabled preprocessed shipping source byte for byte on arm64 baseline/native and x86_64 baseline/AVX2+SSE4.2. The regular arm64 CLI built with matched `ARCH=` flags is also bit-identical to its #107 parent (SHA-256 `60f36534899ff216fb906a2ba09586a8a3223efee5f7a9791e8e5080d5c7d541`). The compiler may auto-vectorize portable C. The build contract covers first-party handwritten ISA kernels in the ordinary shipping library, independent of compiler instruction selection. Research-only build modes and all optional standalone codecs were outside this check.

## Verification

The fixed `make ARCH= NO_ASM=1` CLI passed 274 release-manifest frozen frames including streams. The C library suite passed 357 frozen frames. These sets overlap. All 152 encoded-frame determinism cases passed, the CRC/hash suite passed 28,182 checks and the portable kernel suite passed 25,328 checks. The assembly test explicitly reported zero assembly kernels compiled. The fixed counter probe restored the seven pinned files once each and reported zero native counts in the table above. The default Apple test-dispatch also passed 28,182 checks.

`measurements.csv`, `results.json` and their counterparts under `noasm-before` retain pre-fix input/output hashes and counts. The five arms in each set cover stock, instrumented native, KOLMO_CPU=c, internal force-C and an explicitly instrumented portable reference. The stock binary is shared between the two sets. The private portable-reference arm is an experimental comparator. `instrument.py` adds counters and controls only to an isolated #106 copy. Build it with `build.sh REPO EMPTY_DIR`; reproduce the pre-fix C-only probe by passing `NO_ASM=1` in the environment to that script. `measure.py REPO STOCK_CLI PROBE_CLI OUTPUT_DIRECTORY` checks pin hashes and original bytes. No instrumented binaries ship.

`noasm-after/probe.py instrument WORKTREE` instruments a private copy of the fixed source without overriding its path choice. Build that copy with `make ARCH= NO_ASM=1 build/kolmo` and run `noasm-after/probe.py measure REPO PROBE_CLI OUTPUT_CSV`. `noasm-after` holds the raw hashes, compiler logs and pass counts.
