# deps lane results (rc3)

Branch `claude/rc3-deps-20261116`, from `claude/release-rc3-20261116` (`2207da1`). The decisions and the per-library
answers are in `docs/DEPENDENCIES.md`; this file holds the numbers, the commands and what did not work.

**Machine:** a cloud VM with an Intel Xeon at 2.10 GHz (4 vCPUs, AVX2 and BMI2), Linux 6.18, GCC 13.3. Every timing is
on one thread (`-T 1`), following `benchmark/rc2/PROTOCOL.md` (one thread for every tool, never mixed). The Mac (Apple
M5) repeat is left to the integration lane.

**Data:**
- Silesia: 12 files, 211,938,580 bytes.
- Structured set: 12 files, 28,714,555 bytes. Downloaded and kept outside the repository:
  - `owid-energy-data.csv` and `owid-co2-data.csv` from the OWID GitHub repositories;
  - `owid-covid-latest.json`;
  - the 9 inputs that `benchmark/determinism/check.py` generates.

## What changed in the code

| change | files | output | measured effect |
|---|---|---|---|
| liblzma subset vendored (XZ Utils 5.8.1, signature verified), system liblzma no longer linked | `vendor/xz/` (70 release files + 4 Kolmo files), `Makefile`, `src/core/common.h` (include), `src/codecs/lzma.c` (the buffer bound), `bench/kolmobench.c`, CI, `fuzz/build.sh`, `tune/ga.py`, both bindings | identical (determinism check: 110 frames) | LZMA2 decode 1.43× faster; `ratio` decompression −8% on Silesia |
| vendored zstd and Brotli files Kolmo never compiles removed | `vendor/zstd` (63 files), `vendor/brotli/common/dictionary.bin*` | identical | zstd tree 3.6 → 2.3 MB |
| function-section garbage collection of vendored code | `Makefile` (`VSECT`, `LDGC`) | identical | binary code 3.13 → 2.78 MB; speed within noise |
| Huffman leaves pass zstd's BMI2 flag | `src/codecs/huf.c` (`huf_flags`) | identical | 1.15–1.21× faster leaf decode on baseline x86-64; below noise end to end |
| `kolmobench`'s xz rival coded as raw LZMA2 | `bench/kolmobench.c` | rival only | the subset has no .xz container; xz's own figures add about 60 bytes and a CRC64 pass |

The format did not change. No `expected.sha256` regeneration was needed.

## 1. Where the time goes, per mode

**Tooling.**
- `benchmark/rc3/deps/libprof.c` is a `-Wl,--wrap` profiler around every library entry point Kolmo calls;
  `build_libprof.sh` builds it.
- `profile_modes.py` drives it: each file is compressed and then decompressed in its own process, and every phase is
  summed. `show.py` prints the tables.

Commands:

```
make all && sh benchmark/rc3/deps/build_libprof.sh
python3 benchmark/rc3/deps/profile_modes.py build/kolmo-libprof SILESIA results/libprof-silesia.tsv speed balanced eco hc ratio
python3 benchmark/rc3/deps/profile_modes.py build/kolmo-libprof STRUCT results/libprof-struct.tsv speed balanced eco hc ratio
python3 benchmark/rc3/deps/show.py results/libprof-silesia.tsv
```

**Silesia (system liblzma 5.4.5 build), 1 thread.** Time is the sum over the 12 files of process wall time:

| mode | output bytes | compress | top entries (share of compress) | decompress | top entries (share of decompress) |
|---|---|---|---|---|---|
| speed | 99,938,346 | 0.60 s | LZ4 70.4% | 0.29 s | LZ4 30.2% |
| balanced | 58,487,839 | 1.30 s | zstd 69.0% (587 calls: 477 at level 1, 110 at level 3), HUF 4.7% | 0.48 s | zstd 41.6% (105 frames), HUF 6.5% (1,937 chunks) |
| eco | 50,773,162 | 23.2 s | Brotli 93.2% | 1.02 s | Brotli 62.2% |
| hc | 47,871,778 | 85.3 s | zstd 98.4% (26 calls at 19, 2 at 22) | 0.72 s | zstd 38.8%, HUF 3.0% |
| ratio | 41,146,453 | 68.1 s | LZMA2 87.5% (24 calls, 119.9 MB), bsc 10.1% (27 calls) | 5.10 s | bsc 59.9%, LZMA2 33.5% (17 blocks, 90.9 MB) |

