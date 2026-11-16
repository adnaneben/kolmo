# rc3 simd-arm: NEON kernels for Kolmo's own hot loops on arm64

Branch `claude/rc3-simd-arm-20261116`, from `claude/release-rc3-20261116` (`2207da1`), with the structure lane
(`9ef225d`) merged. Lane brief: `research/RC3-PLAN.md` ("simd-x86 and simd-arm"), conventions: `docs/LAYOUT.md`.

## Summary

- **Ten kernels landed**, each with its C reference kept in the calling part and a test that compares the two. On
  one thread they cut decode time by 20–62% wherever Kolmo's own transforms dominate the decoder, and compression
  time by up to 42% on images and 8–9% on interleaved PCM and DICOM. The largest wins are listed below.
- **Output is unchanged.** The determinism check passes (110 frames). The structure lane's frame comparison,
  between a `NO_ASM=1` build (C references only) and the kernel build, gives 1040 of 1040 frames identical and
  cross-decoded (32 inputs × 18 settings × 1 and 4 threads). `tests/test_arch.c` compresses seven kinds of data
  in five modes with the kernels on and off and gets identical frames. Every frame in the end-to-end benchmark
  (below) was compared byte for byte against the branch point's.
- **End to end against the branch point** (one thread, 15 interleaved rounds): decode is 13–74% faster on text and
  JSON (`eco`, `hc`), CSV, RGB and Y4M, 16-bit images, WAV, DICOM, int32 and float64 arrays. 16-bit image
  compression is 27–38% faster. Nothing else changed beyond noise, apart from a code-placement artefact in the
  vendored LZ4 decoder: `-m speed` decode of JSON and logs was 21% slower with a byte-identical `lz4.o`. It is
  fixed by aligning LZ4's functions and loops to 64 bytes (`aac4b60`; details in the end-to-end section).
- **What did not gain:** FEAT_CSSC scalar min/max in the 16-bit median chain (7–13% slower), a NEON word-transform
  *encoder* (5–10% slower), and hand-written NEON for the CM mixer (clang already vectorises it). Kolmo's own rANS
  leaf stays scalar: its 64-bit states have no NEON multiply.
- **Why the krans NEON decoder is slower than scalar on the M5** (measured here for the first time): see the
  krans section. It is 2.3× slower with 8 ways and with order 1. It is 1.2–1.3× *faster* with 16 and 32 ways at
  order 0. Each round of 4 lanes is a long dependency chain: lane values move to general registers to address the
  tables (there is no gather), the loaded entries go back into a vector, and a horizontal ADDV forms the
  renormalisation mask, which then needs a dependent shuffle-row load. The scalar decoder runs 8 short chains in
  parallel.
- **No assembly.** Every kernel is NEON intrinsics in `src/arch/aarch64/*_neon.c`. The loops are latency- or
  load-bound, and clang schedules them well. No `.S` file was justified, so `src/arch/asm.h` (not yet written by
  the structure lane) was not needed.

Machine: Apple M5 (4 performance + 6 efficiency cores, 128 KiB L1D and 16 MiB L2 per performance cluster, 24 GB),
macOS 27.2, Apple clang 21.0.0. Flags are the Makefile default, `-O3 -mcpu=native -ffp-contract=off`. NEON is
baseline on arm64, so no run-time dispatch was needed; `-mcpu=native` adds nothing that the kernels use. **The
machine was shared** with other rc3 lanes during every measurement (load average 4–5; another session's
`kolmotest` ran at the same time). Every timing is therefore an interleaved A/B with an A/A control, as described
below.

## Kernels

All files are in `src/arch/aarch64/`. They are included into the library's single translation unit from
`src/kolmo.c`, after the other arch parts. `src/arch/arch.h` defines `KOLMO_NEON` (arm64 builds without
`NO_ASM=1`) and `kolmo_force_c`, which sends every call to its C reference (tests and A/B only). B/A is the
time with the kernel divided by the time with the C reference, both in the same binary: the median of 15
interleaved rounds, with the interquartile range in brackets. A/A, the same build against itself, stayed within
±0.5% on large inputs and within ±3% on small ones.

