# Routing fixes: per-type, per-level results

Item 3 of the 40-reviewer synthesis (`review40b/SYNTHESIS.md`): encoder-only routing fixes. Format unchanged: every frame
below is container version 12 and decodes with the 0.0.1 and 0.0.2 release binaries (section Compatibility).

## Method

- **Base:** `origin/main` at cda7bf4b. One commit per fix, stacked in the order below. Each commit was built from clean,
  and each fix is measured against the commit before it.
- **Corpora:** the screening (`corpus`), held-out (`corpus-holdout`) and domain (`corpus-domain`) corpora of the review
  workspace, deduplicated by SHA-256: 48 files, 791 MB. Every level 1-9 and ultra, `-T 1`.
- **Per-fix attribution on the corpora:** a full sweep of the final build against the base. Then each commit's build
  ran on the 10 (file, level) pairs whose frame changed. Every other pair is byte-identical in the base and the
  final build, so it is byte-identical at every commit.
- **Class files:** each fix's data class, from the reviewer's reproducer files (paths in each section), at every level, `-T 1`.
- **Thread counts:** frames are identical at `-T 1` and `-T 10`. The determinism check also confirms 1 and 4 threads
  for its settings. Sizes therefore hold for both. Times are given for both in the timing table.
- **Timing:** best of 3 runs of encode and of decode, `-T 1` and `-T 10`, run under the shared timing lock (`timed.sh`)
  on an Apple M5 (10 cores). Other sessions' untimed work was running (load 6-9), so treat small differences as noise.
  The CPU columns of the size tables come from runs that were not timed (machine load up to 130).

## Summary

| fix | class gain (best level) | corpora (48 files, every level) |
|---|---|---|
| #76 log vs table | BGL 10 MB -3 -55.7%, -9 -35.0%, -U -13.4% | no frame changed |
| #144 record spread guard | GHCN Arrow IPC -9 -39.5%, ORC -3 -34.0%, Parquet -9 -7.7%, -U -13.0% | no frame changed |
| #148 KLZH2 literal floor | kennedy.xls -7 -47.5%, Canterbury -6/-7 -10.5%/-10.8% | -7: 2 files, -0.001% |
| #149 level 8 coder gate | (corpora) | -8 -2.23%: mozilla -9.8%, bf16 -10.9%, sao -5.5%, loghub -4.7% |
| #64 float gate | fp16 weights -12% (-3) to -14% (-5..-7) | no frame changed |
| #61 UTF-8 text | Cyrillic prose -5 -24%, pageviews -3 -14.8%, ultra -2.5% | no frame changed |
| #60 spread text decision | text-headed executables -9 -11.6%/-12.8%, -U -11%/-13% | no frame changed |
| #141 DEC64 vs records | NCEP air -3 -21.8% | no frame changed |
| #62 wide-table veto | wide3.csv -8/-9 -21.8%, -U -9.2% | no frame changed |
| #83 long-row tables | 200-column CSV -37% to -63% | no frame changed |
| #82 small executables | 75 x86-64 + 75 arm64 tools -2.1% to -2.4% at -3..-U | no frame changed |

Final build against base over the corpora: every level but 7 and 8 unchanged, -7 -0.001%, -8 -2.23%. The CI size ledger
(`benchmark/ledger/sizes.tsv`, the tests/v12 originals) changes in 14 entries, all smaller. Per file: `corpus_sizes.csv`.

## Log template against the space table (#76, S3 bug 17)

Files: src/plan/planner.c.

A log whose 64 KiB head splits into space-separated fields was planned as a table, and the log template trial ran only when no table was found. For space-delimited tables the template and the table transform are now scored on the same slice; the template replaces the table when it is at least 10% smaller.

Class files: R12's Loghub set (16 logs of 2k lines, BGL 10 MB, BGL with corrupted lines, NASA access log 6 MB).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 19 | 0 | 5,622,798 | 5,622,798 | +0.000% | 0.1 | 0.1 |
| 2 | 19 | 0 | 3,105,319 | 3,105,319 | +0.000% | 0.1 | 0.1 |
| 3 | 19 | 2 | 2,383,857 | 1,623,758 | -31.885% | 0.2 | 0.2 |
| 4 | 19 | 2 | 2,104,467 | 1,442,969 | -31.433% | 0.3 | 0.3 |
| 5 | 19 | 2 | 1,681,650 | 1,271,645 | -24.381% | 1.2 | 0.8 |
| 6 | 19 | 2 | 1,568,849 | 1,192,837 | -23.967% | 2.1 | 1.4 |
| 7 | 19 | 2 | 1,526,308 | 1,159,062 | -24.061% | 4.7 | 3.2 |
| 8 | 19 | 2 | 1,585,268 | 1,084,822 | -31.569% | 0.8 | 0.7 |
| 9 | 19 | 2 | 1,238,085 | 1,020,377 | -17.584% | 4.4 | 3.2 |
| U | 19 | 2 | 800,592 | 740,661 | -7.486% | 40.9 | 28.0 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| bgl_corr.log | 3 | 666,037 | 336,008 | -49.55% | 0.03 | 0.03 |
| bgl_corr.log | 4 | 599,167 | 313,874 | -47.61% | 0.05 | 0.04 |
| bgl_corr.log | 5 | 402,580 | 256,757 | -36.22% | 0.34 | 0.15 |
| bgl_corr.log | 6 | 396,494 | 244,536 | -38.33% | 0.51 | 0.23 |
| bgl_corr.log | 7 | 369,506 | 245,152 | -33.65% | 1.18 | 0.57 |
| bgl_corr.log | 8 | 489,324 | 251,706 | -48.56% | 0.12 | 0.08 |
| bgl_corr.log | 9 | 316,209 | 217,043 | -31.36% | 0.95 | 0.41 |
| bgl_corr.log | U | 216,509 | 186,266 | -13.97% | 9.55 | 4.29 |
| data2_BGL10M.log | 3 | 771,651 | 341,581 | -55.73% | 0.04 | 0.04 |
| data2_BGL10M.log | 4 | 691,908 | 315,703 | -54.37% | 0.09 | 0.05 |
| data2_BGL10M.log | 5 | 522,791 | 258,609 | -50.53% | 0.42 | 0.22 |
| data2_BGL10M.log | 6 | 476,590 | 252,536 | -47.01% | 0.74 | 0.35 |
| data2_BGL10M.log | 7 | 488,449 | 245,557 | -49.73% | 1.74 | 0.96 |
| data2_BGL10M.log | 8 | 520,464 | 257,636 | -50.50% | 0.17 | 0.11 |
| data2_BGL10M.log | 9 | 338,429 | 219,887 | -35.03% | 1.21 | 0.61 |
| data2_BGL10M.log | U | 221,726 | 192,038 | -13.39% | 12.25 | 6.03 |

## Record layouts confirmed on spread slices (#144, S3 bug 18)

Files: src/plan/planner.c.

A record layout chosen on the middle slice must also beat plain coding by 3% on 7 of 8 slices of up to 256 KiB at the odd sixteenths of the input.

Class files: R34's uncompressed columnar containers (Arrow IPC, ORC, Parquet, SQLite, DuckDB) of the GHCN, OWID, Gaia and Backblaze tables. Three frames got larger. bb.pq.none -8 +14.4% is measured before #149; with #149 on top it is 2,352,392 B, -10.8% against the build without #144. ghcn.ipc.none -4 is +3.5% and -U +3.8%. The class gains at every level from -3 to -U, so the fix is kept at all of them.

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 13 | 0 | 84,252,835 | 84,252,835 | +0.000% | 0.3 | 0.3 |
| 2 | 13 | 0 | 69,561,613 | 69,561,613 | +0.000% | 0.4 | 0.4 |
| 3 | 13 | 1 | 48,764,912 | 48,034,621 | -1.498% | 1.2 | 1.2 |
| 4 | 13 | 3 | 42,595,925 | 42,036,128 | -1.314% | 4.8 | 4.8 |
| 5 | 13 | 3 | 39,914,134 | 39,060,330 | -2.139% | 14.1 | 13.8 |
| 6 | 13 | 3 | 38,958,234 | 37,879,006 | -2.770% | 22.1 | 21.6 |
| 7 | 13 | 3 | 38,760,851 | 37,634,359 | -2.906% | 48.3 | 49.3 |
| 8 | 13 | 3 | 39,062,011 | 38,542,139 | -1.331% | 6.9 | 6.9 |
| 9 | 13 | 3 | 36,527,566 | 35,169,083 | -3.719% | 26.3 | 27.5 |
| U | 13 | 2 | 30,525,751 | 30,264,228 | -0.857% | 180.0 | 137.8 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| bb.pq.none | 4 | 2,919,939 | 2,812,731 | -3.67% | 0.27 | 0.37 |
| bb.pq.none | 5 | 2,881,956 | 2,721,731 | -5.56% | 0.84 | 0.85 |
| bb.pq.none | 6 | 2,830,215 | 2,716,633 | -4.01% | 0.93 | 1.02 |
| bb.pq.none | 7 | 2,821,608 | 2,719,419 | -3.62% | 1.50 | 1.19 |
| bb.pq.none | 8 | 2,637,150 | 3,017,354 | +14.42% | 0.37 | 0.43 |
| bb.pq.none | 9 | 2,527,917 | 2,332,472 | -7.73% | 1.42 | 1.51 |
| bb.pq.none | U | 2,271,739 | 1,975,429 | -13.04% | 8.28 | 7.30 |
| ghcn.ipc.none | 4 | 3,200,337 | 3,311,133 | +3.46% | 0.38 | 0.30 |
| ghcn.ipc.none | 5 | 2,752,088 | 2,624,751 | -4.63% | 1.78 | 1.82 |
| ghcn.ipc.none | 6 | 2,580,884 | 2,129,557 | -17.49% | 3.11 | 3.19 |
| ghcn.ipc.none | 7 | 2,536,554 | 2,052,657 | -19.08% | 7.39 | 9.58 |
| ghcn.ipc.none | 8 | 2,089,712 | 1,572,298 | -24.76% | 0.68 | 0.54 |
| ghcn.ipc.none | 9 | 1,756,364 | 1,062,305 | -39.52% | 5.82 | 6.81 |
| ghcn.ipc.none | U | 909,323 | 944,110 | +3.83% | 68.97 | 40.43 |
| ghcn.orc.none | 3 | 2,148,728 | 1,418,437 | -33.99% | 0.07 | 0.06 |
| ghcn.orc.none | 4 | 1,822,047 | 1,258,662 | -30.92% | 0.20 | 0.16 |
| ghcn.orc.none | 5 | 1,705,311 | 1,139,069 | -33.20% | 0.72 | 0.64 |
| ghcn.orc.none | 6 | 1,557,903 | 1,043,584 | -33.01% | 1.32 | 1.10 |
| ghcn.orc.none | 7 | 1,529,029 | 988,623 | -35.34% | 3.59 | 3.93 |
| ghcn.orc.none | 8 | 1,342,322 | 959,660 | -28.51% | 0.30 | 0.30 |
| ghcn.orc.none | 9 | 1,342,322 | 873,343 | -34.94% | 2.80 | 2.67 |

