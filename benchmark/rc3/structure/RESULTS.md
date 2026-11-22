# rc3 structure lane: the source tree of docs/LAYOUT.md

Branch `claude/rc3-structure-20261116`, from `claude/release-rc3-20261116` (`2207da1`). Machine: cloud VM, Intel Xeon
@ 2.30 GHz, 4 vCPUs, Ubuntu 24.04, gcc 13.3 and clang 18. Output is unchanged: every frame of every mode is the same
byte for byte as rc2's (below).

## What moved where

`src/kolmo.c` (3559 lines) and the twelve `.inc` files were split by `benchmark/rc3/structure/split.py`, which cuts
rc2's `kolmo.c` by line ranges, checks every range against an anchor line, and fails if a non-blank line is left
over or used twice. The `.inc` files were moved with `git mv` (history and `git log --follow` kept). Code was not
rewritten, with four exceptions, all output-neutral and listed here:

1. **Codec table.** The three `switch` statements of `codec_bound`, `codec_compress` and `codec_decompress` became one
   `kolmo_codec` table per codec (`codecs/codec.h`: id, name, `bound`, `compress`, `decompress`, `mem_needed`) and a
   registry indexed by codec id (`codecs/codec.c`). The case bodies moved verbatim into the codec files. A missing
   function keeps the old default (bound → n, compress → 0, decompress → -1), so unknown or block-level ids
   (8-12, 16, 17 for compress) behave as before. `mem_needed` is NULL everywhere for now (docs/MEMORY-MODEL.md).
2. **CRC32C on Arm.** The `__crc32cd` loop became `crc32c_arm()` in `arch/aarch64/crc32c_arm.c`, called by
   `kolmo_crc32c` in `arch/crc32c.c`; the x86 path was only moved.
3. **NEON decimal formatter.** The NEON loop inside `fmt_slots` became `fmt_slots_neon()` returning the count it
   wrote (`arch/aarch64/decimal_neon.c`); `fmt_slots` calls it where the loop was.
4. **Integer inverse.** The SSE2 and AVX2 kernels moved from `kgraph.inc` to `arch/x86_64/int_inv_*.c`; the host
   byte-order switches (`FI_HOST_LE`, `FI_USE`) to `arch/cpu.h`; the dispatch `fi_vec4` stays in `transforms/graph.c`.