| kernel | file | C reference | what it does | measured (1 thread, B/A) |
|---|---|---|---|---|
| word-transform decoder | `word_neon.c` | `word_decode` loop (`transforms/word.c`) | classifies 64 bytes per step with TBL lookups into two bit masks (special bytes, capital markers) and walks the tokens with CTZ, so the chain from one token to the next is a few integer operations; literal runs and words move in 16-byte pieces. It stops at a token boundary near the ends, and in front of any token the reference would reject | dickens decode, -m hc **0.642** [0.641, 0.645], -m eco 0.799 |
| byte-plane split / merge with delta | `shuffle_neon.c` | `shuffle_fwd` / `shuffle_inv` (`transforms/shuffle.c`) | 2/4/8-byte words, 1–4 channels, 16 rows per step: LD1–LD4 separate the channels; the delta is EXT + SUB, and its inverse is an in-register prefix sum with a carry lane; planes come out through UZP and go back through ZIP | mr -m speed decode **0.431**, compress 0.919; stereo PCM16 WAV balanced decode **0.498**, compress 0.914 |
| IMG8 inverse (median predictor) | `image_neon.c` | `kg_img8_back` (`transforms/graph.c`) | runs 16 rows as a wavefront, one column apart: residual tiles are read along the skew and transposed 16×16, 16 steps of byte min/max/saturating arithmetic give the exact median, and the tiles are transposed back | rgb8.ppm balanced decode **0.382**, eco 0.596 |
| IMG8 / IMG16 forward residuals | `image_neon.c` | `kg_img8_fwd`, `img16_fwd` (`transforms/sci.c`) | 16 values per step (all three IMG16 predictors) | img16.pgm balanced compress **0.589**, x-ray **0.579** (with the SAD below) |
| 16-bit SAD | `image_neon.c` | the row-stride search of `img16_detect` | UABD + UADALP, steps 1 and 4 (integer sums, so the scores are the same) | (in the line above) |
| IMG16 inverse | `image_neon.c` + portable C | `img16_inv` | mode 0: NEON running sum. Mode 2 (median): portable C with a 4-operation chain. With d = U − UL, the median is L + max(d, 0) when L < min(U, UL), L + min(d, 0) when L > max(U, UL), and U otherwise; values sit in the top 16 bits, so no mask is needed | mr balanced decode **0.759**, img16.pgm 0.753 |
| 32-bit integer-graph inverse | `intinv_neon.c` | `fi_inv_4_*` / `kg_int_inv` | zips 1/2/4/8 residual planes into 32-bit lanes; orders 0–2 and any factor are prefix sums with a carry (bit 32 of z feeds unzigzag) | int32 walk, balanced decode **0.748**, hc 0.704, ratio 0.903 |
| table-decoder helpers | `table_neon.c` | `row_nonempty`, `compact_rows`, the kind-count pass (`transforms/table.c`) | a real 64-bit field mask (the old NEON code walked a nibble mask bit by bit), left-pack of number indices from a 256-entry table, kind counts in byte lanes | owid-co2 CSV decode **0.887**, hc 0.886, energy_test.csv 0.879 |
| DEC64 inverse (decimal doubles) | `dec64_neon.c` | `KG_DEC_VALUE` (`kg_dec64_inv`) | 16 values per step: planes into 64-bit lanes, running sums as 2-lane prefix sums, FCVT + FDIV on 2 lanes (IEEE division rounds the same). Missing and exception tags enter the sums as 0 and are written afterwards in stream order | owid_energy_f64.npy decode, balanced **0.785**, hc 0.745, eco 0.875 |
| planner image trial | portable C | the old loop order in `kg_img8_trial` | puts the sampled-position loop outside the lag loop, so the lagged reads stay in cache (same sums, same choice) | rgb8.ppm balanced compress 0.949 (0.984 before this change), clip.y4m 0.955 |
| kvid strip prediction (research tool) | `kvid_neon.c` | `predict` / `encode_plane` / `decode_plane` in `experimental/kvid.c` (`KVID_FORCE_C=1`) | encoder costs and residuals of the three predictors, 16 per step (signed medians on bytes biased by 128); the decoder adds temporal strips as vectors and decodes intra strips with the IMG8 wavefront (`kolmo_med_band16_neon`) | clip.y4m `kvid -b`, -m rt compress **0.464**, decode **0.392**; -m balanced 0.695 / 0.517 |

Tests (`make check-arch`, part of `check-rc`, optimised and ASan/UBSan) cover: 60 generated texts and 1,200
damaged word streams; 400 shuffle shapes; 300 8-bit images (widths 1–3,000, 1–4 channels, random residuals); 300
16-bit forward/inverse cases (all modes); 2,000 SAD sums; 12 runs of the whole `img16_detect`; 16 planner image
trials; 3,000 integer inverses; 20,000 field masks and index lists; 3,000 kind counts; and whole frames as above.
`test-fastint` (1,098,727 programs) and `test-dec64` (389 tables × 150 damaged copies) pass with the kernels in
place. `tests/test_kvid_kernels.py` (part of `experimental-check`) compresses 7 clips in 2 modes. A deliberately
broken kernel was caught by its test in each case tried (word, shuffle, IMG8 wavefront, kvid).