## KLZH2 literal floor chosen per input (#148, S3 bug 19)

Files: src/algo/klz.c.

Two warm-up passes of the optimal parse on the first 256 KiB, with a literal floor of 1 and of 2 bits. Where the 2-bit pass estimates at least 2% smaller, its statistics seed the real pass, which keeps the 1-bit floor; otherwise the parse starts exactly as before. The comment that said a 2-bit floor was in force is corrected. The first version always seeded from the cheaper warm-up and cost 16-bit images up to 2%, raw video 0.3% and an arm64 executable 0.2% (found by the CI size ledger); the 2% margin and the untouched default removed every such case.

Class files: the Canterbury corpus (R37). Encode cost: the two warm-up passes run on every KLZH2 optimal-parse block of 16 KiB or more, about 2x parse time on inputs under 256 KiB and a few percent on large ones (timing table).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 2 | -1,782 B (-0.001%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

| file | level | before B | after B | change |
|---|---|---|---|---|
| f64_owid_energy | 7 | 1,395,034 | 1,394,143 | -0.06% |
| owid_energy_f64.npy | 7 | 1,395,125 | 1,394,234 | -0.06% |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 11 | 0 | 1,142,998 | 1,142,998 | +0.000% | 0.0 | 0.0 |
| 2 | 11 | 0 | 687,594 | 687,594 | +0.000% | 0.0 | 0.0 |
| 3 | 11 | 0 | 630,748 | 630,748 | +0.000% | 0.1 | 0.1 |
| 4 | 11 | 0 | 594,099 | 594,099 | +0.000% | 0.1 | 0.1 |
| 5 | 11 | 1 | 567,447 | 541,570 | -4.560% | 0.2 | 0.4 |
| 6 | 11 | 1 | 593,314 | 530,860 | -10.526% | 0.3 | 0.5 |
| 7 | 11 | 2 | 591,956 | 527,815 | -10.835% | 0.6 | 0.9 |
| 8 | 11 | 0 | 456,006 | 456,006 | +0.000% | 0.1 | 0.2 |
| 9 | 11 | 0 | 429,100 | 429,100 | +0.000% | 0.5 | 0.5 |
| U | 11 | 0 | 351,794 | 351,794 | +0.000% | 4.4 | 4.7 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| kennedy.xls | 5 | 98,009 | 72,132 | -26.40% | 0.04 | 0.07 |
| kennedy.xls | 6 | 132,000 | 69,546 | -47.31% | 0.07 | 0.11 |
| kennedy.xls | 7 | 132,595 | 69,546 | -47.55% | 0.19 | 0.22 |
| ptt5 | 7 | 45,074 | 43,982 | -2.42% | 0.19 | 0.37 |

## Level 8: block sorting only where it wins (#149, S3 bug 24)

Files: src/core/blocks.c, docs/LEVELS.md.

Level 8 coded every block with BWT. Text, JSON-token and log streams keep BWT; other blocks of 64 KiB or more are coded both ways (whole up to 2 MiB, a 512 KiB middle slice above) and keep KLZMA 1 unless BWT is at least 2% smaller. The first version used an eighth of small blocks as the slice and cost a 1.2 MB executable and a 135 KB table 12% (CI size ledger); whole-block trials removed that.

The class is in the corpora (mozilla, bf16 weights, sao, ooffice, loghub, x-ray, energy). Decode of those blocks is several times faster; their encode is slower (timing table).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 8 | -5,065,133 B (-2.225%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

| file | level | before B | after B | change |
|---|---|---|---|---|
| energy_test.csv | 8 | 659,712 | 655,827 | -0.59% |
| llm_bf16_47mb.safetensors | 8 | 30,242,348 | 26,942,578 | -10.91% |
| loghub_all.log | 8 | 271,672 | 258,970 | -4.68% |
| mozilla | 8 | 16,011,130 | 14,447,505 | -9.77% |
| ooffice | 8 | 2,104,638 | 2,056,479 | -2.29% |
| samba | 8 | 3,990,502 | 3,986,902 | -0.09% |
| sao | 8 | 2,332,056 | 2,204,426 | -5.47% |
| x-ray | 8 | 3,627,994 | 3,622,232 | -0.16% |

## Float trial for non-random mantissas (#64, S3 bug 21)

Files: src/plan/planner.c.

The float trial no longer requires the second mantissa plane to look random (order-0 entropy of 7 bits or more). Widths that pass only the relaxed test also run the integer trial, which the float trial otherwise pre-empts.

Class files: R15's synthetic weights (fp16 from bf16 as NPY and safetensors, f32 upcast from bf16, big-endian and fp8 variants, a float32 grid).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 8 | 0 | 71,447,219 | 71,447,219 | +0.000% | 0.1 | 0.1 |
| 2 | 8 | 0 | 59,524,411 | 59,524,411 | +0.000% | 0.2 | 0.2 |
| 3 | 8 | 3 | 55,860,008 | 53,418,815 | -4.370% | 0.3 | 0.4 |
| 4 | 8 | 3 | 55,227,602 | 52,737,565 | -4.509% | 7.2 | 7.1 |
| 5 | 8 | 3 | 54,925,950 | 52,396,183 | -4.606% | 19.9 | 18.2 |
| 6 | 8 | 3 | 54,916,392 | 52,461,443 | -4.470% | 21.5 | 13.7 |
| 7 | 8 | 3 | 54,971,315 | 52,461,445 | -4.566% | 18.6 | 12.2 |
| 8 | 8 | 3 | 52,850,941 | 52,508,029 | -0.649% | 12.1 | 20.2 |
| 9 | 8 | 3 | 52,663,446 | 52,469,849 | -0.368% | 20.2 | 37.0 |
| U | 8 | 0 | 52,025,417 | 52,025,417 | +0.000% | 186.4 | 169.8 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| w_f16.npy | 3 | 8,127,021 | 7,132,928 | -12.23% | 0.02 | 0.05 |
| w_f16.npy | 4 | 8,085,291 | 6,962,657 | -13.88% | 1.30 | 1.28 |
| w_f16.npy | 5 | 8,096,822 | 6,956,101 | -14.09% | 3.87 | 3.44 |
| w_f16.npy | 6 | 8,104,606 | 6,962,657 | -14.09% | 3.98 | 1.56 |
| w_f16.npy | 7 | 8,104,606 | 6,962,657 | -14.09% | 3.12 | 1.55 |
| w_f16.npy | 8 | 7,029,610 | 6,956,131 | -1.05% | 1.03 | 4.45 |
| w_f16.npy | 9 | 7,029,610 | 6,956,131 | -1.05% | 1.04 | 5.04 |
| w_f16.safetensors | 3 | 8,126,902 | 7,126,465 | -12.31% | 0.02 | 0.06 |
| w_f16.safetensors | 4 | 8,084,979 | 6,966,239 | -13.84% | 1.39 | 0.80 |
| w_f16.safetensors | 5 | 8,096,651 | 6,958,694 | -14.05% | 4.04 | 1.91 |
| w_f16.safetensors | 6 | 8,104,021 | 6,966,239 | -14.04% | 4.14 | 0.92 |
| w_f16.safetensors | 7 | 8,104,021 | 6,966,239 | -14.04% | 3.34 | 0.97 |
| w_f16.safetensors | 8 | 7,029,592 | 6,958,694 | -1.01% | 1.06 | 4.45 |
| w_f16.safetensors | 9 | 7,029,592 | 6,958,694 | -1.01% | 1.06 | 4.98 |
| w_f32up.npy | 3 | 7,444,582 | 6,997,919 | -6.00% | 0.08 | 0.11 |
| w_f32up.npy | 4 | 7,246,042 | 6,997,379 | -3.43% | 0.76 | 1.06 |
| w_f32up.npy | 5 | 7,216,838 | 6,965,749 | -3.48% | 2.21 | 2.72 |
| w_f32up.npy | 6 | 7,172,597 | 6,997,379 | -2.44% | 3.36 | 0.95 |
| w_f32up.npy | 7 | 7,227,518 | 6,997,379 | -3.18% | 3.57 | 0.77 |
| w_f32up.npy | 8 | 7,164,237 | 6,965,702 | -2.77% | 2.09 | 3.51 |
| w_f32up.npy | 9 | 7,014,922 | 6,965,702 | -0.70% | 5.51 | 14.32 |

## UTF-8 text (#61, S3 bug 23)

Files: src/plan/detect.c, src/plan/planner.c, src/transforms/word.c, src/core/blocks.c.

looks_text counts UTF-8 continuation bytes (a lead byte with its continuations); the planner's head test counts whole UTF-8 sequences at levels 3-7; the word transform tokenizes bytes >= 0x80 as letters in text with 5% or more such bytes, and is kept there only when the probe saves 15%. Max (levels 8-9) keeps the ASCII head test and the old text test for its block-sorting rules, so its UTF-8 frames are unchanged; ultra gets the UTF-8-aware context-mixing text mask.

Class files: real non-Latin UTF-8 found on this Mac (Apple dictionary XML in Arabic, Greek, Hebrew, Hindi, Japanese, Russian, Thai, Ukrainian; first 20 MB of a Wikimedia pageviews dump), R04's and R17's letter-mapped Cyrillic dickens, and R17's ASCII control. Measured first without the 15% gate and the max routing: the XML files lost 2-7% at -6/-7 and pageviews 0.9% at -8/-9; those routes were removed and every frame below is from the final code. No frame of the class is larger.

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 12 | 0 | 18,204,139 | 18,204,139 | +0.000% | 0.1 | 0.1 |
| 2 | 12 | 0 | 13,941,309 | 13,941,309 | +0.000% | 0.2 | 0.2 |
| 3 | 12 | 1 | 12,608,499 | 11,880,393 | -5.775% | 0.3 | 0.3 |
| 4 | 12 | 4 | 11,458,453 | 9,421,428 | -17.777% | 0.9 | 1.2 |
| 5 | 12 | 4 | 10,680,876 | 8,891,798 | -16.750% | 5.9 | 4.7 |
| 6 | 12 | 4 | 9,155,490 | 8,357,617 | -8.715% | 14.7 | 8.4 |
| 7 | 12 | 4 | 9,034,244 | 8,288,027 | -8.260% | 19.3 | 11.0 |
| 8 | 12 | 0 | 6,922,250 | 6,922,250 | +0.000% | 2.2 | 2.3 |
| 9 | 12 | 0 | 6,922,250 | 6,922,250 | +0.000% | 4.7 | 5.0 |
| U | 12 | 11 | 6,040,171 | 5,890,426 | -2.479% | 57.8 | 65.5 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| apple_ar.xml | U | 29,896 | 29,114 | -2.62% | 0.47 | 0.49 |
| apple_el.xml | U | 31,013 | 30,190 | -2.65% | 0.51 | 0.54 |
| apple_he.xml | U | 29,885 | 29,139 | -2.50% | 0.44 | 0.47 |
| apple_hi.xml | 4 | 62,246 | 53,845 | -13.50% | 0.01 | 0.02 |
| apple_hi.xml | 5 | 56,714 | 50,940 | -10.18% | 0.07 | 0.07 |
| apple_hi.xml | 6 | 50,271 | 49,280 | -1.97% | 0.12 | 0.12 |
| apple_hi.xml | 7 | 49,746 | 49,378 | -0.74% | 0.19 | 0.10 |
| apple_hi.xml | U | 30,424 | 29,557 | -2.85% | 0.60 | 0.57 |
| apple_ja.xml | U | 29,171 | 28,458 | -2.44% | 0.45 | 0.47 |
| apple_ru.xml | U | 33,371 | 32,482 | -2.66% | 0.51 | 0.55 |
| apple_th.xml | U | 31,045 | 29,702 | -4.33% | 0.61 | 0.61 |
| apple_uk.xml | U | 31,790 | 31,084 | -2.22% | 0.50 | 0.54 |
| d3.cyr | 4 | 1,282,468 | 974,102 | -24.04% | 0.09 | 0.15 |
| d3.cyr | 5 | 1,179,001 | 922,921 | -21.72% | 0.68 | 0.43 |
| d3.cyr | 6 | 948,426 | 875,362 | -7.70% | 1.78 | 0.67 |
| d3.cyr | 7 | 945,067 | 874,983 | -7.42% | 1.76 | 0.70 |
| d3.cyr | U | 689,034 | 666,524 | -3.27% | 6.54 | 7.87 |
| dickens_cyr.txt | 4 | 4,375,076 | 3,243,778 | -25.86% | 0.26 | 0.42 |
| dickens_cyr.txt | 5 | 4,015,057 | 3,051,342 | -24.00% | 2.19 | 1.66 |
| dickens_cyr.txt | 6 | 3,129,101 | 2,808,223 | -10.25% | 6.78 | 3.00 |
| dickens_cyr.txt | 7 | 3,089,012 | 2,804,407 | -9.21% | 7.20 | 3.10 |
| dickens_cyr.txt | U | 2,231,337 | 2,153,879 | -3.47% | 20.84 | 22.55 |
| pageviews20m | 3 | 4,926,566 | 4,198,460 | -14.78% | 0.11 | 0.14 |
| pageviews20m | 4 | 4,404,551 | 3,815,591 | -13.37% | 0.36 | 0.34 |
| pageviews20m | 5 | 4,164,473 | 3,600,964 | -13.53% | 2.17 | 1.58 |
| pageviews20m | 6 | 3,824,353 | 3,421,413 | -10.54% | 4.73 | 3.19 |
| pageviews20m | 7 | 3,750,239 | 3,359,079 | -10.43% | 8.45 | 5.13 |
| pageviews20m | U | 2,228,899 | 2,185,991 | -1.93% | 21.62 | 25.56 |

## Text decided on spread slices (#60, S3 bug 22)

Files: src/plan/planner.c.

For inputs of 256 KiB or more whose 64 KiB head is text but whose four spread 16 KiB slices are not (looks_text), the planner takes the binary route when the middle 1 MiB holds x86 call/jump opcodes at the density of the x86 trial. The first version took the binary route for any such mixture and cost a tar of text, a table, an int32 series and a log 63-78% at levels 1-7 (CI size ledger: a shuffle chosen on the series was applied to everything); it is now limited to machine code.

Class files: R04's text-headed executables (512 KiB dickens + ooffice, 128 KiB text + 1 MiB ooffice, a second mix, a 1 MiB executable) and three made here (dickens + mozilla, text + sao, xml + magika.onnx; the last is not x86 and keeps the text route).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 7 | 0 | 13,606,646 | 13,606,646 | +0.000% | 0.1 | 0.1 |
| 2 | 7 | 0 | 12,117,125 | 12,117,125 | +0.000% | 0.1 | 0.1 |
| 3 | 7 | 2 | 11,137,533 | 10,789,327 | -3.126% | 0.2 | 0.2 |
| 4 | 7 | 2 | 10,835,830 | 10,394,251 | -4.075% | 1.0 | 1.0 |
| 5 | 7 | 2 | 10,527,357 | 10,099,935 | -4.060% | 3.0 | 2.8 |
| 6 | 7 | 2 | 10,361,941 | 9,945,676 | -4.017% | 3.8 | 3.7 |
| 7 | 7 | 2 | 10,356,516 | 9,939,598 | -4.026% | 3.9 | 3.9 |
| 8 | 7 | 2 | 9,948,928 | 9,458,010 | -4.934% | 3.2 | 2.9 |
| 9 | 7 | 2 | 9,888,164 | 9,494,566 | -3.980% | 5.6 | 5.8 |
| U | 7 | 2 | 8,664,690 | 8,348,847 | -3.645% | 26.0 | 26.8 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| big.bin | 3 | 3,222,333 | 2,936,281 | -8.88% | 0.06 | 0.07 |
| big.bin | 4 | 3,072,211 | 2,696,883 | -12.22% | 0.39 | 0.37 |
| big.bin | 5 | 2,895,718 | 2,533,044 | -12.52% | 1.38 | 1.25 |
| big.bin | 6 | 2,801,312 | 2,447,836 | -12.62% | 1.85 | 1.78 |
| big.bin | 7 | 2,799,280 | 2,445,202 | -12.65% | 1.79 | 1.79 |
| big.bin | 8 | 2,713,738 | 2,287,998 | -15.69% | 0.96 | 0.42 |
| big.bin | 9 | 2,602,365 | 2,268,325 | -12.84% | 2.20 | 1.99 |
| big.bin | U | 2,073,065 | 1,804,716 | -12.94% | 8.61 | 8.94 |
| mix.bin | 3 | 615,130 | 552,976 | -10.10% | 0.02 | 0.02 |
| mix.bin | 4 | 590,520 | 524,269 | -11.22% | 0.04 | 0.04 |
| mix.bin | 5 | 559,184 | 494,436 | -11.58% | 0.16 | 0.17 |
| mix.bin | 6 | 547,255 | 484,466 | -11.47% | 0.23 | 0.22 |
| mix.bin | 7 | 547,127 | 484,287 | -11.49% | 0.22 | 0.24 |
| mix.bin | 8 | 500,710 | 435,532 | -13.02% | 0.27 | 0.47 |
| mix.bin | 9 | 515,204 | 455,646 | -11.56% | 0.34 | 0.64 |
| mix.bin | U | 422,359 | 374,865 | -11.24% | 1.83 | 1.94 |

## DEC64 no longer skips the record trial (#141, S3 bug 20)

Files: src/plan/planner.c.

At levels 3-4 (balanced trials), a typed plan that saves a fifth skipped the record search. A DEC64 plan no longer does: DEC64 scores well on a slice of exact decimals even where the input interleaves them with computed values, and the record layout is far smaller on the whole file. DEC64 keeps its 7% margin over the record layout.

Class files: R33's NCEP reanalysis grids (air, rhum, slp, uwnd as raw float32, air as NetCDF, air in 8 and 64 time-slice chunks, slp as FITS). Only levels 3 change. The reviewer's -15% at -5 was a split oracle that needs exact and computed slices coded apart inside one graph (a graph change, not done).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 8 | 0 | 166,290,679 | 166,290,679 | +0.000% | 0.9 | 0.9 |
| 2 | 8 | 0 | 131,324,222 | 131,324,222 | +0.000% | 1.2 | 1.2 |
| 3 | 8 | 2 | 84,731,029 | 80,691,166 | -4.768% | 2.5 | 2.5 |
| 4 | 8 | 0 | 76,263,911 | 76,263,911 | +0.000% | 13.1 | 14.2 |
| 5 | 8 | 0 | 73,402,559 | 73,402,559 | +0.000% | 49.0 | 49.5 |
| 6 | 8 | 0 | 72,529,704 | 72,529,704 | +0.000% | 80.4 | 79.4 |
| 7 | 8 | 0 | 72,270,598 | 72,270,598 | +0.000% | 109.4 | 111.4 |
| 8 | 8 | 0 | 59,605,035 | 59,605,035 | +0.000% | 27.1 | 27.3 |
| 9 | 8 | 0 | 58,919,361 | 58,919,361 | +0.000% | 81.2 | 79.5 |
| U | 8 | 0 | 56,734,617 | 56,734,617 | +0.000% | 663.1 | 658.3 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| air.raw | 3 | 17,536,403 | 13,709,800 | -21.82% | 0.49 | 0.38 |
| air_c64.bin | 3 | 831,241 | 617,981 | -25.66% | 0.04 | 0.05 |

## Wide-table veto checked on 2 MiB (#62, S3 bug 25)

Files: src/plan/planner.c.

At levels 8, 9 and ultra the wide-table graph (over 256 columns) had to beat BWT by 3% on a 384 KiB run. A veto there is now checked on a 2 MiB run from the same place, and the graph is kept if it wins by 3% on that.

Class files: R04's wide3.csv, wide_test.csv, and 2- and 5-rows-per-line variants of it made here. -9 is now below -7 on wide3.csv (1,080,366 against 1,097,934 B).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 4 | 0 | 12,437,510 | 12,437,510 | +0.000% | 0.1 | 0.1 |
| 2 | 4 | 0 | 8,152,559 | 8,152,559 | +0.000% | 0.1 | 0.1 |
| 3 | 4 | 0 | 6,646,470 | 6,646,470 | +0.000% | 0.2 | 0.2 |
| 4 | 4 | 0 | 4,982,378 | 4,982,378 | +0.000% | 0.6 | 0.5 |
| 5 | 4 | 0 | 4,815,090 | 4,815,090 | +0.000% | 1.7 | 1.7 |
| 6 | 4 | 0 | 4,709,004 | 4,709,004 | +0.000% | 3.6 | 3.5 |
| 7 | 4 | 0 | 4,663,594 | 4,663,594 | +0.000% | 14.5 | 14.0 |
| 8 | 4 | 1 | 4,732,695 | 4,431,727 | -6.359% | 0.9 | 0.9 |
| 9 | 4 | 1 | 4,732,695 | 4,431,727 | -6.359% | 0.9 | 0.9 |
| U | 4 | 1 | 4,374,427 | 4,264,549 | -2.512% | 19.4 | 9.6 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| wide3.csv | 8 | 1,381,334 | 1,080,366 | -21.79% | 0.28 | 0.30 |
| wide3.csv | 9 | 1,381,334 | 1,080,366 | -21.79% | 0.26 | 0.29 |
| wide3.csv | U | 1,197,135 | 1,087,257 | -9.18% | 9.49 | 0.34 |

## Long-row table detection (#83)

Files: src/plan/planner.c.

When fewer than 51 lines fit in the 64 KiB head, the field counts are taken again on up to 1 MiB, and the table trial uses that head.

Class files: R09's CSV robustness set (16 files, among them 200-column and 40-column random walks of the same data).

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 16 | 0 | 20,868,484 | 20,868,484 | +0.000% | 0.1 | 0.2 |
| 2 | 16 | 0 | 13,525,473 | 13,525,473 | +0.000% | 0.2 | 0.2 |
| 3 | 16 | 1 | 4,991,081 | 4,315,549 | -13.535% | 0.3 | 0.5 |
| 4 | 16 | 1 | 5,814,125 | 4,114,683 | -29.230% | 0.8 | 0.9 |
| 5 | 16 | 1 | 4,707,136 | 4,099,678 | -12.905% | 2.4 | 1.5 |
| 6 | 16 | 1 | 5,012,077 | 4,063,459 | -18.927% | 5.6 | 2.0 |
| 7 | 16 | 1 | 4,970,339 | 4,023,485 | -19.050% | 11.6 | 8.8 |
| 8 | 16 | 1 | 5,371,944 | 3,971,017 | -26.079% | 1.3 | 1.5 |
| 9 | 16 | 1 | 5,313,363 | 3,912,449 | -26.366% | 4.2 | 4.5 |
| U | 16 | 1 | 3,973,545 | 3,816,517 | -3.952% | 48.3 | 42.8 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| w200.csv | 3 | 1,723,993 | 1,048,461 | -39.18% | 0.05 | 0.22 |
| w200.csv | 4 | 2,715,408 | 1,015,966 | -62.59% | 0.30 | 0.42 |
| w200.csv | 5 | 1,623,169 | 1,015,711 | -37.42% | 1.47 | 0.49 |
| w200.csv | 6 | 1,966,401 | 1,017,783 | -48.24% | 4.06 | 0.49 |
| w200.csv | 7 | 1,964,615 | 1,017,761 | -48.20% | 4.14 | 1.27 |
| w200.csv | 8 | 2,421,842 | 1,020,915 | -57.85% | 0.39 | 0.54 |
| w200.csv | 9 | 2,421,842 | 1,020,928 | -57.84% | 0.40 | 0.65 |
| w200.csv | U | 1,162,338 | 1,005,310 | -13.51% | 13.72 | 7.90 |

## Branch filters for small executables (#82)

Files: src/plan/meta.c, src/plan/planner.c, src/core/compress.c.

The ELF, Mach-O, fat and PE parsers cut executables of 4 KiB or more (was 1 MiB) into code and data, so ARM64 code gets its branch transform; inputs under 256 KiB get the x86 call/jump trial. Level 1, which applies no branch filter, keeps the 1 MiB minimum (the cut alone cost 0.5% there).

Class files: R18's 75 x86-64 and 75 arm64 macOS tools of 32-256 KiB. The reviewer's ARM64 content detector (for headerless ARM64 code) is not done; arm64 tools gain from the parser only.

#### Whole corpora: corpus (48 files, 791 MB)

| level | files changed | corpus total change |
|---|---|---|
| 1 | 0 | +0 B (+0.000%) |
| 2 | 0 | +0 B (+0.000%) |
| 3 | 0 | +0 B (+0.000%) |
| 4 | 0 | +0 B (+0.000%) |
| 5 | 0 | +0 B (+0.000%) |
| 6 | 0 | +0 B (+0.000%) |
| 7 | 0 | +0 B (+0.000%) |
| 8 | 0 | +0 B (+0.000%) |
| 9 | 0 | +0 B (+0.000%) |
| U | 0 | +0 B (+0.000%) |

#### Class files, per level (sizes are frames; CPU is user+sys of -T 1 runs on a loaded machine)

| level | files | changed | total before | total after | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|---|
| 1 | 150 | 0 | 3,680,968 | 3,680,968 | +0.000% | 0.4 | 0.4 |
| 2 | 150 | 4 | 3,035,723 | 3,015,405 | -0.669% | 0.4 | 0.4 |
| 3 | 150 | 33 | 2,898,069 | 2,832,300 | -2.269% | 0.6 | 0.6 |
| 4 | 150 | 33 | 2,838,227 | 2,769,339 | -2.427% | 0.7 | 0.7 |
| 5 | 150 | 33 | 2,722,734 | 2,659,692 | -2.315% | 1.4 | 1.5 |
| 6 | 150 | 33 | 2,636,070 | 2,577,156 | -2.235% | 1.9 | 1.9 |
| 7 | 150 | 33 | 2,635,696 | 2,576,565 | -2.243% | 3.2 | 3.2 |
| 8 | 150 | 33 | 2,468,437 | 2,415,929 | -2.127% | 1.6 | 2.1 |
| 9 | 150 | 33 | 2,372,543 | 2,319,438 | -2.238% | 1.9 | 2.2 |
| U | 150 | 33 | 2,145,538 | 2,100,052 | -2.120% | 13.2 | 13.4 |

#### Class files, every changed frame

| file | level | before B | after B | size | CPU before s | CPU after s |
|---|---|---|---|---|---|---|
| AssetCacheManagerUtil.x64 | 3 | 24,187 | 23,444 | -3.07% | 0.01 | 0.01 |
| AssetCacheManagerUtil.x64 | 4 | 23,358 | 22,577 | -3.34% | 0.01 | 0.01 |
| AssetCacheManagerUtil.x64 | 5 | 22,547 | 21,790 | -3.36% | 0.01 | 0.01 |
| AssetCacheManagerUtil.x64 | 6 | 21,873 | 21,140 | -3.35% | 0.02 | 0.03 |
| AssetCacheManagerUtil.x64 | 7 | 21,859 | 21,126 | -3.35% | 0.03 | 0.04 |
| AssetCacheManagerUtil.x64 | 8 | 20,182 | 19,549 | -3.14% | 0.02 | 0.03 |
| AssetCacheManagerUtil.x64 | 9 | 20,005 | 19,351 | -3.27% | 0.01 | 0.03 |
| AssetCacheManagerUtil.x64 | U | 17,085 | 16,511 | -3.36% | 0.11 | 0.10 |
| SafeEjectGPU.x64 | 3 | 11,682 | 11,255 | -3.66% | 0.01 | 0.00 |
| SafeEjectGPU.x64 | 4 | 11,453 | 10,979 | -4.14% | 0.00 | 0.01 |
| SafeEjectGPU.x64 | 5 | 11,144 | 10,710 | -3.89% | 0.01 | 0.01 |
| SafeEjectGPU.x64 | 6 | 10,821 | 10,414 | -3.76% | 0.01 | 0.01 |
| SafeEjectGPU.x64 | 7 | 10,827 | 10,418 | -3.78% | 0.01 | 0.02 |
| SafeEjectGPU.x64 | 8 | 11,382 | 10,976 | -3.57% | 0.01 | 0.01 |
| SafeEjectGPU.x64 | 9 | 9,951 | 9,603 | -3.50% | 0.01 | 0.01 |
| SafeEjectGPU.x64 | U | 9,125 | 8,832 | -3.21% | 0.06 | 0.05 |
| applesingle.x64 | 3 | 14,656 | 13,916 | -5.05% | 0.00 | 0.00 |
| applesingle.x64 | 4 | 14,311 | 13,582 | -5.09% | 0.00 | 0.01 |
| applesingle.x64 | 5 | 13,819 | 13,133 | -4.96% | 0.01 | 0.01 |
| applesingle.x64 | 6 | 13,488 | 12,833 | -4.86% | 0.01 | 0.01 |
| applesingle.x64 | 7 | 13,480 | 12,823 | -4.87% | 0.01 | 0.02 |
| applesingle.x64 | 8 | 13,728 | 13,118 | -4.44% | 0.00 | 0.01 |
| applesingle.x64 | 9 | 12,497 | 11,892 | -4.84% | 0.01 | 0.01 |
| applesingle.x64 | U | 11,183 | 10,706 | -4.27% | 0.07 | 0.07 |
| archiveutil.x64 | 3 | 23,544 | 22,780 | -3.24% | 0.01 | 0.00 |
| archiveutil.x64 | 4 | 22,474 | 21,596 | -3.91% | 0.01 | 0.01 |
| archiveutil.x64 | 5 | 21,607 | 20,779 | -3.83% | 0.01 | 0.02 |
| archiveutil.x64 | 6 | 20,789 | 19,988 | -3.85% | 0.02 | 0.02 |
| archiveutil.x64 | 7 | 20,783 | 19,971 | -3.91% | 0.03 | 0.02 |
| archiveutil.x64 | 8 | 19,010 | 18,264 | -3.92% | 0.02 | 0.02 |
| archiveutil.x64 | 9 | 18,754 | 18,031 | -3.86% | 0.02 | 0.03 |
| archiveutil.x64 | U | 16,693 | 16,166 | -3.16% | 0.10 | 0.10 |
| atq.x64 | 3 | 15,346 | 14,822 | -3.41% | 0.00 | 0.00 |
| atq.x64 | 4 | 15,150 | 14,572 | -3.82% | 0.00 | 0.00 |
| atq.x64 | 5 | 14,731 | 14,146 | -3.97% | 0.01 | 0.01 |
| atq.x64 | 6 | 14,205 | 13,730 | -3.34% | 0.01 | 0.01 |
| atq.x64 | 7 | 14,233 | 13,725 | -3.57% | 0.02 | 0.02 |
| atq.x64 | 8 | 14,606 | 14,094 | -3.51% | 0.01 | 0.01 |
| atq.x64 | 9 | 12,948 | 12,469 | -3.70% | 0.01 | 0.01 |
| atq.x64 | U | 11,837 | 11,467 | -3.13% | 0.07 | 0.06 |
| auvaltool.a64 | 2 | 78,578 | 71,756 | -8.68% | 0.00 | 0.00 |
| auvaltool.a64 | 3 | 73,966 | 68,312 | -7.64% | 0.01 | 0.01 |
| auvaltool.a64 | 4 | 72,820 | 66,786 | -8.29% | 0.01 | 0.01 |
| auvaltool.a64 | 5 | 68,782 | 63,638 | -7.48% | 0.04 | 0.03 |
| auvaltool.a64 | 6 | 66,597 | 62,066 | -6.80% | 0.04 | 0.04 |
| auvaltool.a64 | 7 | 66,596 | 62,065 | -6.80% | 0.06 | 0.06 |
| auvaltool.a64 | 8 | 58,858 | 55,212 | -6.19% | 0.04 | 0.05 |
| auvaltool.a64 | 9 | 58,159 | 54,303 | -6.63% | 0.03 | 0.03 |
| auvaltool.a64 | U | 52,826 | 48,718 | -7.78% | 0.27 | 0.25 |
| auvaltool.x64 | 2 | 83,827 | 82,245 | -1.89% | 0.00 | 0.00 |
| auvaltool.x64 | 3 | 78,967 | 73,566 | -6.84% | 0.01 | 0.01 |
| auvaltool.x64 | 4 | 76,800 | 71,260 | -7.21% | 0.02 | 0.01 |
| auvaltool.x64 | 5 | 72,972 | 68,074 | -6.71% | 0.04 | 0.04 |
| auvaltool.x64 | 6 | 70,811 | 66,271 | -6.41% | 0.04 | 0.04 |
| auvaltool.x64 | 7 | 70,800 | 66,267 | -6.40% | 0.05 | 0.05 |
| auvaltool.x64 | 8 | 66,504 | 62,273 | -6.36% | 0.04 | 0.06 |
| auvaltool.x64 | 9 | 65,538 | 61,284 | -6.49% | 0.03 | 0.05 |
| auvaltool.x64 | U | 56,443 | 52,779 | -6.49% | 0.24 | 0.23 |
| bc.a64 | 2 | 64,125 | 58,145 | -9.33% | 0.00 | 0.00 |
| bc.a64 | 3 | 60,651 | 55,785 | -8.02% | 0.01 | 0.01 |
| bc.a64 | 4 | 59,706 | 54,452 | -8.80% | 0.02 | 0.01 |
| bc.a64 | 5 | 55,859 | 51,326 | -8.12% | 0.03 | 0.02 |
| bc.a64 | 6 | 53,586 | 49,769 | -7.12% | 0.04 | 0.03 |
| bc.a64 | 7 | 53,581 | 49,755 | -7.14% | 0.05 | 0.06 |
| bc.a64 | 8 | 47,736 | 44,525 | -6.73% | 0.04 | 0.04 |
| bc.a64 | 9 | 46,966 | 43,626 | -7.11% | 0.03 | 0.04 |
| bc.a64 | U | 42,333 | 39,123 | -7.58% | 0.20 | 0.23 |
| bc.x64 | 3 | 63,364 | 59,279 | -6.45% | 0.00 | 0.01 |
| bc.x64 | 4 | 61,488 | 57,280 | -6.84% | 0.01 | 0.01 |
| bc.x64 | 5 | 58,533 | 54,547 | -6.81% | 0.02 | 0.03 |
| bc.x64 | 6 | 57,101 | 53,225 | -6.79% | 0.04 | 0.04 |
| bc.x64 | 7 | 57,107 | 53,217 | -6.81% | 0.04 | 0.05 |
| bc.x64 | 8 | 54,478 | 50,879 | -6.61% | 0.03 | 0.05 |
| bc.x64 | 9 | 53,703 | 50,019 | -6.86% | 0.03 | 0.05 |
| bc.x64 | U | 45,985 | 43,086 | -6.30% | 0.20 | 0.19 |
| binhex.x64 | 3 | 17,677 | 16,870 | -4.57% | 0.00 | 0.00 |
| binhex.x64 | 4 | 17,182 | 16,351 | -4.84% | 0.00 | 0.01 |
| binhex.x64 | 5 | 16,612 | 15,860 | -4.53% | 0.01 | 0.01 |
| binhex.x64 | 6 | 16,143 | 15,426 | -4.44% | 0.01 | 0.01 |
| binhex.x64 | 7 | 16,138 | 15,420 | -4.45% | 0.01 | 0.02 |
| binhex.x64 | 8 | 16,698 | 15,980 | -4.30% | 0.01 | 0.01 |
| binhex.x64 | 9 | 15,061 | 14,358 | -4.67% | 0.01 | 0.01 |
| binhex.x64 | U | 13,509 | 13,051 | -3.39% | 0.06 | 0.08 |
| bsdtar.x64 | 3 | 31,650 | 30,034 | -5.11% | 0.00 | 0.01 |
| bsdtar.x64 | 4 | 30,641 | 28,936 | -5.56% | 0.00 | 0.01 |
| bsdtar.x64 | 5 | 29,091 | 27,470 | -5.57% | 0.02 | 0.02 |
| bsdtar.x64 | 6 | 28,369 | 26,692 | -5.91% | 0.02 | 0.02 |
| bsdtar.x64 | 7 | 28,367 | 26,686 | -5.93% | 0.03 | 0.03 |
| bsdtar.x64 | 8 | 26,028 | 24,660 | -5.26% | 0.02 | 0.03 |
| bsdtar.x64 | 9 | 25,530 | 24,193 | -5.24% | 0.02 | 0.03 |
| bsdtar.x64 | U | 21,800 | 20,680 | -5.14% | 0.13 | 0.13 |
| bunzip2.x64 | 3 | 16,425 | 15,802 | -3.79% | 0.01 | 0.00 |
| bunzip2.x64 | 4 | 16,148 | 15,493 | -4.06% | 0.00 | 0.00 |
| bunzip2.x64 | 5 | 15,641 | 15,045 | -3.81% | 0.01 | 0.01 |
| bunzip2.x64 | 6 | 15,240 | 14,683 | -3.65% | 0.01 | 0.01 |
| bunzip2.x64 | 7 | 15,239 | 14,680 | -3.67% | 0.02 | 0.01 |
| bunzip2.x64 | 8 | 15,424 | 14,912 | -3.32% | 0.01 | 0.01 |
| bunzip2.x64 | 9 | 13,908 | 13,410 | -3.58% | 0.01 | 0.01 |
| bunzip2.x64 | U | 12,646 | 12,216 | -3.40% | 0.07 | 0.07 |
| bzcat.x64 | 3 | 16,425 | 15,802 | -3.79% | 0.00 | 0.00 |
| bzcat.x64 | 4 | 16,148 | 15,493 | -4.06% | 0.00 | 0.01 |
| bzcat.x64 | 5 | 15,641 | 15,045 | -3.81% | 0.01 | 0.01 |
| bzcat.x64 | 6 | 15,240 | 14,683 | -3.65% | 0.01 | 0.01 |
| bzcat.x64 | 7 | 15,239 | 14,680 | -3.67% | 0.01 | 0.02 |
| bzcat.x64 | 8 | 15,424 | 14,912 | -3.32% | 0.00 | 0.01 |
| bzcat.x64 | 9 | 13,908 | 13,410 | -3.58% | 0.01 | 0.01 |
| bzcat.x64 | U | 12,646 | 12,216 | -3.40% | 0.07 | 0.07 |
| calendar.x64 | 3 | 26,281 | 25,283 | -3.80% | 0.01 | 0.00 |
| calendar.x64 | 4 | 25,831 | 24,721 | -4.30% | 0.00 | 0.01 |
| calendar.x64 | 5 | 24,686 | 23,647 | -4.21% | 0.01 | 0.01 |
| calendar.x64 | 6 | 24,091 | 23,060 | -4.28% | 0.01 | 0.01 |
| calendar.x64 | 7 | 24,085 | 23,041 | -4.33% | 0.02 | 0.03 |
| calendar.x64 | 8 | 22,333 | 21,459 | -3.91% | 0.01 | 0.02 |
| calendar.x64 | 9 | 22,182 | 21,274 | -4.09% | 0.01 | 0.02 |
| calendar.x64 | U | 20,144 | 19,389 | -3.75% | 0.08 | 0.10 |
| crontab.x64 | 3 | 15,838 | 15,320 | -3.27% | 0.00 | 0.00 |
| crontab.x64 | 4 | 15,610 | 15,080 | -3.40% | 0.00 | 0.00 |
| crontab.x64 | 5 | 15,041 | 14,561 | -3.19% | 0.01 | 0.01 |
| crontab.x64 | 6 | 14,679 | 14,224 | -3.10% | 0.01 | 0.01 |
| crontab.x64 | 7 | 14,672 | 14,226 | -3.04% | 0.01 | 0.01 |
| crontab.x64 | 8 | 14,750 | 14,400 | -2.37% | 0.01 | 0.01 |
| crontab.x64 | 9 | 13,352 | 12,972 | -2.85% | 0.01 | 0.01 |
| crontab.x64 | U | 12,081 | 11,802 | -2.31% | 0.06 | 0.06 |
| cu.a64 | 2 | 67,454 | 61,520 | -8.80% | 0.00 | 0.00 |
| cu.a64 | 3 | 63,652 | 58,903 | -7.46% | 0.01 | 0.00 |
| cu.a64 | 4 | 62,817 | 57,738 | -8.09% | 0.01 | 0.01 |
| cu.a64 | 5 | 59,053 | 54,586 | -7.56% | 0.02 | 0.03 |
| cu.a64 | 6 | 56,958 | 53,122 | -6.73% | 0.04 | 0.03 |
| cu.a64 | 7 | 56,978 | 53,131 | -6.75% | 0.04 | 0.05 |
| cu.a64 | 8 | 50,090 | 47,157 | -5.86% | 0.03 | 0.04 |
| cu.a64 | 9 | 49,475 | 46,104 | -6.81% | 0.03 | 0.04 |
| cu.a64 | U | 44,969 | 41,453 | -7.82% | 0.20 | 0.20 |
| cu.x64 | 3 | 66,130 | 61,931 | -6.35% | 0.01 | 0.01 |
| cu.x64 | 4 | 64,613 | 60,321 | -6.64% | 0.02 | 0.01 |
| cu.x64 | 5 | 61,431 | 57,411 | -6.54% | 0.02 | 0.03 |
| cu.x64 | 6 | 59,636 | 55,929 | -6.22% | 0.04 | 0.04 |
| cu.x64 | 7 | 59,631 | 55,915 | -6.23% | 0.04 | 0.04 |
| cu.x64 | 8 | 56,436 | 52,931 | -6.21% | 0.03 | 0.06 |
| cu.x64 | 9 | 55,743 | 52,135 | -6.47% | 0.04 | 0.05 |
| cu.x64 | U | 48,006 | 45,299 | -5.64% | 0.18 | 0.19 |
| install.x64 | 3 | 14,272 | 13,659 | -4.30% | 0.00 | 0.00 |
| install.x64 | 4 | 13,996 | 13,379 | -4.41% | 0.00 | 0.00 |
| install.x64 | 5 | 13,586 | 13,008 | -4.25% | 0.01 | 0.01 |
| install.x64 | 6 | 13,319 | 12,715 | -4.53% | 0.01 | 0.01 |
| install.x64 | 7 | 13,315 | 12,712 | -4.53% | 0.01 | 0.01 |
| install.x64 | 8 | 13,564 | 12,998 | -4.17% | 0.01 | 0.01 |
| install.x64 | 9 | 12,046 | 11,597 | -3.73% | 0.01 | 0.01 |
| install.x64 | U | 10,780 | 10,399 | -3.53% | 0.04 | 0.07 |
| ipptool.x64 | 3 | 31,367 | 28,809 | -8.16% | 0.00 | 0.00 |
| ipptool.x64 | 4 | 30,524 | 27,839 | -8.80% | 0.01 | 0.01 |
| ipptool.x64 | 5 | 29,061 | 26,619 | -8.40% | 0.01 | 0.02 |
| ipptool.x64 | 6 | 28,291 | 25,898 | -8.46% | 0.02 | 0.02 |
| ipptool.x64 | 7 | 28,286 | 25,894 | -8.46% | 0.03 | 0.03 |
| ipptool.x64 | 8 | 26,324 | 24,185 | -8.13% | 0.02 | 0.03 |
| ipptool.x64 | 9 | 26,098 | 23,970 | -8.15% | 0.02 | 0.03 |
| ipptool.x64 | U | 22,563 | 20,886 | -7.43% | 0.12 | 0.11 |
| kextutil.x64 | 3 | 42,510 | 40,106 | -5.66% | 0.01 | 0.01 |
| kextutil.x64 | 4 | 41,127 | 38,764 | -5.75% | 0.01 | 0.01 |
| kextutil.x64 | 5 | 39,236 | 37,050 | -5.57% | 0.02 | 0.02 |
| kextutil.x64 | 6 | 37,880 | 35,686 | -5.79% | 0.02 | 0.03 |
| kextutil.x64 | 7 | 37,874 | 35,671 | -5.82% | 0.03 | 0.04 |
| kextutil.x64 | 8 | 35,409 | 33,468 | -5.48% | 0.03 | 0.04 |
| kextutil.x64 | 9 | 34,639 | 32,823 | -5.24% | 0.03 | 0.04 |
| kextutil.x64 | U | 29,622 | 28,190 | -4.83% | 0.14 | 0.16 |
| locale.x64 | 3 | 22,831 | 20,644 | -9.58% | 0.00 | 0.00 |
| locale.x64 | 4 | 22,466 | 20,147 | -10.32% | 0.00 | 0.00 |
| locale.x64 | 5 | 21,477 | 19,404 | -9.65% | 0.01 | 0.01 |
| locale.x64 | 6 | 21,038 | 18,945 | -9.95% | 0.01 | 0.01 |
| locale.x64 | 7 | 21,032 | 18,944 | -9.93% | 0.02 | 0.02 |
| locale.x64 | 8 | 19,278 | 17,375 | -9.87% | 0.01 | 0.01 |
| locale.x64 | 9 | 19,141 | 17,247 | -9.89% | 0.01 | 0.01 |
| locale.x64 | U | 16,807 | 15,173 | -9.72% | 0.08 | 0.10 |
| lsmp.x64 | 3 | 19,109 | 17,964 | -5.99% | 0.00 | 0.00 |
| lsmp.x64 | 4 | 18,675 | 17,444 | -6.59% | 0.00 | 0.00 |
| lsmp.x64 | 5 | 17,926 | 16,776 | -6.42% | 0.01 | 0.01 |
| lsmp.x64 | 6 | 17,455 | 16,319 | -6.51% | 0.01 | 0.01 |
| lsmp.x64 | 7 | 17,453 | 16,316 | -6.51% | 0.02 | 0.02 |
| lsmp.x64 | 8 | 17,650 | 16,400 | -7.08% | 0.01 | 0.01 |
| lsmp.x64 | 9 | 16,091 | 15,052 | -6.46% | 0.01 | 0.01 |
| lsmp.x64 | U | 14,243 | 13,394 | -5.96% | 0.08 | 0.07 |
| mail.x64 | 3 | 46,171 | 43,426 | -5.95% | 0.01 | 0.01 |
| mail.x64 | 4 | 45,134 | 42,275 | -6.33% | 0.01 | 0.01 |
| mail.x64 | 5 | 42,964 | 40,375 | -6.03% | 0.01 | 0.02 |
| mail.x64 | 6 | 41,589 | 39,227 | -5.68% | 0.02 | 0.02 |
| mail.x64 | 7 | 41,592 | 39,200 | -5.75% | 0.03 | 0.03 |
| mail.x64 | 8 | 39,604 | 37,343 | -5.71% | 0.03 | 0.04 |
| mail.x64 | 9 | 39,182 | 36,862 | -5.92% | 0.02 | 0.04 |
| mail.x64 | U | 33,900 | 32,080 | -5.37% | 0.12 | 0.13 |
| powermetrics.x64 | 3 | 58,995 | 56,788 | -3.74% | 0.01 | 0.01 |
| powermetrics.x64 | 4 | 57,134 | 54,846 | -4.00% | 0.01 | 0.01 |
| powermetrics.x64 | 5 | 54,237 | 52,077 | -3.98% | 0.03 | 0.03 |
| powermetrics.x64 | 6 | 52,703 | 50,657 | -3.88% | 0.04 | 0.04 |
| powermetrics.x64 | 7 | 52,671 | 50,619 | -3.90% | 0.05 | 0.05 |
| powermetrics.x64 | 8 | 49,575 | 47,820 | -3.54% | 0.03 | 0.05 |
| powermetrics.x64 | 9 | 48,974 | 47,081 | -3.87% | 0.03 | 0.05 |
| powermetrics.x64 | U | 42,428 | 40,942 | -3.50% | 0.18 | 0.18 |
| ppdpo.x64 | 3 | 44,804 | 40,442 | -9.74% | 0.01 | 0.01 |
| ppdpo.x64 | 4 | 43,409 | 38,980 | -10.20% | 0.01 | 0.01 |
| ppdpo.x64 | 5 | 41,260 | 37,140 | -9.99% | 0.02 | 0.02 |
| ppdpo.x64 | 6 | 40,089 | 36,102 | -9.95% | 0.03 | 0.03 |
| ppdpo.x64 | 7 | 40,089 | 36,099 | -9.95% | 0.04 | 0.04 |
| ppdpo.x64 | 8 | 37,579 | 34,041 | -9.41% | 0.03 | 0.04 |
| ppdpo.x64 | 9 | 37,056 | 33,374 | -9.94% | 0.03 | 0.02 |
| ppdpo.x64 | U | 32,013 | 28,794 | -10.06% | 0.14 | 0.16 |
| sort.x64 | 3 | 30,495 | 29,309 | -3.89% | 0.00 | 0.00 |
| sort.x64 | 4 | 30,034 | 28,779 | -4.18% | 0.01 | 0.01 |
| sort.x64 | 5 | 28,658 | 27,506 | -4.02% | 0.01 | 0.01 |
| sort.x64 | 6 | 27,709 | 26,613 | -3.96% | 0.01 | 0.01 |
| sort.x64 | 7 | 27,703 | 26,616 | -3.92% | 0.02 | 0.03 |
| sort.x64 | 8 | 25,621 | 24,622 | -3.90% | 0.01 | 0.03 |
| sort.x64 | 9 | 25,474 | 24,424 | -4.12% | 0.02 | 0.03 |
| sort.x64 | U | 22,800 | 21,968 | -3.65% | 0.11 | 0.09 |
| sysdiagnose.x64 | 3 | 24,863 | 23,886 | -3.93% | 0.00 | 0.01 |
| sysdiagnose.x64 | 4 | 23,928 | 22,937 | -4.14% | 0.00 | 0.01 |
| sysdiagnose.x64 | 5 | 22,903 | 22,046 | -3.74% | 0.01 | 0.01 |
| sysdiagnose.x64 | 6 | 22,165 | 21,328 | -3.78% | 0.01 | 0.01 |
| sysdiagnose.x64 | 7 | 22,167 | 21,330 | -3.78% | 0.03 | 0.03 |
| sysdiagnose.x64 | 8 | 20,787 | 19,946 | -4.05% | 0.01 | 0.03 |
| sysdiagnose.x64 | 9 | 20,406 | 19,633 | -3.79% | 0.02 | 0.02 |
| sysdiagnose.x64 | U | 17,795 | 17,156 | -3.59% | 0.08 | 0.10 |
| syslog.x64 | 3 | 31,031 | 29,526 | -4.85% | 0.00 | 0.00 |
| syslog.x64 | 4 | 30,506 | 28,999 | -4.94% | 0.01 | 0.01 |
| syslog.x64 | 5 | 29,060 | 27,610 | -4.99% | 0.01 | 0.02 |
| syslog.x64 | 6 | 28,106 | 26,783 | -4.71% | 0.01 | 0.02 |
| syslog.x64 | 7 | 28,115 | 26,764 | -4.81% | 0.02 | 0.02 |
| syslog.x64 | 8 | 25,987 | 24,756 | -4.74% | 0.02 | 0.03 |
| syslog.x64 | 9 | 25,802 | 24,654 | -4.45% | 0.02 | 0.02 |
| syslog.x64 | U | 22,728 | 21,753 | -4.29% | 0.10 | 0.10 |
| systemextensionsctl.x64 | 3 | 20,037 | 19,335 | -3.50% | 0.00 | 0.00 |
| systemextensionsctl.x64 | 4 | 19,411 | 18,596 | -4.20% | 0.00 | 0.00 |
| systemextensionsctl.x64 | 5 | 18,739 | 17,960 | -4.16% | 0.01 | 0.01 |
| systemextensionsctl.x64 | 6 | 18,129 | 17,411 | -3.96% | 0.01 | 0.01 |
| systemextensionsctl.x64 | 7 | 18,130 | 17,411 | -3.97% | 0.02 | 0.02 |
| systemextensionsctl.x64 | 8 | 18,496 | 17,790 | -3.82% | 0.01 | 0.01 |
| systemextensionsctl.x64 | 9 | 16,700 | 16,036 | -3.98% | 0.01 | 0.01 |
| systemextensionsctl.x64 | U | 14,773 | 14,248 | -3.55% | 0.07 | 0.08 |
| top.x64 | 3 | 39,946 | 38,388 | -3.90% | 0.01 | 0.01 |
| top.x64 | 4 | 38,881 | 37,271 | -4.14% | 0.01 | 0.01 |
| top.x64 | 5 | 37,361 | 35,769 | -4.26% | 0.01 | 0.02 |
| top.x64 | 6 | 36,142 | 34,623 | -4.20% | 0.02 | 0.03 |
| top.x64 | 7 | 36,153 | 34,613 | -4.26% | 0.03 | 0.03 |
| top.x64 | 8 | 33,527 | 32,169 | -4.05% | 0.03 | 0.04 |
| top.x64 | 9 | 33,207 | 31,813 | -4.20% | 0.02 | 0.04 |
| top.x64 | U | 29,510 | 28,334 | -3.99% | 0.13 | 0.14 |
| uux.x64 | 3 | 48,197 | 45,837 | -4.90% | 0.00 | 0.01 |
| uux.x64 | 4 | 47,018 | 44,508 | -5.34% | 0.01 | 0.01 |
| uux.x64 | 5 | 45,009 | 42,599 | -5.35% | 0.02 | 0.02 |
| uux.x64 | 6 | 43,624 | 41,400 | -5.10% | 0.02 | 0.03 |
| uux.x64 | 7 | 43,602 | 41,382 | -5.09% | 0.05 | 0.03 |
| uux.x64 | 8 | 41,077 | 39,006 | -5.04% | 0.03 | 0.05 |
| uux.x64 | 9 | 40,736 | 38,489 | -5.52% | 0.02 | 0.03 |
| uux.x64 | U | 35,371 | 33,713 | -4.69% | 0.14 | 0.14 |
| xmllint.x64 | 3 | 26,336 | 24,818 | -5.76% | 0.00 | 0.00 |
| xmllint.x64 | 4 | 25,943 | 24,294 | -6.36% | 0.01 | 0.00 |
| xmllint.x64 | 5 | 25,020 | 23,351 | -6.67% | 0.01 | 0.01 |
| xmllint.x64 | 6 | 24,426 | 22,749 | -6.87% | 0.01 | 0.01 |
| xmllint.x64 | 7 | 24,473 | 22,766 | -6.98% | 0.02 | 0.02 |
| xmllint.x64 | 8 | 21,760 | 20,573 | -5.45% | 0.02 | 0.02 |
| xmllint.x64 | 9 | 21,350 | 20,303 | -4.90% | 0.01 | 0.03 |
| xmllint.x64 | U | 18,712 | 17,621 | -5.83% | 0.09 | 0.11 |
| xsltproc.x64 | 3 | 11,739 | 11,334 | -3.45% | 0.00 | 0.00 |
| xsltproc.x64 | 4 | 11,409 | 10,982 | -3.74% | 0.00 | 0.00 |
| xsltproc.x64 | 5 | 11,147 | 10,734 | -3.71% | 0.01 | 0.01 |
| xsltproc.x64 | 6 | 10,875 | 10,542 | -3.06% | 0.01 | 0.01 |
| xsltproc.x64 | 7 | 10,875 | 10,531 | -3.16% | 0.01 | 0.01 |
| xsltproc.x64 | 8 | 11,310 | 10,914 | -3.50% | 0.00 | 0.01 |
| xsltproc.x64 | 9 | 9,830 | 9,515 | -3.20% | 0.01 | 0.01 |
| xsltproc.x64 | U | 9,051 | 8,776 | -3.04% | 0.06 | 0.06 |

## Timing (encode and decode wall seconds, best of 3)

Base (cda7bf4b) against the final build.

| fix | file | level | T | size before | size after | encode s before | after | decode s before | after | round trip |
|---|---|---|---|---|---|---|---|---|---|---|
| #76 | data2_BGL10M.log | 3 | 1 | 771,651 | 341,581 | 0.029 | 0.025 | 0.010 | 0.010 | ok |
| #76 | data2_BGL10M.log | 3 | 10 | 771,651 | 341,581 | 0.013 | 0.012 | 0.006 | 0.005 | ok |
| #76 | data2_BGL10M.log | 9 | 1 | 338,429 | 219,887 | 0.861 | 0.486 | 0.026 | 0.018 | ok |
| #76 | data2_BGL10M.log | 9 | 10 | 338,429 | 219,887 | 0.462 | 0.265 | 0.026 | 0.018 | ok |
| #144 | ghcn.ipc.none | 3 | 1 | 5,919,571 | 5,919,571 | 0.175 | 0.175 | 0.031 | 0.031 | ok |
| #144 | ghcn.ipc.none | 3 | 10 | 5,919,571 | 5,919,571 | 0.058 | 0.057 | 0.014 | 0.013 | ok |
| #144 | ghcn.ipc.none | 9 | 1 | 1,756,364 | 1,062,305 | 4.359 | 4.952 | 0.093 | 0.056 | ok |
| #144 | ghcn.ipc.none | 9 | 10 | 1,756,364 | 1,062,305 | 2.332 | 2.395 | 0.092 | 0.058 | ok |
| #144 | ghcn.orc.none | 3 | 1 | 2,148,728 | 1,418,437 | 0.046 | 0.045 | 0.013 | 0.009 | ok |
| #144 | ghcn.orc.none | 3 | 10 | 2,148,728 | 1,418,437 | 0.027 | 0.037 | 0.007 | 0.008 | ok |
| #144 | ghcn.orc.none | 9 | 1 | 1,342,322 | 873,343 | 1.976 | 1.961 | 0.163 | 0.077 | ok |
| #144 | ghcn.orc.none | 9 | 10 | 1,342,322 | 873,343 | 0.934 | 0.819 | 0.087 | 0.066 | ok |
| #148 | kennedy.xls | 5 | 1 | 98,009 | 72,132 | 0.035 | 0.048 | 0.003 | 0.003 | ok |
| #148 | kennedy.xls | 5 | 10 | 98,009 | 72,132 | 0.035 | 0.052 | 0.004 | 0.004 | ok |
| #148 | kennedy.xls | 7 | 1 | 132,595 | 69,546 | 0.126 | 0.153 | 0.003 | 0.004 | ok |
| #148 | kennedy.xls | 7 | 10 | 132,595 | 69,546 | 0.125 | 0.150 | 0.003 | 0.004 | ok |
| #148 | webster | 5 | 1 | 9,983,795 | 9,983,795 | 4.058 | 4.009 | 0.030 | 0.030 | ok |
| #148 | webster | 5 | 10 | 9,983,795 | 9,983,795 | 1.340 | 1.329 | 0.019 | 0.019 | ok |
| #148 | webster | 7 | 1 | 8,544,309 | 8,544,309 | 12.503 | 12.547 | 0.033 | 0.032 | ok |
| #148 | webster | 7 | 10 | 8,544,309 | 8,544,309 | 4.387 | 4.409 | 0.033 | 0.031 | ok |
| #149 | mozilla | 8 | 1 | 16,011,130 | 14,447,505 | 1.665 | 4.571 | 1.792 | 0.465 | ok |
| #149 | mozilla | 8 | 10 | 16,011,130 | 14,447,505 | 0.467 | 1.319 | 0.500 | 0.126 | ok |
| #149 | llm_bf16_47mb.safetensors | 8 | 1 | 30,242,348 | 26,942,578 | 5.290 | 3.700 | 3.046 | 0.931 | ok |
| #149 | llm_bf16_47mb.safetensors | 8 | 10 | 30,242,348 | 26,942,578 | 1.993 | 2.018 | 0.852 | 0.668 | ok |
| #64 | w_f16.npy | 3 | 1 | 8,127,021 | 7,132,928 | 0.019 | 0.038 | 0.012 | 0.011 | ok |
| #64 | w_f16.npy | 3 | 10 | 8,127,021 | 7,132,928 | 0.014 | 0.019 | 0.008 | 0.006 | ok |
| #64 | w_f16.npy | 5 | 1 | 8,096,822 | 6,956,101 | 1.741 | 1.302 | 0.012 | 0.022 | ok |
| #64 | w_f16.npy | 5 | 10 | 8,096,822 | 6,956,101 | 0.482 | 0.467 | 0.013 | 0.023 | ok |
| #61 | dickens_cyr.txt | 5 | 1 | 4,015,057 | 3,051,342 | 1.728 | 1.035 | 0.015 | 0.017 | ok |
| #61 | dickens_cyr.txt | 5 | 10 | 4,015,057 | 3,051,342 | 0.923 | 0.546 | 0.015 | 0.018 | ok |
| #61 | dickens_cyr.txt | 7 | 1 | 3,089,012 | 2,804,407 | 4.909 | 1.751 | 0.012 | 0.016 | ok |
| #61 | dickens_cyr.txt | 7 | 10 | 3,089,012 | 2,804,407 | 2.538 | 0.726 | 0.012 | 0.017 | ok |
| #61 | dickens_cyr.txt | U | 1 | 2,231,337 | 2,153,879 | 14.698 | 16.425 | 15.494 | 17.560 | ok |
| #61 | dickens_cyr.txt | U | 10 | 2,231,337 | 2,153,879 | 14.960 | 16.024 | 15.494 | 17.276 | ok |
| #60 | big.bin | 3 | 1 | 3,222,333 | 2,936,281 | 0.039 | 0.045 | 0.009 | 0.011 | ok |
| #60 | big.bin | 3 | 10 | 3,222,333 | 2,936,281 | 0.025 | 0.027 | 0.006 | 0.006 | ok |
| #60 | big.bin | 9 | 1 | 2,602,365 | 2,268,325 | 1.251 | 1.092 | 0.074 | 0.066 | ok |
| #60 | big.bin | 9 | 10 | 2,602,365 | 2,268,325 | 0.815 | 0.636 | 0.074 | 0.066 | ok |
| #141 | air.raw | 3 | 1 | 17,536,403 | 13,709,800 | 0.273 | 0.253 | 0.077 | 0.076 | ok |
| #141 | air.raw | 3 | 10 | 17,536,403 | 13,709,800 | 0.100 | 0.117 | 0.023 | 0.028 | ok |
| #62 | wide3.csv | 9 | 1 | 1,381,334 | 1,080,366 | 0.194 | 0.215 | 0.155 | 0.121 | ok |
| #62 | wide3.csv | 9 | 10 | 1,381,334 | 1,080,366 | 0.180 | 0.217 | 0.139 | 0.116 | ok |
| #83 | w200.csv | 3 | 1 | 1,723,993 | 1,048,461 | 0.033 | 0.147 | 0.011 | 0.011 | ok |
| #83 | w200.csv | 3 | 10 | 1,723,993 | 1,048,461 | 0.018 | 0.044 | 0.005 | 0.006 | ok |
| #83 | w200.csv | 5 | 1 | 1,623,169 | 1,015,711 | 0.973 | 0.339 | 0.012 | 0.010 | ok |
| #83 | w200.csv | 5 | 10 | 1,623,169 | 1,015,711 | 0.479 | 0.118 | 0.012 | 0.010 | ok |
| #82 | bc.x64 | 9 | 1 | 53,703 | 50,019 | 0.023 | 0.035 | 0.004 | 0.004 | ok |
| #82 | bc.x64 | 9 | 10 | 53,703 | 50,019 | 0.023 | 0.036 | 0.004 | 0.004 | ok |

## Compatibility

- **Old decoders:** every frame that changed was re-encoded with the final build and decoded by the final build, by
  the 0.0.2 release binary, and by 0.0.1 (built from tag v0.0.1). That is 397 (file, level) frames: the 10 corpus
  pairs and every changed class frame. All 397 give the input back byte for byte.
- **`make check`:** passes, including the frozen corpus (274 frames) and the FORMAT.md reference decoder (358 frames).
- **`benchmark/determinism/check.py`:** passes after the deliberate regeneration (14 of 152 frames, attributed in that
  commit). 1 and 4 threads give identical frames.

## Not done here

- #141's in-graph split of exact and computed slices (estimated -15% at -5 on NCEP air): a graph change.
- #82's content-based ARM64 detector for code without a header (R18 measured -4.5% for arm64 tools with Kolmo's own map applied first).
- #148's per-span trap detection for KLZH2 inside archives (kennedy.xls inside canterbury.tar is not reached by a prefix warm-up).