| rc2 | rc3 |
|---|---|
| `kolmo.c` 1-51 (headers, version) | `core/common.h` |
| `kolmo.c` 53-89 (x86 dispatch) | `arch/cpu.h` |
| `kolmo.c` 91-141 (CRC32C) | `arch/crc32c.c`, `arch/x86_64/crc32c_sse42.c`, `arch/aarch64/crc32c_arm.c` |
| `vendor/xxhash/kolmo_xxh3.c` | `arch/xxh3.c` (own code; `vendor/xxhash/xxhash.h` stays vendored) |
| `kolmo.c` 143-431, 1109-1113 (transpose) | `transforms/transpose.c`, `transforms/common.h` |
| `kolmo.c` 434-793 (decimal parse/format, AVX2 and NEON formatters) | `algo/decimal.c`, `arch/decimal.c`, `arch/x86_64/decimal_avx2.c`, `arch/aarch64/decimal_neon.c`, `transforms/table_v03.c` |
| `kolmo.c` 795-835, 1584-1603 (scratch cache, checked allocation) | `core/memory.c` |
| `kolmo.c` 836-1063 (v0.3 column table) | `transforms/table_v03.c` |
| `kolmo.c` 1065-1086 (tunables) | `plan/tunables.h` |
| `tbl2.inc`, `log.inc`, `sci.inc`, `word.inc`, `lines.inc` | `transforms/table.c`, `log.c`, `sci.c`, `word.c`, `lines.c` |
| `kolmo.c` 1093-1100 (`kg_hist`) | `algo/hist.c` |
| `cm.inc` | `codecs/cm.c` (codec 7) and `codecs/cm_image.c` (codec 17) |
| `kfam.inc` | `codecs/fastq.c` (codec 16) |
| `kolmo.c` 1115-1154 (shuffle) | `transforms/shuffle.c` |
| `kolmo.c` 1157-1158, 1247-1252 (codec ids, codec context) | `codecs/codec.h` |
| `kolmo.c` 1161-1216 (E8/E9) | `transforms/x86.c` |
| `kolmo.c` 1218-1245, 1433-1518 (codec switches, libbsc) | `codecs/store.c lz4.c zstd.c brotli.c bwt.c lzma.c codec.c` |
| `kolmo.c` 1254-1431 (varint, Huffman, rANS, plane leaves) | `algo/varint.h`, `codecs/huf.c`, `codecs/rans.c`, `codecs/graph_leaf.c` |
| `nplane.inc` | `codecs/nplane.c` (codec 15) |
| `kgraph.inc`, `kg_int_fwd.inc`, `typed_int_encoder.inc` | `transforms/graph.c`, `graph_fwd.c`, `graph_typed.c` |
| `kolmo.c` 1525-1555, 1582, 2765-2818 (per-thread state, pool) | `core/pool.c` |
| `kolmo.c` 1557-1573 (container layout) | `core/container.h` |
| `kolmo.c` 1576-1580, 1605-1617, 1675-2321 (plan, planner) | `plan/plan.h`, `plan/planner.c` |
| `kolmo.c` 1220-1229, 1619-1673 (text and JSON detection, JSON split) | `plan/detect.c`, `transforms/json.c` |
| `kolmo.c` 2323-2342 (positional I/O) | `core/io.c` |
| `kolmo.c` 2344-2406, 2414-2763 (blocks, block encoder) | `core/blocks.c` |
| `kolmo.c` 2820-2836, 2997-3070 (segment planning) | `plan/parallel.c` |
| `kolmo.c` 2838-2858 (tar segments) | `transforms/tar.c` |
| `kolmo.c` 2860-2995 (frontier) | `plan/frontier.c` |
| `kolmo.c` 3075-3213 (encoder) | `core/compress.c` |
| `kolmo.c` 3219-3439 (decoder) | `core/decode.c` |
| `kolmo.c` public functions | `api/api.c` |
| `stream.inc` | `core/stream.c` |
| `cli.c` | `cli/cli.c` |

The tree and its differences from the proposal are described in `docs/LAYOUT.md` ("The tree as built in rc3").

### Why one translation unit

`src/kolmo.c` now only includes the parts in order. I kept the library one translation unit for three reasons:

- **Same code generation, same speed.** The compiler inlines across what used to be one file (the transforms call
  the decimal formatters, the codecs and the scratch allocator in their inner loops). Separate objects would need
  LTO to get that back, and LTO changes code generation per toolchain; one unit gives rc2's machine code, so speed
  is unchanged by construction.
- **No exported internals.** About 450 file-scope functions and tables are `static`. Separate objects would need them all
  global, visible in `libkolmo.a` to every program that links it, with names such as `parse` and `grow`.
- **The tests reach internals** by including `src/kolmo.c` (eleven test programs, the research benches). They work
  unchanged.

The cost is build parallelism: the library core is one 18 s compile (below). Splitting into separately compiled
objects later is mechanical from this tree (one header per directory, `static` → hidden), if the build time matters.

## How byte identity was proven

1. **Determinism check** (`benchmark/determinism/check.py`, unchanged): 110 frames (9 inputs × 12 settings, 1 and 4
   threads, plus `--typed-huf`) match `expected.sha256`. OK.
