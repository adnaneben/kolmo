# Kolmo's dependencies: review and decisions (rc3)

Written by the deps lane on 16 November 2026 (`claude/rc3-deps-20261116`). The owner asked four questions of every
external dependency:

1. Can it be removed or reduced?
2. Should it be rewritten in C, and where in assembly, for portability?
3. How much speed can owning or rewriting it gain?
4. Which parts should be integrated into Kolmo as specialised code?

Every answer below rests on a measurement. Commands, raw tables and what failed are in
`benchmark/rc3/deps/RESULTS.md`. libbsc and libsais belong to the bwt lane. They appear here only so the picture is
complete.

## How the time was measured

- **Machine:** a cloud VM with an Intel Xeon at 2.10 GHz (4 vCPUs), Linux 6.18, GCC 13.3. Every run used one thread
  (`-T 1`).
- **Profiler:** `build/kolmo-libprof` is the `kolmo` CLI linked with `-Wl,--wrap` around every library entry point Kolmo
  calls (`benchmark/rc3/deps/libprof.c`). Each call's wall time and byte count is summed. Calls a library makes to
  itself are not wrapped, so nothing is counted twice.
- **Data:**
  - Silesia: 12 files, 211.9 MB.
  - Structured set: 12 files, 28.7 MB. Two OWID CSV tables (energy, CO2), an OWID JSON file, and the 9 inputs of the
    determinism check (text, CSV, log, int16/int64/float64 arrays, image, NPY, WAV).
- **Phases:** each figure is the sum over the files of one compress process and one decompress process, process start
  and file I/O included.

### Share of wall time spent inside each dependency, per mode (1 thread)

| mode | phase | Silesia | structured set |
|---|---|---|---|
| `speed` | compress | LZ4 70.4% | LZ4 55.3% |
| `speed` | decompress | LZ4 30.2% | LZ4 19.3% |
| `balanced` | compress | zstd 69.0%, HUF 4.7%, XXH3 1.0% | zstd 12.3%, HUF 1.3% |
| `balanced` | decompress | zstd 41.6%, HUF 6.5%, XXH3 1.8% | zstd 11.0%, HUF 2.2%, XXH3 1.4% |
| `eco` | compress | Brotli 93.2%, zstd 1.3% | Brotli 45.0%, zstd 3.0% |
| `eco` | decompress | Brotli 62.2% | Brotli 34.8% |
| `hc` | compress | zstd (levels 19/22) 98.4% | zstd 78.2% |
| `hc` | decompress | zstd 38.8%, HUF 3.0% | zstd 14.1%, HUF 2.4% |
| `ratio` | compress | LZMA2 87.5%, libbsc 10.1% | LZMA2 91.6%, libbsc 1.6% |
| `ratio` | decompress | libbsc 59.9%, LZMA2 33.5% (system 5.4.5) → 24.0% (vendored 5.8.1) | LZMA2 45.6%, libbsc 6.6% |

The remainder is Kolmo's own code (transforms, planner, CM, rANS, the graph coder), process start (about 1.2 ms per
process) and file I/O.

How to read the table:

- **Compression** is dominated by the dependencies in every mode except `balanced` on structured data. In each of those
  modes, the time goes into a deliberately expensive setting: LZMA2 preset 9e, zstd 19/22, Brotli quality 8. It is not
  lost to inefficiency in the libraries.
- **Decompression** spends 2% to 62% of its time in the dependencies. The largest single item was the system liblzma's
  LZMA2 decoder. Vendoring a newer liblzma removed a third of that time (below).

## Summary

