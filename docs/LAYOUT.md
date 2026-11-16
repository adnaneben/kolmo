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
  core/                      the container and everything every mode shares
              container.c    frame header, block index, version, checksums (CRC32C, XXH3 wrappers)
              blocks.c       block splitting, block scheduler, thread pool
              stream.c       streaming encoder/decoder
              memory.c       memory model: budgets, limits, accounting (docs/MEMORY-MODEL.md)
  plan/       planner.c      analysis, transform choice, codec choice per block and per leaf, --target cost model
              detect.c       content detection (text, table, FASTQ, WAV, Y4M, NPY, executables, ...)
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
  experimental/              research tools that are not part of the library
```

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