## Profile (before)

`benchmark/rc3/simd-arm/prof.py` runs each case in a loop (`proploop.c`, in-process, one thread) and attaches
macOS `sample` (1 ms) for 3 s. The inputs are 12 files × `speed`, `balanced`, `eco`, `hc`, `ratio`, `-x` ×
compress/decode. Line-level profiles came from the same loop with an in-process SIGPROF sampler and `atos -i`
(`pcsym.py`). The raw output is in `results/profile-before.txt`. Inputs are text (dickens), a table
(owid-co2-data.csv), a log (loghub_all.log), JSON (api.json), 8-bit RGB (rgb8.ppm), video (clip.y4m, CIF, 60
frames), 16-bit images (x-ray, mr, img16.pgm), audio (music.wav, stereo PCM16), int32 (walk_i32.npy) and float64
(owid_energy_f64.npy). The synthetic ones come from `mk_inputs.py`: the benchmark set has no WAV, Y4M or
16-bit PGM.

Kolmo-owned share of the sampled time, before (cases with 3% or more), and what was done:

| input | mode, direction | Kolmo-owned hot spots (share) | done |
|---|---|---|---|
| text | hc / eco decode | `wd_scan_low` + `word_decode` 45% / 29% | word decoder |
| text, JSON | ratio / eco compress | `word_encode` 21% / 14% (JSON 35% / 39%) | tried, slower (see below) |
| JSON | hc / eco decode | `word_decode` + `wd_scan_low` 69% / 41% | word decoder |
| table | balanced decode | `tbl2_stream` 72% (spread: masks, index lists, formatter, row assembly) | table helpers |
| table | balanced compress | `ncol_parse` 38%, `tbl2_encode` 28% (SWAR parsing, splitting already NEON) | no kernel: spread over many lines |
| log | balanced decode / compress | `log_decode` 45% / `log_encode_m` 39% (template emission, already SWAR) | no kernel: scattered small copies |
| RGB, Y4M | balanced / hc decode | `kg_img8_back` 78% (Y4M 87%) | IMG8 wavefront |
| RGB, Y4M | eco / ratio decode | `kg_img8_back` 51%, `rans_decompress_all` 48% | wavefront; rANS stays scalar |
| RGB, Y4M | balanced compress | `kg_img8_trial` 16% / 7% | trial loop order |
| 16-bit | balanced compress | `img16_detect` 42–68%, `img16_encode` 12–25% | forward residuals, SAD |
| 16-bit | balanced / hc decode | `img16_decode` 49–76% | IMG16 inverse |
| mr, int32 | speed decode | `shuffle_inv` 60% / 79% | shuffle |
| WAV | balanced decode | `shuffle_inv` 48% | shuffle |
| int32 | balanced / hc decode | `fi_inv_4_*` 46% / 54% | integer inverse |
| int32 | eco / ratio / -x decode | `rans_decompress_all` 72–75% | stays scalar (below) |
| float64 | balanced / hc decode | `kg_dec64_inv` 50% / 58% | DEC64 inverse |
| all | -x | `cm_compress_m` 93–99%; line level: `cm_find` 48% (first touch of a bucket: cache miss), mixer ~8% | no kernel (below) |
| all | speed / balanced / hc / ratio compress of text | vendored zstd, LZ4, Brotli, liblzma, libbsc 80–98% | not ours |

## End to end against the branch point

`e2e.sh` compares this branch (`NEW`, at `1c60e76`, before the LZ4 alignment below) against the structure lane's
head `9ef225d` (`BASE`, the branch point after the merge). Each case is 15 interleaved rounds of BASE, NEW and BASE
again (A/A), one thread, the fastest iteration of each process over 0.3 s (one iteration per process for `hc`,
`ratio` and `-x` compression). The decode frames of both builds were compared byte for byte, and all of them were
identical. "change" is shown only where the interquartile range of new/base clears the A/A noise of that case.
dickens has every mode in both directions. The other inputs have every decode, and compression in `speed` and
`balanced` only: the full matrix takes hours on the shared machine, and dickens's eco, hc and ratio compression,
which is the vendored coders' time, was within noise. `-x` was run on the first 2 MB of dickens.

Results (raw: `results/e2e.jsonl`):

- **Decode is 13–74% faster** wherever a kernel sits on the decode path: text and JSON in `eco` and `hc`
  (word transform), CSV in every mode but `speed`, RGB and Y4M in every mode but `speed` (IMG8 wavefront;
  `clip.y4m` balanced decode −74%), 16-bit images, interleaved PCM16 and DICOM (shuffle), int32 arrays (shuffle
  in `speed` −73%, integer inverse) and decimal doubles.
