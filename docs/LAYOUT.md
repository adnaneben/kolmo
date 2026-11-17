# Source layout (rc3 and later)

Status: adopted for rc3 (owner's request of 16 November 2026). Backward compatibility of the file format is not a
constraint for rc3: a change that makes Kolmo faster or smaller may break reading of older files.

## Principles

1. **One concern per directory, one codec per file.** A reader finds a codec, a transform or a CPU kernel by its
   name.
2. **Layers depend downwards only:** `cli` → `api` → `core` → `plan` → `transforms` → `codecs` → `algo` → `arch`.
   Nothing in `algo` or `arch` knows the container format.
3. **Every assembly or intrinsic kernel has a portable C reference.** The reference is the specification. A test
   compares each kernel with its reference on random and edge-case inputs. `make NO_ASM=1` builds the C references
   only, so CI checks both paths.
4. **Runtime dispatch, never compile-time only.** Release builds target the baseline ISA (x86-64, ARMv8.0-A). Faster
   kernels are chosen once at startup from CPU features, as `fc9a356` does for x86.
5. **Same bytes everywhere.** A kernel may change speed, never output. The determinism check covers every ISA path.
6. **Third-party code we keep stays in `vendor/` untouched.** Code we take over (port, subset or rewrite) moves to
   `src/` under our name. The original copyright and licence are kept in the file header and in `NOTICE`.

## Tree

```
src/
  kolmo.h                    public API (the only installed header)
  api/        api.c          public entry points, parameter checking, limits
              features.c     public entry points of trained profiles and --target, the compact profile frame,
                             target chunk merging, kolmo_calibrate
  core/                      the container and everything every mode shares
              container.c    frame header, block index, version, checksums (CRC32C, XXH3 wrappers)
              blocks.c       block splitting, block scheduler, thread pool
              stream.c       streaming encoder/decoder
              memory.c       memory model: budgets, limits, accounting (docs/MEMORY-MODEL.md)
  plan/       planner.c      analysis, transform choice, codec choice per block and per leaf, --target cost model
              detect.c       content detection (text, table, FASTQ, WAV, Y4M, NPY, executables, ...)
              profile.c      trained profiles (KPF1): format, pinned plans, dictionary codecs of profile frames, training
              target.c       --target: parser, cost model, speed table (target_table.inc), per-chunk choice
  transforms/                structure transforms, one per file (format-aware, codec-agnostic)
              table.c log.c lines.c sci.c word.c json.c x86.c tar.c nplane.c
              graph.c        integer/decimal graph (was kgraph.inc), graph_fwd.c (was kg_int_fwd.inc)
  codecs/                    one block codec per file, each exporting a `kolmo_codec` table (codec.h)
              codec.h        struct kolmo_codec { id, name, bound(), compress(), decompress(), mem_needed() }
              store.c lz4.c zstd.c brotli.c lzma.c bwt.c cm.c cm_image.c huf.c rans.c fastq.c audio.c video.c graph_leaf.c
  algo/                      reusable algorithms with no knowledge of the format; own code or code we took over
              sais/          suffix array, BWT and inverse BWT (taken over from libsais; see algo/sais/README.md)
              lzp.c          LZP preprocessor (from libbsc, ported to C)
              qlfc.c         QLFC coder (from libbsc, ported to C)
              rangecoder.h   binary range coder shared by CM, QLFC, kfam
              huffman.c rans.c lpc.c ...
  arch/                      CPU-specific code, by ISA (the dav1d and FFmpeg convention)
              cpu.c cpu.h    feature detection and dispatch tables (x86 CPUID, Arm HWCAP/sysctl)
              asm.h          macros for portable GNU assembler: symbol names (leading underscore on macOS),
                             function begin/end, .note.GNU-stack, CET (endbr64) and BTI landing pads
              x86_64/        *_sse2.c *_avx2.c (intrinsics, __attribute__((target))), *_avx2.S (assembly)
              aarch64/       *_neon.c (intrinsics), *_neon.S (assembly)
              metal/         GPU kernels (*.metal) and their host code (*.m)
  cli/        cli.c
              train.c        kolmo train, kolmo calibrate, kolmo profile
  experimental/              research tools that are not part of the library
```

### The tree as built in rc3 (structure lane)

The proposal above is followed with these differences, which the structure lane chose to keep the output byte-identical
and the move mechanical:

- **One translation unit.** `src/kolmo.c` is now only a list of `#include`s of the parts below, in dependency order.
  The compiler still sees the whole library at once (the same inlining as rc2), internal functions stay `static`, and
  the tests that include `src/kolmo.c` to reach them work unchanged. Each part can only use what an earlier part
  defines. Compiled on their own and archived with it: `arch/xxh3.c` (it defines its own xxHash instance) and the
  assembly kernels `arch/<isa>/*.S`. The Makefile lists the parts by directory as prerequisites.
- **Per-file blocks instead of a few big files.** `core/` has `blocks.c` (block splitting and the block encoder),
  `pool.c` (thread pool, per-thread state), `compress.c`, `decode.c`, `io.c`, `memory.c` (scratch cache, checked
  allocation), `stream.c`, `container.h`, `common.h` (system and codec headers). There is no `core/container.c` yet:
  the header layout is in `container.h`, parsing in `decode.c`, CRC32C in `arch/crc32c.c`.
- **plan/**: `planner.c` (mode, trials, `make_plan`), `detect.c` (text and JSON detection), `parallel.c` (planning
  of archive members on the pool), `frontier.c` (opt-in research build), `plan.h`, `tunables.h`.
- **transforms/**: `table.c` (was `tbl2.inc`), `table_v03.c` (the column table of v0.3, tflag 3), `transpose.c`,
  `shuffle.c`, `log.c`, `lines.c`, `sci.c`, `word.c`, `json.c`, `x86.c`, `tar.c` (archive segments), `graph.c`,
  `graph_fwd.c`, `graph_typed.c` (was `typed_int_encoder.inc`), `common.h` (transform ids, scanners).
- **codecs/**: one file per codec family with its `kolmo_codec` table: `store.c lz4.c (LZ4, LZ4-HC) zstd.c brotli.c
  bwt.c (block sorting: codec 18, and codec 5 read for rc2 frames; streams in bwt_stream.c, compiled on its own) lzma.c cm.c cm_image.c (codec 17) huf.c rans.c nplane.c (codec 15, a graph leaf codec, so
  in codecs/ rather than transforms/) fastq.c (was kfam.inc, codec 16)`, `graph_leaf.c` (plane-aligned leaves), and
  `codec.c`, the registry and `codec_bound`/`codec_compress`/`codec_decompress`. `audio.c` and `video.c` come with
  the format2 lane.
- **algo/**: `decimal.c` (decimal parse and format: the references of the decimal kernels), `hist.c` (byte
  histogram), `varint.h`; `lzp.c`, `qlfc.c` (+ `qlfc_model.h`, `qlfc_tables.h`, `rangecoder.h`), `par.c` (fork-join and the thread crew) and `sais/` (the libsais subset): ported from libbsc and libsais by the bwt lane, compiled on their own.
- **arch/**: `cpu.h` (CPUID dispatch, target attributes, host byte order), `crc32c.c` (reference and dispatch),
  `decimal.c` (dispatch of the formatters), `xxh3.c` (was `vendor/xxhash/kolmo_xxh3.c`), `asm.h`;
  `x86_64/`: `crc32c_sse42.c decimal_avx2.c int_inv_sse2.c int_inv_avx2.c hist_x86_64.S`;
  `aarch64/`: `crc32c_arm.c decimal_neon.c hist_aarch64.S`. `arch/metal/` does not exist yet (`NO_GPU=1` is ready
  for it). The SSE2/NEON scanners inside the transforms (transpose, table, word) were left where they are; they are
  candidates for the simd lanes.
- **cli/**: `cli.c`.

Adding a part: create the file in its directory, add one `#include` line to `src/kolmo.c` after the parts it uses,
and nothing else (the Makefile and the bindings find it). Adding a codec: a file in `codecs/` defining
`static const kolmo_codec codec_<name>`, its tentative declaration in `codecs/codec.h` and its identifier in the
registry of `codecs/codec.c`. Adding an assembly kernel: copy `arch/x86_64/hist_x86_64.S` (or the aarch64 one), its
reference and dispatch in `algo/hist.c`, and its test in `tests/test_asm.c`.
Adding an intrinsics kernel chosen through the table (the simd-x86 lane's pattern): the reference `kolmo_<name>_c` in
`algo/<topic>.c` with its prototype in `algo/<topic>.h`; the ISA versions `kolmo_<name>_<isa>` in
`arch/x86_64/<name>_<isa>.c` (or a `*_tmpl.h` included once per ISA), compiled on their own with
`__attribute__((target))`; a pointer in `arch/kernels.h`, set by level in `arch/cpu.c`; the file in the Makefile's
`KSRC` (and the two bindings' lists); a comparison at every level in `tests/test_kernels.c`. Callers use
`KOLMO_K-><name>(...)`. `KOLMO_CPU=c|sse2|sse41|avx2` lowers the level at run time for tests and benchmarks.

### Why per ISA, not a single `asm/` folder

Assembly is specific to an instruction set, and so are intrinsics. Keeping both in `arch/<isa>/` puts every kernel
for one CPU family together with its dispatch entry. dav1d (`src/x86`, `src/arm/64`), FFmpeg (`libavcodec/x86`,
`libavcodec/aarch64`) and zstd (`.S` next to the C it replaces) all follow this convention. A top-level `asm/`
would split the intrinsics fallback from the assembly that replaces it.

### Assembly conventions

- GNU assembler syntax in `.S` files, run through the C preprocessor by the C compiler. No NASM dependency, and the
  same files build with clang on macOS and gcc/clang on Linux.
- Naming: `<kernel>_<isa>` for the symbol and `<kernel>_<isa>.S` for the file, e.g. `kolmo_unbwt_avx2`, matching its
  C reference `kolmo_unbwt_c`.
- Calling convention: System V AMD64 / AAPCS64. Windows is not a target; if it becomes one, `asm.h` gains a Win64
  prologue.
- Every `.S` ends with the non-executable-stack note and starts with `#include "../asm.h"`.
- A kernel lands only with a measured gain (any size, even a few percent, as long as it is above the noise of the
  benchmark), a C reference and a test.

## Build

The `Makefile` keeps one library (`build/libkolmo.a`, `libkolmo.dylib`/`.so`). Sources are listed by directory
(`$(wildcard src/codecs/*.c)` and so on). `NO_ASM=1` drops `arch/*/*.S`. `NO_GPU=1` drops `arch/metal`. The Python and
Rust bindings copy `src/` as a whole (`bindings/*/native/src`), so the layout is the same everywhere.

## Codec identifiers

`src/codecs/codec.h` holds the registry. rc3 is container format 2 (magic and version decided by the integration
lane). Identifiers reserved for the rc3 lanes are listed in `research/RC3-PLAN.md`.