**Structured set, 1 thread (vendored liblzma).**

| mode | output bytes | compress | top entries | decompress | top entries |
|---|---|---|---|---|---|
| speed | 13,814,249 | 0.12 s | LZ4 55.3% | 0.08 s | LZ4 19.3% |
| balanced | 3,199,789 | 0.44 s | zstd 12.3%, HUF 1.3% | 0.12 s | zstd 11.0% (26 frames), HUF 2.2% (165 chunks) |
| eco | 2,981,479 | 1.49 s | Brotli 45.0% | 0.20 s | Brotli 34.8% |
| hc | 2,923,408 | 3.74 s | zstd 78.2% | 0.14 s | zstd 14.1%, HUF 2.4% |
| ratio | 2,756,925 | 11.9 s | LZMA2 91.6% (12 calls, 23.1 MB) | 0.30 s | LZMA2 45.6% |

Exact byte counts are in `results/libprof-struct.tsv` (column `out_bytes` of the `_total_` rows).

**Process start.** A 10-byte CSV takes 1.41 ms to compress and 1.23 ms to decompress (best of 40). That is about 15 ms of
the 115 ms `balanced` decompression of the structured set. The rest of the non-library time is Kolmo's own transforms
and coders, which belong to the other lanes.

## 2. liblzma: system 5.4.5 against vendored 5.8.1

**Verification.**
- Tarball `xz-5.8.1.tar.gz` from the GitHub release, SHA-256
  `507825b599356c10dca1cd720c9d0d0c9d5400b9de300af00e4d1ea150795543`.
- `gpg --verify xz-5.8.1.tar.gz.sig`: "Good signature from Lasse Collin", key `3690C240CE51B4670D30AD1C38EE757D69184620`.
- After copying, every release file in `vendor/xz` was compared with `cmp` against the tarball.

**Identical output.**
- `benchmark/determinism/check.py` passes unchanged (110 frames). Its expected hashes were made with the system
  library.
- Raw LZMA2 at 9e of xml, dickens and x-ray is byte-identical between the two libraries.
- `kolmo_lzma2_bound()` equals `lzma_stream_buffer_bound()` for 0..5 MB in steps of 997 and for 10 edge sizes (a test
  program against the system library). So every buffer Kolmo sizes from it is unchanged.

**Coder A/B** (`benchmark/rc3/deps/lzma_ab.c`: the same program linked with `-llzma` or with the subset; settings as
codec 6). The run used dickens, osdb, ooffice, sao, x-ray and xml (47.5 MB), best of 2, in the order sys, vend, sys,
vend (`results/lzma_ab_9e.txt`):

| run | compressed | compress | decompress |
|---|---|---|---|
| system 5.4.5, round 1 | 17,454,442 | 1.47 MB/s | 44.7 MB/s |
| vendored 5.8.1, round 1 | 17,454,442 | 1.44 MB/s | 63.8 MB/s |
| system 5.4.5, round 2 | 17,454,442 | 1.49 MB/s | 45.5 MB/s |
| vendored 5.8.1, round 2 | 17,454,442 | 1.40 MB/s | 64.9 MB/s |

**Release versus flags** (`results/lzma_ab_flags_p6.txt`, preset 6, 4 files, best of 3):

| build | compress | decompress |
|---|---|---|
| system | 1.35 MB/s | 42.5 MB/s |
| vendored `-O2` | 1.39 MB/s | 60.0 MB/s |
| vendored `-O3` | 1.40 MB/s | 64.2 MB/s |
| vendored `-O3 -march=native` | 1.35 MB/s | 60.6 MB/s |

The decode gain is the release (the LZMA decoder was sped up after 5.4), not the compiler flags. Compression is the
same within about 4%.

**Inside Kolmo `ratio` on Silesia** (`results/libprof-silesia-xz581.tsv`): LZMA2 decoding took 1,120 ms against
1,710 ms with the system library. The whole decompression went from 5.10 s to 4.67 s (−8%). Compression went from
68.1 s to 71.7 s, which is within this VM's run-to-run spread (a later identical run took 72.6 s with bsc slower too);
the coder A/B above puts the encoder at −3% ± 4%.