- **Compression** is 27–38% faster on 16-bit images (x-ray, mr, img16.pgm, balanced), 11% faster on stereo WAV
  and 13% on int32 in `speed`, 4.5% faster on RGB in balanced, and unchanged elsewhere.
- **`-x`** is unchanged: CM dominates, and no kernel is on its path.
- **One regression, a code-placement artefact, fixed in `aac4b60`:** `-m speed` decode of loghub_all.log and
  api.json was 21–22% slower. Every sample of both builds is in the vendored `LZ4_decompress_safe`, and `lz4.o` is
  byte-identical in the two builds; only the function's address modulo 64 changed (12 against 52) because the
  library grew. Compiling `lz4.c` with `-falign-functions=64 -falign-loops=64` makes both builds decode api.json
  in 0.38 ms; unaligned they took 0.37 and 0.45 ms. The table below is the speed-mode decode re-run with the
  aligned LZ4 (`results/e2e-speed-aligned-lz4.jsonl`). The JSON and log cases are back to base. x-ray and
  clip.y4m are 4–5% slower than base, but that is the aligned placement against base's fortunate one: with LZ4
  aligned in both builds, new/base is 1.002 (clip.y4m), 1.014 (x-ray, A/A 1.012) and 1.011 (api.json, A/A 1.017),
  that is, equal. Alignment removes the placement lottery (±20%) at a cost of at most 5% on two inputs at
  10–20 GB/s. The integration lane may prefer to drop it.

