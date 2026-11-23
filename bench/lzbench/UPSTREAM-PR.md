# Draft: pull request to inikep/lzbench

**Status: draft, not sent.** The owner decided the lzbench pull request waits for Kolmo's big release. Nothing here
has been posted anywhere. Before sending, work through "Before sending" at the end.

---

## Title

Add kolmo 0.0.2

## Body

This adds [Kolmo](https://github.com/adnaneben/kolmo), a structure-aware lossless compressor written in C
(Apache License 2.0). Kolmo analyses each block (records, columns, integer and floating-point series, images, text)
and picks a transform and a back end per block: an LZ77 + Huffman/ANS coder at levels 1-7, block sorting (BWT) or
an LZMA-class coder at levels 8-9, and context mixing at level 10 ("ultra").

**What changes**

- `misc/kolmo/src/`: Kolmo's `src/` directory at tag `v0.0.2` (commit `<commit>`), unchanged, with `LICENSE`,
  `NOTICE` and a short `README.md` saying so. `src/cli` and `src/experimental` are part of the copy but not built.
- `mk/kolmo.mk`: the library objects (one translation unit, `kolmo.c`, plus the kernels compiled on their own and,
  on x86-64 and AArch64, one assembly file each). Kolmo is off on 32-bit, big-endian and Windows targets.
- `bench/misc_codecs.cpp`: `lzbench_kolmo_compress` / `lzbench_kolmo_decompress`, about 20 lines, calling the public
  API (`kolmo_params_for_level`, `kolmo_compress`, `kolmo_decompress`).
- `bench/codecs.h`, `bench/lzbench.h`: declarations, the `comp_desc` entry (levels 1-10, `NO_THREADING`), the
  per-level algorithms, and the aliases `LZ+ENTROPY` (`kolmo,1,3,5,7`), `SYMMETRIC` (`kolmo,8,9`), `SLOW`
  (`kolmo,10`) and `FASTEST` (`kolmo,1`).
- `README.md` codec table, `CHANGELOG`.

**Levels.** lzbench level *n* is `kolmo -n` (1-9; 3 is Kolmo's default) and 10 is `kolmo -U`. The wrapper uses the
same level table as Kolmo's command line, so the compressed sizes are byte for byte those of `kolmo -T 1 -<n>`.
Level 10 needs about 1 GB of memory or more, hence `SLOW` only.

**Compiler flags.** Kolmo's objects get `-fno-fast-math -ffp-contract=off` after lzbench's flags. Its encoders
choose some routes by floating-point cost and its frame format is specified to be identical on every machine and
compiler; `-ffast-math` or FMA contraction could change those choices. Everything else is lzbench's `-O3`.
The SIMD kernels pick SSE2/SSE4.1/AVX2 or NEON at run time, so no `-m` flags are needed.

**No conflicts with other codecs.** Kolmo vendors no codec library. Its suffix-sorting code is a subset of libsais
whose symbols are renamed `kolmo_sais_*`, so it links next to bsc's libsais.

**Why it is notable** (CONTRIBUTING: "significant in at least one benchmark dimension"). Silesia, one thread, lzbench
2.4.1 (MB/s):

| | ratio | compress | decompress | Apple M5 | x86-64 (`<cpu>`) |
|---|---:|---:|---:|---|---|
| kolmo 0.0.2 -6 | 4.367 | | | `<m5 c / d>` | `<x86 c / d>` |
| xz 5.8.4 -9 | 4.346 | | | | |
| kolmo 0.0.2 -9 | 5.232 | | | | |
| zpaq 7.15 -5 | 5.237 | | | | |
| kolmo 0.0.2 -10 | 5.912 | | | | |

`<fill in from evidence/X86_CHECK_0.0.2.md: level 6 is smaller than xz -9 and decodes about 8x faster; level 9
matches zpaq -5's ratio and decodes about 100x faster; level 10 has the best Silesia ratio of the codecs in
lzbench>`

**Tested**

- Built and run on Linux x86-64 (GCC, GitHub runner) and macOS arm64 (Apple clang), `make` with all codecs and
  with `BUILD_ONLY`; every round trip verified by lzbench.
- `<link to this PR's Azure pipeline run>`

---

## Before sending (owner checklist)

1. **Owner's go.** The lzbench PR waits for the big release (decision recorded in the market report's plan).
2. **Tag.** CONTRIBUTING wants sources from a tagged release: run `install.py --ref <release tag>` on a fresh fork
   of lzbench master, and update the version in the title, the `comp_desc` entry and the README row (the script does
   the last two).
3. **Global symbols.** 31 internal Kolmo symbols are global without the `kolmo_` prefix (`kx_*`, `tbl_*`,
   `tbl2_*`, `gr6_*`, `np_*`, `log_*`, `sci_*`, `word_*`, `parse_dec_pub`, `fset_discover_pub`). None collides with
   lzbench 2.4.1, but lzbench links 70-odd codecs into one binary, so make them `static` or prefixed in Kolmo before
   the release that is submitted.
4. **lzbench's CI matrix.** Their Azure pipeline builds with GCC 10-15 and Clang 12-22 on Linux x86-64/x86-32/ARM64/
   ARM32/PPC64LE, macOS arm64 and Windows MinGW. Kolmo is off on 32-bit and Windows by `mk/kolmo.mk`. PPC64LE (and
   RISC-V where they build it) would use Kolmo's C-only path (`KOLMO_NO_ASM`), which Kolmo's own CI tests only on
   x86-64 and AArch64: run the PR branch on their pipeline first (a draft PR, or Azure on a fork), and if those
   targets fail, add them to the `DONT_BUILD_KOLMO` conditions rather than patching Kolmo's sources.
5. **Compiler warnings.** Apple clang 21 reports two harmless warnings in Kolmo (`unknown attribute 'optimize'` in
   `codecs/audio.c`, a constant `&&` in `transforms/graph.c`). Fixing them in Kolmo before the release avoids noise in
   lzbench's logs.
6. **Numbers.** Fill the table above from the latest x86-64 run (`bench-x86` workflow) and the Mac run, and link the
   reproduction (`bench/lzbench/README.md`).