| dependency | version | what Kolmo calls | removed or reduced? | rewrite in C / assembly? | speed from owning it | specialised code in Kolmo |
|---|---|---|---|---|---|---|
| liblzma (XZ Utils) | system 5.4.5 → vendored 5.8.1 subset | raw LZMA2 buffer encode/decode, `lzma_lzma_preset` | **done:** the system library is gone; 70 of the release's files (LZMA2 coder only) are vendored, plus 4 Kolmo files | already C99; no | **1.43× faster LZMA2 decode** from the newer release, measured; an own coder would gain at most a share of the remaining 24% of `ratio` decode | none: decoding straight into Kolmo's buffer saves ≤ 3% of LZMA2 decode |
| zstd | 1.5.7 | one-shot `ZSTD_compressCCtx`/`decompressDCtx` (levels 1, 3, 9, 19, 22), dictionaries, `ZDICT_trainFromBuffer`, HUF directly | **done:** 63 unused files removed; 152 KiB of unused code dropped at link | C99 with x86-64 Huffman assembly; the assembly is worth ~2% of `balanced` decode end to end; no ARM port needed for rc3 | none measurable: per-call overhead is negligible (105 frames for 212 MB) | **done:** Kolmo's Huffman leaves now pass zstd's BMI2 flag, 1.15–1.21× faster leaf decode on baseline x86-64 builds; frameless zstd leaves would save < 0.03% |
| LZ4 | 1.10.0 | `LZ4_compress_fast`, `LZ4_compress_HC_extStateHC`, `LZ4_decompress_safe` | **done:** 105 KiB (49%) of its code dropped at link; already 2 files | C99; no | none: decode is at 2.4 GB/s per thread, copy-bound | none |
| Brotli | 1.1.0 | `BrotliEncoderCompress` (quality 8, window 24), `BrotliDecoderDecompress` | **done:** unused dictionary blobs removed; 13 KiB dropped at link | C99; no | none from ownership: 93% of `eco` compression is the quality-8 search itself | none |
| xxHash | 0.8.2 | XXH3-64 (block checksums), XXH64 (bsc payload check, versions 1–2) | 27 KiB (75%) of its code dropped at link; keep | C99 with AVX2 dispatch (rc2); no | none: checksums take 0.4–2% | already specialised (`kolmo_xxh3.c`) |
| libbsc | 3.4.0 (C++) | `bsc_compress`/`bsc_decompress` (LZP, BWT, static QLFC) | bwt lane: ported to C (codec 18) | bwt lane | 60% of Silesia `ratio` decode | bwt lane |
| libsais | (in libbsc) | `libsais_bwt`/`unbwt` | bwt lane: owned under `src/algo/sais/` | bwt lane | — | bwt lane |

## liblzma

**What Kolmo uses.** Codec 6 (`ratio` on binary data, and trials in `max` and `-x`) codes a block as raw LZMA2:
- encoding with `lzma_raw_buffer_encode` at preset 9e, with the dictionary set to the next power of two at or above
  the block size (4 KiB to 64 MiB), and lc/lp/pb chosen by Kolmo;
- decoding with `lzma_raw_buffer_decode`.

Kolmo never uses the .xz container, integrity checks, BCJ or delta filters, or threads.

**1. Removed or reduced: yes, done.** Until rc3, every build linked the system liblzma (`liblzma-dev`, Homebrew `xz`).
Builds without it silently lost codec 6, and the version varied by machine (5.4.5 on this VM).

rc3 vendors a pinned subset of **XZ Utils 5.8.1**:
- The tarball's signature was verified against Lasse Collin's key `3690C240CE51B4670D30AD1C38EE757D69184620`.
- 5.8.1 is not 5.6.0 or 5.6.1.
- C sources and headers only. No build scripts, test files or assembly.
- 70 files are byte-identical to the release. Four small Kolmo files are added: the config header, the CRC32 table the
  match finders hash with, a symbol namespace, and an include wrapper.