| input | mode | direction | base ms | new ms | new/base [IQR] | A/A [IQR] | change |
|---|---|---|---:|---:|---:|---:|---:|
| dickens | speed | decode | 2.09 | 2.09 | 1.000 [0.998, 1.003] | 1.000 [0.999, 1.007] | noise |
| dickens | speed | compress | 17.00 | 16.98 | 0.998 [0.992, 1.008] | 0.991 [0.984, 1.000] | noise |
| dickens | balanced | decode | 6.64 | 6.64 | 1.004 [0.983, 1.022] | 1.002 [0.992, 1.016] | noise |
| dickens | balanced | compress | 35.65 | 35.75 | 1.005 [0.999, 1.021] | 1.005 [0.990, 1.011] | noise |
| dickens | eco | decode | 23.70 | 19.23 | 0.809 [0.805, 0.822] | 0.998 [0.995, 1.012] | -19.1% |
| dickens | eco | compress | 463.27 | 466.17 | 1.001 [0.979, 1.013] | 0.997 [0.949, 1.001] | noise |
| dickens | hc | decode | 12.96 | 8.47 | 0.649 [0.645, 0.652] | 1.003 [0.995, 1.009] | -35.1% |
| dickens | hc | compress | 1743.10 | 1715.37 | 1.003 [0.986, 1.022] | 1.006 [0.987, 1.016] | noise |
| dickens | ratio | decode | 177.51 | 177.68 | 1.000 [0.996, 1.012] | 1.003 [0.997, 1.008] | noise |
| dickens | ratio | compress | 270.44 | 269.92 | 1.006 [0.989, 1.008] | 1.003 [0.994, 1.015] | noise |
| owid-co2-data.csv | speed | decode | 3.07 | 3.07 | 1.001 [1.000, 1.002] | 1.000 [1.000, 1.002] | noise |
| owid-co2-data.csv | speed | compress | 23.84 | 24.20 | 1.013 [0.969, 1.052] | 0.989 [0.959, 1.069] | noise |
| owid-co2-data.csv | balanced | decode | 18.50 | 15.69 | 0.848 [0.756, 0.893] | 0.970 [0.944, 1.056] | -15.2% |
| owid-co2-data.csv | balanced | compress | 61.90 | 64.70 | 1.014 [0.978, 1.045] | 1.002 [0.927, 1.114] | noise |
| owid-co2-data.csv | eco | decode | 28.38 | 24.21 | 0.858 [0.828, 0.878] | 0.973 [0.965, 0.999] | -14.2% |
| owid-co2-data.csv | hc | decode | 16.22 | 13.39 | 0.822 [0.813, 0.824] | 1.001 [0.993, 1.017] | -17.8% |
| owid-co2-data.csv | ratio | decode | 47.62 | 45.15 | 0.945 [0.939, 0.953] | 1.006 [0.998, 1.013] | -5.5% |
| loghub_all.log | speed | decode | 0.41 | 0.49 | 1.214 [1.197, 1.219] | 1.002 [0.998, 1.007] | +21.4% |
| loghub_all.log | speed | compress | 2.41 | 2.39 | 0.989 [0.986, 0.996] | 0.998 [0.993, 1.006] | noise |
| loghub_all.log | balanced | decode | 2.03 | 2.04 | 1.003 [0.989, 1.007] | 0.998 [0.987, 1.003] | noise |
| loghub_all.log | balanced | compress | 9.41 | 9.35 | 1.001 [0.992, 1.011] | 0.997 [0.992, 1.021] | noise |
| loghub_all.log | eco | decode | 3.70 | 3.68 | 0.996 [0.990, 1.000] | 0.998 [0.996, 1.003] | noise |
| loghub_all.log | hc | decode | 2.14 | 2.14 | 1.002 [0.999, 1.004] | 1.002 [0.998, 1.006] | noise |
| loghub_all.log | ratio | decode | 9.71 | 9.69 | 0.998 [0.996, 1.000] | 1.001 [0.998, 1.004] | noise |
| api.json | speed | decode | 0.36 | 0.45 | 1.223 [1.217, 1.235] | 1.000 [0.993, 1.008] | +22.3% |
| api.json | speed | compress | 2.28 | 2.26 | 0.991 [0.983, 0.996] | 1.006 [1.002, 1.010] | noise |
| api.json | balanced | decode | 0.82 | 0.82 | 1.004 [0.999, 1.010] | 0.999 [0.998, 1.006] | noise |
| api.json | balanced | compress | 3.30 | 3.32 | 1.005 [1.001, 1.011] | 0.996 [0.990, 1.010] | noise |
| api.json | eco | decode | 3.63 | 2.74 | 0.752 [0.747, 0.759] | 1.002 [0.990, 1.008] | -24.8% |
| api.json | hc | decode | 2.55 | 1.66 | 0.649 [0.645, 0.655] | 1.005 [0.997, 1.013] | -35.1% |
| api.json | ratio | decode | 16.04 | 16.08 | 1.004 [0.993, 1.021] | 1.004 [0.989, 1.023] | noise |
| rgb8.ppm | speed | decode | 0.07 | 0.07 | 1.000 [1.000, 1.000] | 1.000 [1.000, 1.007] | noise |
| rgb8.ppm | speed | compress | 0.32 | 0.33 | 1.041 [1.031, 1.048] | 1.019 [1.005, 1.037] | noise |
| rgb8.ppm | balanced | decode | 4.06 | 1.56 | 0.384 [0.383, 0.406] | 0.998 [0.986, 1.056] | -61.6% |
| rgb8.ppm | balanced | compress | 14.69 | 14.02 | 0.955 [0.941, 0.959] | 1.004 [0.994, 1.008] | -4.5% |
| rgb8.ppm | eco | decode | 6.37 | 3.74 | 0.587 [0.584, 0.592] | 1.001 [0.999, 1.008] | -41.3% |
| rgb8.ppm | hc | decode | 4.04 | 1.56 | 0.386 [0.386, 0.407] | 1.000 [0.995, 1.030] | -61.4% |
| rgb8.ppm | ratio | decode | 6.34 | 3.72 | 0.585 [0.585, 0.588] | 0.999 [0.996, 1.002] | -41.5% |
| clip.y4m | speed | decode | 0.98 | 0.98 | 0.999 [0.994, 1.008] | 0.996 [0.993, 1.006] | noise |
| clip.y4m | speed | compress | 6.99 | 6.95 | 1.006 [0.999, 1.008] | 1.002 [0.997, 1.007] | noise |
| clip.y4m | balanced | decode | 26.94 | 7.05 | 0.261 [0.261, 0.264] | 1.000 [0.997, 1.001] | -73.9% |
| clip.y4m | balanced | compress | 34.93 | 34.56 | 0.970 [0.951, 1.036] | 0.998 [0.995, 1.088] | noise |
| clip.y4m | eco | decode | 42.15 | 18.91 | 0.452 [0.445, 0.458] | 0.999 [0.967, 1.047] | -54.8% |
| clip.y4m | hc | decode | 30.07 | 8.49 | 0.280 [0.278, 0.284] | 0.997 [0.989, 1.003] | -72.0% |
| clip.y4m | ratio | decode | 246.44 | 225.46 | 0.915 [0.904, 0.921] | 0.997 [0.993, 1.011] | -8.5% |
| x-ray | speed | decode | 0.42 | 0.43 | 1.026 [1.018, 1.035] | 1.007 [0.998, 1.012] | +2.6% |
| x-ray | speed | compress | 2.80 | 2.83 | 1.009 [1.007, 1.011] | 1.003 [0.997, 1.008] | noise |
| x-ray | balanced | decode | 6.23 | 6.22 | 0.999 [0.997, 1.002] | 0.999 [0.998, 1.003] | noise |
| x-ray | balanced | compress | 17.02 | 10.54 | 0.620 [0.613, 0.628] | 1.002 [0.996, 1.009] | -38.0% |
| x-ray | eco | decode | 21.24 | 21.15 | 0.994 [0.991, 1.004] | 0.999 [0.993, 1.008] | noise |
| x-ray | hc | decode | 6.30 | 6.29 | 1.000 [0.996, 1.003] | 0.999 [0.996, 1.000] | noise |
| x-ray | ratio | decode | 112.48 | 111.27 | 0.994 [0.986, 0.999] | 1.001 [0.992, 1.003] | noise |
| img16.pgm | speed | decode | 0.07 | 0.07 | 1.000 [1.000, 1.000] | 1.000 [1.000, 1.000] | noise |
| img16.pgm | speed | compress | 0.24 | 0.24 | 0.992 [0.988, 0.998] | 0.996 [0.996, 1.000] | noise |
| img16.pgm | balanced | decode | 2.30 | 1.74 | 0.756 [0.752, 0.792] | 1.000 [0.961, 1.051] | -24.4% |
| img16.pgm | balanced | compress | 10.22 | 6.37 | 0.623 [0.621, 0.625] | 1.002 [0.998, 1.006] | -37.7% |
| img16.pgm | eco | decode | 4.33 | 3.76 | 0.868 [0.866, 0.874] | 1.003 [0.997, 1.006] | -13.2% |
| img16.pgm | hc | decode | 2.31 | 1.73 | 0.748 [0.746, 0.750] | 1.000 [0.999, 1.000] | -25.2% |
| img16.pgm | ratio | decode | 26.76 | 25.99 | 0.970 [0.963, 0.972] | 1.001 [0.998, 1.012] | -3.0% |
| mr | speed | decode | 2.88 | 1.27 | 0.440 [0.434, 0.448] | 1.014 [1.008, 1.038] | -56.0% |
| mr | speed | compress | 6.27 | 5.88 | 0.931 [0.903, 0.947] | 0.996 [0.975, 1.021] | -6.9% |
| mr | balanced | decode | 11.63 | 8.97 | 0.777 [0.770, 0.792] | 1.007 [0.996, 1.022] | -22.3% |
| mr | balanced | compress | 29.14 | 20.38 | 0.728 [0.697, 0.745] | 0.996 [0.953, 1.055] | -27.2% |
| mr | eco | decode | 19.26 | 16.92 | 0.860 [0.839, 0.914] | 1.019 [0.975, 1.126] | noise |
| mr | hc | decode | 12.50 | 10.21 | 0.795 [0.781, 0.823] | 1.002 [0.989, 1.013] | -20.5% |
| mr | ratio | decode | 131.49 | 129.99 | 0.985 [0.954, 1.030] | 0.989 [0.963, 1.029] | noise |
| music.wav | speed | decode | 0.11 | 0.11 | 1.000 [0.991, 1.009] | 1.000 [0.991, 1.005] | noise |
| music.wav | speed | compress | 0.25 | 0.25 | 1.000 [0.996, 1.010] | 1.004 [1.000, 1.004] | noise |
| music.wav | balanced | decode | 2.47 | 1.22 | 0.496 [0.496, 0.497] | 1.002 [1.001, 1.005] | -50.4% |
| music.wav | balanced | compress | 9.91 | 8.79 | 0.892 [0.887, 0.900] | 0.994 [0.987, 1.001] | -10.8% |
| music.wav | eco | decode | 6.88 | 6.81 | 0.996 [0.960, 1.000] | 0.996 [0.984, 1.001] | noise |
| music.wav | hc | decode | 2.40 | 2.40 | 1.000 [0.992, 1.004] | 0.997 [0.994, 1.005] | noise |
| music.wav | ratio | decode | 10.14 | 10.09 | 0.996 [0.974, 1.005] | 0.999 [0.987, 1.014] | noise |
| walk_i32.npy | speed | decode | 3.85 | 1.03 | 0.265 [0.259, 0.285] | 1.003 [0.966, 1.019] | -73.5% |
| walk_i32.npy | speed | compress | 4.48 | 3.95 | 0.874 [0.869, 0.880] | 0.997 [0.993, 1.003] | -12.6% |
| walk_i32.npy | balanced | decode | 4.20 | 3.21 | 0.764 [0.761, 0.771] | 0.993 [0.990, 1.001] | -23.6% |
| walk_i32.npy | balanced | compress | 26.38 | 26.41 | 1.000 [0.995, 1.004] | 0.999 [0.995, 1.006] | noise |
| walk_i32.npy | eco | decode | 10.73 | 9.73 | 0.906 [0.904, 0.914] | 1.000 [0.998, 1.005] | -9.4% |
| walk_i32.npy | hc | decode | 3.54 | 2.56 | 0.724 [0.721, 0.724] | 0.999 [0.997, 1.002] | -27.6% |
| walk_i32.npy | ratio | decode | 10.74 | 9.72 | 0.906 [0.905, 0.907] | 1.001 [0.999, 1.002] | -9.4% |
| owid_energy_f64.npy | speed | decode | 1.83 | 1.87 | 1.021 [1.019, 1.022] | 1.001 [0.999, 1.001] | +2.1% |
| owid_energy_f64.npy | speed | compress | 10.02 | 10.01 | 1.000 [0.996, 1.002] | 1.004 [1.000, 1.007] | noise |
| owid_energy_f64.npy | balanced | decode | 7.69 | 6.60 | 0.861 [0.850, 0.865] | 0.999 [0.992, 1.004] | -13.9% |
| owid_energy_f64.npy | balanced | compress | 25.35 | 25.16 | 0.992 [0.989, 0.996] | 1.000 [0.995, 1.004] | noise |
| owid_energy_f64.npy | eco | decode | 17.93 | 17.41 | 0.972 [0.968, 0.973] | 1.000 [0.996, 1.003] | -2.8% |
| owid_energy_f64.npy | hc | decode | 8.05 | 6.55 | 0.811 [0.807, 0.817] | 0.998 [0.993, 1.005] | -18.9% |
| owid_energy_f64.npy | ratio | decode | 92.29 | 90.50 | 0.981 [0.979, 0.982] | 0.998 [0.996, 1.001] | -1.9% |
| dickens2m | -x | compress | 5234.67 | 5233.46 | 0.995 [0.982, 1.004] | 1.000 [0.987, 1.007] | noise |
| dickens2m | -x | decode | 3367.09 | 3347.84 | 0.995 [0.975, 1.002] | 0.991 [0.980, 1.004] | noise |