## 3. LZMA2 parse settings inside `ratio`

Kolmo's dictionary size and lc/lp/pb were kept; only the match finder and parser fields of the preset were changed,
through libprof's `LIBPROF_LZMA_PRESET`/`_NICE`/`_DEPTH` (`preset_sweep.sh`).

| setting | structured: output | compress | Silesia: output | compress |
|---|---|---|---|---|
| 9e (rc2: nice 273, depth 512) | 2,756,925 | 11.7 s | 41,146,453 | 72.6 s |
| depth auto (152) | 2,758,711 (+0.065%) | 10.5 s | | |
| depth 128 | 2,759,211 (+0.083%) | 10.0 s | | |
| depth 64 | 2,762,939 (+0.22%) | 8.3 s | | |
| nice 192 | 2,762,689 (+0.21%) | 9.3 s | | |
| nice 128 | 2,770,152 (+0.48%) | 7.7 s | | |
| non-extreme (presets 6, 7 and 9 are the same once the dictionary is fixed: nice 64, depth auto) | 2,789,830 (+1.19%) | 5.0 s | 41,153,856 (+0.018%) | 60.2 s |

No setting gives 9e's size for less time, so 9e stays. On Silesia, `ratio` encodes 24 LZMA2 trials for 17 LZMA2
blocks kept: about 29 MB of the 120 MB LZMA2 input is coded and then discarded in favour of BWT, ≈ 21% of compression
time. That is the bwt lane's "drop the LZMA2 trial when BWT clearly wins".

**Decoding straight into the output** (`lzma_dictcost.c`): a fresh mapping filled and copied, the overhead a
direct-to-output LZMA2 decoder would remove, costs 22.2 ms per 47.5 MB. That is 3.0% of LZMA2 decode time at
64 MB/s, ≈ 0.75% of `ratio` decompression on Silesia. Not done.

## 4. Huffman leaves (codec 13)

**Method.**
- `LIBPROF_DUMP_HUF=FILE build/kolmo-libprof2 -d ...` writes every Huffman chunk Kolmo decodes.
- Chunks were dumped for `balanced` and `hc` over both sets: 165, 1,937, 277 and 2,213 chunks, averaging 5–12 KB.
- `huf_bench.c` re-encodes them as `huf_compress_all` does and times decoder variants (`results/huf_bench.txt`,
  `results/huf_bench_idle.txt`).

**Decode speed**, baseline x86-64 build (`-O3`, no `-march`, the release configuration), idle machine, best of 15 in
each of 2 rounds:

| chunks | rc2 (`flags = 0`) | `HUF_flags_bmi2` | gain |
|---|---|---|---|
| balanced, structured | 815 / 505 MB/s | 979 / 784 MB/s | 1.20× (best of rounds) |
| balanced, Silesia | 754 / 691 MB/s | 864 / 868 MB/s | 1.15× |
| hc, structured | 712 / 700 MB/s | 823 / 815 MB/s | 1.16× |
| hc, Silesia | 608 / 581 MB/s | 723 / 735 MB/s | 1.21× |

In the `-march=native` build the flag makes no difference, as expected (BMI2 is compiled in). Forcing the
single-symbol or double-symbol decoder is within ±5% of the automatic choice.

**End to end** (`ab_cli.py`, baseline builds with and without the flag, structured set; `results/ab_hufflags_baseline.txt`):
- `balanced` decode: 0.111 s against 0.115 s.
- `hc` decode: 0.138 s against 0.148 s.

Both within noise, in the wrong direction. Huffman leaves are about 2% of decode time, so a 1.2× gain there is
0.3–0.4%. The change is kept because it is free and gives the same bytes: it makes Kolmo call HUF as zstd itself does.

**Size variants (rejected):**

| variant | balanced, Silesia chunks | structured |
|---|---|---|
| table log 11 (rc2) | 18,916,839 | 1,554,289 |
| table log 8 / 9 / 10 / 12 | +1.95% / +1.76% / +0.31% / −0.09% | +0.10% / +0.06% / +0.01% / −0.003% |
| optimal depth | −0.014% | −0.04% |
| reuse the previous table when smaller | ±0 (never chosen) | ±0 |