2. **Old against new build, wider set** (`benchmark/rc3/structure/compare_frames.py`): rc2's binary (built from
   `2207da1`) and this branch's, both from the same Makefile flags, compress 32 inputs (21 generated: zeros, random,
   empty, 1 byte, text, CSV with quotes, TSV with 40 columns, access log, JSON lines, minified JSON, int16/32/64 series,
   float32, decimal float64, an RGB `.npy` image, a WAV file, FASTQ, a tar archive with a 1.3 MB member, a 7.8 MB
   CSV in several blocks; 11 repository files: C sources, Markdown, JSON, JSONL, PNG; and rc2's x86-64 `kolmo`
   executable) at 18 settings (`-m speed rt balanced fast eco hc ratio max ultra`, `-x`, `-x -l 1`, `-x -l 2`,
   `-m max -l 1`, `-q`, `-Q`, `-n`, `--native-leaf`, `-B 1`, `--typed-huf`), each at 1 and 4 threads (`--typed-huf`
   at 1). The frames are compared byte for byte, and each frame is decoded by the other binary.
   - `-march=native` builds: **1040 of 1040 frames identical**, all cross-decoded.
   - baseline builds (`make ARCH=`, the release configuration: AVX2 and SSE4.2 chosen at run time by CPUID):
     **1040 of 1040 frames identical**, all cross-decoded.
   - after the histogram kernel (second commit), `-march=native`: **1040 of 1040 frames identical**, all cross-decoded (the kernel changes speed, not counts).
3. **The test suites** of the plan's rules, below.

## Tests

| test | result |
|---|---|
| `make check` | pass (both commits) |
| `make check-rc` (+ `check-asm` in the second commit) | pass |
| `make check-sanitizers` | pass |
| `make experimental-check GPU_CHECK=` | pass |
| `make kfam-check` | pass |
| `benchmark/determinism/check.py` | 110 frames OK (both commits; and on the `NO_ASM=1` build) |
| also: `check-frontier-dna`, `check-cm-compact`, `check-tar-files`, `make lib` + `bindings/python/test_kolmo.py` + `tests/test_concurrent_dict.py`, `cargo test` (Rust binding from `native/`), the Python library built by `setup.py` from `native/` (kernel linked as a hidden symbol), `make ARCH= NO_ASM=1` + `test-dispatch` + `test-asm` + determinism (no kernel symbol linked) | pass |
| compile matrix (`-fsyntax-only -Wall`, gcc and clang): default, `KOLMO_NO_LZMA`, `KOLMO_FRONTIER` (+ LAG, PATCHED_PCM), `KOLMO_CM_COMPACT`, `KOLMO_TIMING`, `KOLMO_PROF`, `KOLMO_TRACE`, `KOLMO_NO_DISPATCH`, `KOLMO_FORCE_GENERIC_INT_INV`, `KOLMO_TESTING`, `-mavx2 -msse4.2` | no new warning (the three `-Wunused-function` and one `-Wpsabi` of rc2 remain) |

## Build

Same machine, idle, sequential runs (`TIMEFORMAT`, wall and user CPU seconds):

| | rc2 (`2207da1`) | rc3 structure |
|---|---|---|
| clean `make -j4 all` | 35.4 s wall, 83.9 s CPU | 35.6 s wall, 83.6 s CPU |
| library unit only (`touch src/kolmo.c`, `make build/kolmo`) | 18.5 s | 17.9 s |
| one part changed (`kgraph.inc` / `codecs/zstd.c`), `make -j4 all` | 18.8 s | 19.5 s |
| `kolmo.o` text / data | 760 372 / 224 bytes | 760 793 / 1 264 bytes (codec tables) |

The build time does not change: the library is still one 18 s compile, which is the critical path of `make -j4`.
Separately compiled parts would let `-j4` spread it, at the cost discussed above.

End-to-end speed, best of 25 alternating runs, 1 thread (`-T 1`), inputs of `compare_frames.py`:

| input, mode | rc2 | restructure only | + histogram kernel |
|---|---|---|---|
| big.csv (6.1 MB), `-m fast` | 42.6 ms | 41.7 ms | 42.2 ms |
| series.i32 (1.2 MB), `-m fast` | 12.1 ms | 12.2 ms | 12.2 ms |
| series.i32, `-m ratio` | 55.0 ms | 55.9 ms | 55.2 ms |
| series.i64 (0.8 MB), `-m ratio` | 137.1 ms | 134.6 ms | 136.0 ms |
| dec.f64 (0.6 MB), `-m ratio` | 227.5 ms | 243.9 ms | 222.3 ms |
| floats.f32, `-m fast` | 7.7 ms | 7.8 ms | 7.6 ms |
| mono.wav, `-m fast` | 10.5 ms | 10.5 ms | 10.1 ms |
| text.txt (1.6 MB), `-m ratio` | 772.0 ms | 758.6 ms | 758.8 ms |
| records.jsonl, `-m fast` | 9.0 ms | 9.1 ms | 9.2 ms |

No systematic change. Single cases move by up to ±7% between builds whose hot code is the same (dec.f64: the
restructure-only build is 7% slower, the next build 2% faster than rc2): on this VM, code placement and inlining
choices alone move end-to-end times by that much. **For the other lanes:** judge a kernel by a micro-benchmark
against its reference and by several end-to-end inputs, not by one end-to-end run.

## Assembly pattern: `src/arch/asm.h` and the histogram kernel

`src/arch/asm.h` gives GNU assembler that builds unchanged with gcc and clang on Linux and Apple clang on macOS:
`kolmo_function NAME` / `kolmo_endfunc NAME` (alignment, global and hidden symbol, `_` prefix on Mach-O,
`.type`/`.size` on ELF, `endbr64` or `bti c` landing pad when `-fcf-protection` or `-mbranch-protection` is on) and
`kolmo_asm_end` (`.note.GNU-stack`, and the GNU property note for IBT/SHSTK or BTI/PAC so that the linker keeps the
protection for the whole binary). Statements are in `.macro` bodies because `;` starts a comment in Apple's arm64
assembler.

The example kernel is the byte histogram (`kg_hist`, used by the graph leaves, the rANS coder and the word
transform):

- reference `kolmo_hist_c` and dispatch `kg_hist` in `src/algo/hist.c`;
- `src/arch/x86_64/hist_x86_64.S`: eight counter tables (seven on the stack), two 4-byte loads per 8 bytes split by
  shifts in two independent chains, SSE2 sum (baseline x86-64, no CPUID test needed);
- `src/arch/aarch64/hist_aarch64.S`: the same with `ubfx` and load-add-store, NEON sum;
- `tests/test_asm.c` (`make check-asm`, in `check-rc`): every length 0-300 at 16 alignments and large inputs, four
  byte distributions, guard word after `hc[255]`, against `kolmo_hist_c`; optimised and with ASan/UBSan;
- `benchmark/rc3/structure/hist_bench.c`: the kernel against the reference.

Kernel against reference (best of 7 × 256 MiB, idle; `-march=native` build; the baseline build is within 3%):

| bytes per call | random | skewed (90% 4 symbols) | text-like | runs of one value |
|---|---|---|---|---|
| 256 | -33% | -32% | -33% | -14% |
| 1 KiB | -12% | -12% | -12% | +22% |
| 4 KiB | -2% | -2% | -2% | +44% |
| 16 KiB | -1% | +2% | +1% | +52% |
| 64 KiB | 0 to +11% | +3 to +6% | +2 to +11% | +52% |
| 1 MiB | +5% | +9% | +6% | +55% |

(64 KiB: range over three runs.) The kernel clears and sums 8 KiB of tables where the reference clears 4 KiB, so it
loses on short inputs; `kg_hist` uses it from 16 KiB (`KOLMO_HIST_KERNEL_MIN`). The gain is in the kernel; end to end
it is below the noise of the table above (the histogram is a small part of every mode).