`-m speed` decode with the aligned LZ4 (`aac4b60`) against base (unaligned):

| input | mode | direction | base ms | new ms | new/base [IQR] | A/A [IQR] | change |
|---|---|---|---:|---:|---:|---:|---:|
| dickens | speed | decode | 2.02 | 2.02 | 1.001 [0.999, 1.003] | 1.001 [1.000, 1.003] | noise |
| owid-co2-data.csv | speed | decode | 3.02 | 3.02 | 0.999 [0.997, 0.999] | 0.999 [0.998, 1.001] | noise |
| loghub_all.log | speed | decode | 0.41 | 0.41 | 1.007 [0.996, 1.011] | 1.010 [0.994, 1.022] | noise |
| api.json | speed | decode | 0.37 | 0.37 | 1.003 [0.996, 1.018] | 1.011 [0.995, 1.026] | noise |
| rgb8.ppm | speed | decode | 0.07 | 0.07 | 1.014 [1.000, 1.014] | 1.000 [1.000, 1.000] | noise |
| clip.y4m | speed | decode | 0.95 | 1.00 | 1.047 [1.041, 1.051] | 1.006 [0.999, 1.008] | +4.7% |
| x-ray | speed | decode | 0.41 | 0.43 | 1.046 [1.035, 1.050] | 1.002 [0.996, 1.010] | +4.6% |
| img16.pgm | speed | decode | 0.07 | 0.07 | 1.000 [1.000, 1.000] | 1.000 [1.000, 1.000] | noise |
| mr | speed | decode | 2.78 | 1.24 | 0.443 [0.437, 0.446] | 1.004 [0.991, 1.011] | -55.7% |
| music.wav | speed | decode | 0.11 | 0.11 | 1.000 [1.000, 1.000] | 1.000 [1.000, 1.000] | noise |
| walk_i32.npy | speed | decode | 3.59 | 0.96 | 0.268 [0.265, 0.270] | 1.004 [0.996, 1.012] | -73.2% |
| owid_energy_f64.npy | speed | decode | 1.83 | 1.84 | 1.002 [1.000, 1.005] | 1.002 [1.000, 1.003] | noise |