Every external symbol is renamed to `kolmo_lzma_*`, so `libkolmo.so` can share a process with the system liblzma
(Python's `lzma` module loads it). The Makefile, CI, the fuzz build, `tune/ga.py` and both bindings now compile the
subset. No build needs a system library, and cross-compiled wheels keep codec 6 (rc2 dropped it there).

Output is byte-identical to 5.4.5: the determinism check passes, and raw LZMA2 of xml, dickens and x-ray matches.
Details are in `vendor/xz/README.kolmo`.

**2. Rewrite in C or assembly: no.** liblzma is portable C99.

The only assembly that would matter is a hand-written LZMA decoder loop. 7-Zip's LZMA SDK has one for x86-64 and
ARM64; it is in the public domain, a licence not on the plan's list, so it would need the owner's decision. Its gain is
bounded by the next answer.

**3. Speed from owning it.** The measured gain came from the release, not from owning the code:

| LZMA2 coder, preset 9e, 6 Silesia files (47.5 MB), best of 2 | compress | decompress |
|---|---|---|
| system liblzma 5.4.5 (Ubuntu build) | 1.47–1.49 MB/s | 44.7–45.5 MB/s |
| vendored 5.8.1, `-O3 -march=native` | 1.40–1.44 MB/s | 63.8–64.9 MB/s (**1.43×**) |
| vendored 5.8.1, `-O2` baseline (preset 6, 4 files) | 1.39 MB/s (system: 1.35) | 60.0 MB/s (system: 42.5) |

Inside Kolmo `ratio` on Silesia, LZMA2 decoding fell from 1,710 ms to 1,120 ms. The whole decompression went from
5.10 s to 4.67 s (−8%). On the structured set, LZMA2 is 46% of `ratio` decompression time.

An own LZMA-class coder (codec 24) would have to beat liblzma on its own terms:

- **Encoding.** It would have to beat the 9e parse at equal size. The parse settings show how steep that curve is on
  the structured set:
  - automatic search depth: +0.065% size for 1.11× speed;
  - depth 64: +0.22% for 1.41×;
  - nice length 128: +0.48% for 1.52×;
  - non-extreme: +1.19% for 2.3×.

  No setting gives the same size for less time, so the parse is not where liblzma wastes time.
- **Decoding.** It would be capped by the 24% (Silesia) to 46% (structured) of `ratio` decompression left in LZMA2. A
  decoder 1.3× faster than 5.8.1 (the published asm-versus-C margin of 7-Zip's decoder; not measured here) would gain
  about 6% on Silesia `ratio` decode.

**Decision: no codec 24 in rc3.**

The larger lever is elsewhere. On Silesia, `ratio` runs both LZMA2 and BWT on some blocks and keeps the smaller:
- 24 LZMA2 encodes against 17 LZMA2 blocks written;
- about 29 MB of LZMA2 encoding thrown away, ≈ 21% of `ratio` compression time.

Skipping the LZMA2 trial when BWT clearly wins is item 3 of the bwt lane.

**4. Specialised code: none.** liblzma decodes into its own dictionary buffer and copies to the output. Writing straight
into Kolmo's output block would save at most the mapping, fill and copy of one block: 22 ms per 47.5 MB, ≈ 3% of LZMA2
decoding (`lzma_dictcost`). That is not worth a fork of vendored code.

## zstd (with HUF and FSE)

**What Kolmo uses.**
- One-shot `ZSTD_compressCCtx` and `ZSTD_decompressDCtx` with a reused context: levels 1 and 3 for planner trials;
  3, 9, 19 and 22 for blocks and leaves.
- `ZSTD_compress_usingDict`, `ZSTD_decompress_usingDict` and `ZDICT_trainFromBuffer` for `-D`/`-t`.
- HUF (`HUF_compress4X_repeat`, `HUF_decompress4X_hufOnly_wksp`) directly, for codec 13 (Huffman-only leaves).
- `kolmobench` also uses `ZSTD_compress2` with workers, to measure the zstd rival.

**1. Removed or reduced: reduced, done.** Removed from `vendor/zstd`:
- files that were never compiled: `legacy/`, `deprecated/`, `dll/`, a stale `obj/` tree of dependency files, and
  zstd's own build files;
- 63 files in all; the directory went from 3.6 MB to 2.3 MB.

Vendored objects are now compiled with `-ffunction-sections -fdata-sections`, and the linker drops what Kolmo never
reaches: 152 KiB of zstd (19% of its code), 371 KiB over all dependencies. The `kolmo` binary's code went from 3.13 MB
to 2.78 MB. Output is identical, and the speed difference is within noise (`ab_gcsections.txt`).

zstd cannot be removed. It is the `balanced`/`hc` workhorse and the planner's cost probe (level-1 trials).

**2. Rewrite in C or assembly: no.** zstd is C99. Its one assembly file (x86-64 Huffman 4-stream decode) gains about
2% of `balanced` decompression end to end on a baseline build: 0.480 s with `ZSTD_DISABLE_ASM`, 0.469 s with the
assembly. A NEON/AArch64 port for ARM is therefore low priority; the simd-arm lane can measure it on the M5.

**3. Speed from owning it: none measurable.**
- Kolmo calls zstd with large inputs: 105 frames for 208 MB in `balanced` decode on Silesia, 26 frames on the
  structured set, so per-call overhead is negligible.
- The level-19/22 compression in `hc` is the search itself.

**4. Specialised code.** Two candidates were measured:

- **Huffman leaves (codec 13) without the BMI2 flag. Fixed.** Kolmo called HUF with `flags = 0`. zstd's own frames
  pass `HUF_flags_bmi2` when the CPU has BMI2. In baseline-ISA builds (release binaries, wheels), Kolmo's leaves
  therefore never used the BMI2 decode loops or the assembly. On the leaf chunks Kolmo actually writes (dumped from
  real decodes), decoding is 1.15–1.21× faster with the flag:

  | chunks | rc2 (flags 0) | with the flag |
  |---|---|---|
  | `balanced`, structured | 815 MB/s | 979 MB/s |
  | `balanced`, Silesia | 754 MB/s | 868 MB/s |
  | `hc`, structured | 712 MB/s | 823 MB/s |
  | `hc`, Silesia | 608 MB/s | 735 MB/s |

  Same bytes. End to end the gain is below the noise, because Huffman leaves are about 2% of decode time on these
  sets. Native builds are unchanged.
- **Leaf format variants: rejected.** On the same chunks, these change the size by under 0.1% (`huf_bench.txt`):
  table log 8–12, optimal table depth, reusing the previous chunk's table, and forcing the single- or double-symbol
  decoder. No format change is justified.
- **zstd leaves without the frame (magicless, no content size): rejected.** Frame and block headers are about 10 bytes
  per frame plus 3 per 128 KiB. With 26 frames in the structured set's 3.2 MB output, that is under 0.03%, and frame
  parsing does not show in the profile.

## LZ4

**What Kolmo uses.**
- `LZ4_compress_fast` (`speed`, acceleration 1) and `LZ4_compress_HC_extStateHC` (codec 2);
- `LZ4_decompress_safe`;
- `LZ4_compress_default` on a 32 KiB sample as a planner probe.

**1. Reduced, done.** LZ4 is already two files. The linker now drops the 105 KiB (49%) of its code that Kolmo never
calls (streaming, dictionary and frame-less variants).

**2. Rewrite: no.** LZ4 is C99 and portable.

**3. Speed from owning it: none.** `speed` decompression runs LZ4 at 2.4 GB/s per thread (211 MB in 89 ms), and 70% of
the process time is file I/O and process start.

**4. Specialised code: none.**

## Brotli

**What Kolmo uses.** `BrotliEncoderCompress` (quality 8, window 2^24, generic mode) and `BrotliDecoderDecompress` for
`eco`.

**1. Reduced, done.** The encoder needs the static dictionary compiled in, so `dictionary.bin` and `dictionary.bin.br`
(the external-data variant) are removed. The linker drops 13 KiB. Brotli's encoder and decoder are otherwise all
reached at quality 8.

**2. Rewrite: no.** Brotli is C99.

**3. Speed from owning it: none.** `eco` spends 93% of Silesia compression in the quality-8 search (8.1 MB/s). That is
a mode choice (the TRADEOFFS.md cost model), not overhead. Brotli decoding is 62% of `eco` decompression and runs at
276 MB/s.

**4. Specialised code: none.**

## xxHash

**What Kolmo uses.**
- XXH3-64 for block checksums (`kolmo_xxh3.c`, with rc2's AVX2 runtime dispatch);
- XXH64 (zstd's copy) for the bsc payload check and for checksums in container versions 1–2.

**1.** Keep it. It costs 0.4–2% of decode time, and the linker drops the unused 75% of its code.

**2–4.** No rewrite. It is already specialised, and the simd lanes own any further kernels.

## libbsc and libsais (bwt lane)

For completeness:
- libbsc is the only C++ dependency, and the reason the build links `libstdc++`/`libc++`.
- It is 60% of `ratio` decompression time on Silesia, and 10% of `ratio` compression (plus the LZMA2 trials it
  competes with).

The bwt lane ports it to C (codec 18) and takes over the libsais subset.

## Recommendations, in the order implemented

1. **Vendor the liblzma subset (done).** No system library is needed; every build uses the same, signature-verified
   liblzma. LZMA2 decoding is 1.43× faster, with the same bytes.
2. **Trim the vendored libraries (done).** 63 unused zstd files and 2 Brotli blobs are removed. Function-section
   garbage collection drops 371 KiB of unreached code (−11% of the binary's code), with the same bytes and the same
   speed.
3. **Specialised integrated paths (done where measured).** Huffman leaves pass the BMI2 flag: 1.15–1.21× faster leaf
   decode on baseline x86-64, same bytes. Frameless zstd leaves, HUF table reuse and other leaf format variants were
   measured and rejected (< 0.1%).
4. **Own LZMA-class coder (codec 24): not written.** liblzma 5.8.1 is the faster decoder already, and no parse setting
   beats 9e at equal size. The remaining LZMA2 time is better recovered by skipping losing trials (bwt lane, item 3).
5. **For other lanes:**
   - simd-arm: measure whether an AArch64 Huffman decode loop gains on the M5 (zstd ships x86-64 assembly only; worth
     about 2% of `balanced` decode on x86).
   - bwt lane: the 21% of `ratio` compression spent on LZMA2 trials that lose.