What went wrong first, for whoever copies the pattern: (1) an 8-byte load split with `al`/`ah` was 15% *slower* on
skewed data than the C reference (high-byte register reads); two 4-byte chains fixed it. (2) Dispatching inside an
inlinable `kg_hist` made `-m fast` 10% slower on CSV, where the histogram is never called: the larger body changed
GCC's inlining in its callers. `kg_hist` is now `noinline`, and the end-to-end times match rc2. (3) AArch64 `add`/`sub`
immediates are 12 bits: a 7 KiB stack frame takes two instructions.

The AArch64 kernel was checked here with `aarch64-linux-gnu-gcc` and `qemu-aarch64` (same counts as the reference on
every length 0-300, 16 alignments, three distributions and 1 MiB; also with `-mbranch-protection=standard`:
`bti c` at entry and the BTI/PAC property note), and both kernels assemble for Mach-O with
`clang -target arm64-apple-macos11` / `x86_64-apple-macos11` (symbols `_kolmo_hist_*`, private extern). There is no
Arm hardware here to time it, so `kg_hist` does not choose it yet on AArch64: on the M5,
`make build/libkolmoarch.a build/libvendor.a` and the command in `hist_bench.c`; if it gains from some size, define
`KOLMO_HIST_AARCH64` by default in `algo/hist.c` and set the threshold.

## Changes to the build

- `Makefile`: the library parts are listed by directory (`LIBDIRS`, `$(wildcard src/<dir>/*.c src/<dir>/*.h)`) as
  prerequisites of `build/kolmo.o`; `src/arch/xxh3.c` and `src/arch/<isa>/*.S` are compiled on their own into
  `build/libkolmoarch.a`, which `libkolmo.a` also contains and which every rule that compiles `src/kolmo.c`
  directly (tests, sanitizer builds, profile and frontier builds) links. `NO_ASM=1` drops the `.S` files and defines
  `KOLMO_NO_ASM`; `NO_GPU=1` drops `src/arch/metal/*.m` (none yet) and defines `KOLMO_NO_GPU`; with Metal sources on
  macOS the frameworks are linked and `KOLMO_HAVE_METAL` is defined. New targets: `build/test-asm`,
  `build/test-asm-asan`, `check-asm` (in `check-rc`).
- `experimental.mk`, `src/experimental/kfam.c`, `tests/test_cm_compact.c`: new paths of `fastq.c` and `cm.c`.
- `bindings/python/setup.py`, `bindings/rust/build.rs`: compile `src/kolmo.c`, `src/arch/xxh3.c` and the target's
  `src/arch/<isa>/*.S` (`KOLMO_NO_ASM=1` for none); `-DKOLMO_NO_GPU`; Rust reruns on any change under `src/`.
  `sync-native.sh` keeps only the frontier sources of `src/experimental` (as the committed copies had);
  `native/` re-synced.
- `fuzz/build.sh`, `.github/workflows/ci.yml`: link `build/libkolmoarch.a`; CI gains a `NO_ASM=1` step (kernel
  tests and determinism on the C references); the streaming test reads `src/**/*.[ch]`.
  `planner-ownership-validation.yml` watches `src/**`.
- `tune/ga.py`: new paths and `libkolmoarch.a`.

Not changed: `tests/validate_planner_fix.py` and `research/stack-audit/build.sh` check out or patch pinned rc1/rc2
commits by their old paths, so they keep them.

## For the other lanes

Merge this branch (`git fetch origin claude/rc3-structure-20261116 && git merge FETCH_HEAD`). A hook in rc2's
`kolmo.c` now lives in the file of the table above; `git log --follow` works for the moved `.inc` files. A new part
needs one `#include` line in `src/kolmo.c` after the parts it uses. A new codec: a file in `codecs/` with
`static const kolmo_codec codec_<name>`, a tentative declaration in `codecs/codec.h` and one registry line in
`codecs/codec.c` (reserved ids: `research/RC3-PLAN.md`). A new kernel: copy the histogram pattern.