## What did not gain, and why

- **FEAT_CSSC in the IMG16 median chain.** The M5 has CSSC (scalar UMIN/UMAX/SMIN/SMAX). A target-attribute
  version of the decode loop, chosen by `sysctl hw.optional.arm.FEAT_CSSC`, was 7–13% *slower* than the
  compare-and-select loop (mr 1.074, img16.pgm 1.128): on this core the CSSC min/max do not shorten the chain. The
  4-operation reformulation above, which needs no CSSC, gives −25% instead. The CSSC code is not kept.
- **Word-transform encoder.** Three changes were tried: run ends found with a 16-byte vector compare instead of
  the byte loop; the hash-collision check done with the first byte and `memcmp`; and the taken slots collected
  and sorted instead of scanning the 1M-slot table. Together they made `-m ratio` / `-m eco` compression 4–10%
  slower (api.json 1.056 / 1.076, dickens 1.103 / 1.047). Words and gaps are short, and the vector compare puts a
  load → compare → move → count chain where the scalar loop had a well-predicted branch. The encoder's real cost
  is the hash probe into a 24 MB table. Not kept.
- **CM mixer (`-x`).** clang already vectorises the dot products and the weight updates (224 `SMLAL` in
  `cm_compress_m`). The mixer is about 8% of `-x` time; 48% is the first touch of a context bucket (`cm_find`,
  cache misses). Software prefetch of the next byte's buckets is the lever, and it is item 3 of the xmode lane.