## 5. zstd's x86-64 assembly and the frame overhead

**Assembly A/B** (`ab_cli.py`, baseline builds with and without `-DZSTD_DISABLE_ASM`, Silesia `balanced`, best of 7;
`results/ab_zstd_asm_baseline.txt`):
- decode: 0.469 s against 0.480 s (2%);
- encode: 1.365 s against 1.468 s (noise: the assembly is decode-only).

**Frames.** `balanced` writes 105 zstd frames for 208 MB of Silesia and 26 for the structured set. A frame costs about
10 bytes plus 3 per 128 KiB block. Frameless or magicless leaves would save under 1 KB on the 3.2 MB structured
output (< 0.03%), so this was not pursued.

## 6. Trimming

**Garbage collection measured without the Makefile change.** Vendored objects and `kolmo.o` were rebuilt with
`-ffunction-sections -fdata-sections` and linked with `--gc-sections --print-gc-sections`. Unused code and read-only
data per library:

| library | code + rodata | unused |
|---|---|---|
| zstd | 808 KiB | 152 KiB (19%) |
| LZ4 | 214 KiB | 105 KiB (49%) |
| libbsc | 324 KiB | 59 KiB (18%) |
| xxHash | 36 KiB | 27 KiB (75%) |
| Brotli | 861 KiB | 13 KiB (2%) |
| xz subset | 82 KiB | 5 KiB (6%) |
| kolmo.o | 744 KiB | 10 KiB (1%) |

**After the Makefile change** (vendored code only): `size build/kolmo` text 3,127,481 → 2,777,165 bytes.

**Speed** (`results/ab_gcsections.txt`): two runs of `ab_cli.py`, 24 files, 5 and 9 rounds. The differences change
sign between runs (−4% to +9%), within noise, with the same bytes.

**Files removed:**
- `vendor/zstd`: `legacy/` (built only with `ZSTD_LEGACY_SUPPORT ≥ 1`; Kolmo sets 0), `deprecated/`, `dll/`, a stale
  `obj/` of make dependency files, `BUCK`, `Makefile`, `libzstd.mk`, `libzstd.pc.in`, `module.modulemap` and
  `.gitignore`.
- `vendor/brotli/common/dictionary.bin*`: the dictionary is compiled in from `dictionary.c`.

## Tests run before pushing

All passed:
- `make check`
- `make check-rc`
- `make check-sanitizers`
- `make experimental-check GPU_CHECK=`
- `make kfam-check`
- `python3 benchmark/determinism/check.py build/kolmo build/determinism`: 110 frames OK
- the Python binding round trip (`make lib`, `bindings/python/test_kolmo.py`, `python3 setup.py build` from the synced
  `native/`)
- the Rust binding (`cargo test`)

Every one ran with no system liblzma linked (`ldd build/kolmo` shows none).

## What failed or was not done

- **No `perf` on the VM.** The `--wrap` profiler measures time per library entry point. It does not show where inside
  a library the time goes; inside-library hot spots were not needed for the decisions above.
- **First compile of the xz subset refused.** It was compiled from the download directory and the session's safety
  check refused it. The subset was copied into `vendor/xz` and built from the repository with Kolmo's config instead.
- **7-Zip's LZMA SDK decoder (C and asm) not measured.** It is public domain, a licence not on the plan's list. Its
  possible gain is bounded above (≤ about 6% of Silesia `ratio` decode at a 1.3× faster decoder).
- **Not repeated on the Apple M5.** In particular, the AArch64 Huffman question (zstd ships no AArch64 assembly) is
  for the simd-arm lane.
- **Structure lane merged late.** `claude/rc3-structure-20261116` appeared after the four deps commits were made on
  the old layout. It was merged into this branch (`git merge`), and the hooks moved into the new files:
  - the include into `src/core/common.h`;
  - `kolmo_lzma2_bound` into `src/codecs/lzma.c`;
  - `huf_flags` into `src/codecs/huf.c`;
  - `-Ivendor/xz` and the `-llzma` removal into the structure lane's new CI, fuzz, `tune/ga.py`, Rust build script and
    lzbench setup.

  The merged `src/` differs from the structure branch only by those three hooks. Every test above was run again on the
  merged tree before pushing.