- **Kolmo's rANS leaf (`rans_decompress_all`).** It is 72–75% of int32 decode in eco/ratio/-x and 48% of IMG8
  eco/ratio decode. The format has four interleaved 64-bit states with 32-bit renormalisation, and NEON has no
  64-bit lane multiply. Four states are also too few to hide the latency (see krans). The decoder is
  table-in-L1 (a 64 KiB slot → symbol table) and branch-predicted, about 2 cycles per symbol. It stays scalar.
- **krans NEON decoder (research codec), measured on the M5.** krans `-b` on dickens, decode MB/s, scalar vs
  NEON. Order 0: 1510 vs 652 at 8 ways, 953 vs 1160 at 16, 1300 vs 1660 at 32. Order 1: 1018 vs 544, 865 vs 437,
  880 vs 334. Per round, a group of 4 lanes stores the states and reloads them as scalars to address the tables
  (no gather), assembles the loaded entries into a vector again, forms the renormalisation mask with a horizontal
  ADDV, loads the shuffle row that the mask selects, and counts the words read with a population count, which
  ARMv8.0 has only in the vector unit. That is roughly 50 cycles of latency per round. With 8 ways there are two
  such chains per round. The scalar decoder runs 8 chains of about 11 cycles at once and wins 2.3×. At 32 ways
  there are 8 vector chains to overlap, and NEON wins 1.28×. Three changes were tried. Lane loads straight into
  the vector: no change. A popcount table: 8 ways no better, 32 ways 1.38×. Building the vector from two 64-bit
  register pairs: worse. Order 1 adds a per-lane context lookup through memory to every step, so it loses at every
  width. Conclusion: on the M5, NEON rANS needs at least 16 interleaved states, and Kolmo's own leaf has 4.
- **Table encode, log encode and decode.** These are already SWAR/NEON-assisted, and their time is spread over
  many lines (parsing, template emission, short copies). No single loop is above about 7%.
- **Histograms (`kg_hist`, `h0`).** These are scatter increments; NEON has no scatter. The 4-table scalar form is
  kept.
- **Vendored code** (zstd's Huffman/FSE and match finders, LZ4, Brotli, liblzma, libbsc's QLFC, libsais's inverse
  BWT) belongs to the deps and bwt lanes and was not touched.

## Reproduce

```
python3 -I benchmark/rc3/simd-arm/mk_inputs.py DATA                       # WAV, Y4M, PGM, PPM, NPY
cc -O2 -Isrc benchmark/rc3/simd-arm/proploop.c build/libkolmo.a build/libvendor.a -o proploop $(LIBS)
python3 -I benchmark/rc3/simd-arm/prof.py proploop OUT 3 name=path ...    # flat profiles with `sample`
PROPLOOP_PCS=pcs.txt proploop-g balanced d 10 FILE && python3 -I benchmark/rc3/simd-arm/pcsym.py proploop-g pcs.txt
python3 -I benchmark/rc3/simd-arm/ab.py proploop WORK 15 0.3 balanced:d:FILE ...          # kernels vs C references
bash benchmark/rc3/simd-arm/e2e.sh NEW_PROPLOOP BASE_PROPLOOP WORK DATA SYNTH out.jsonl  # against the branch point
python3 -I benchmark/rc3/simd-arm/ab_kvid.py build/kvid 15 rt clip.y4m
make check-arch; make NO_ASM=1 ...; python3 benchmark/rc3/structure/compare_frames.py NOASM_KOLMO KOLMO WORK
```
