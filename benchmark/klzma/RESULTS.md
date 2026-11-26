# KLZMA parse: composite paths and nice 273 (review40 #93, R19)

Encoder-only change to KLZMA (codec 24) at parse levels 6-9; the product's `-9` uses level 8. Levels 1-5 are unchanged.
- The optimal parse prices "match, literal, rep 0" and "rep k, literal, rep 0" (one node per match's or rep's longest length).
- Level 8's nice length is 273 (was 192), as in xz -9e.
- Length and distance prices are refreshed every 32 coded lengths (was 128).

The payload format is unchanged. 0.0.1 and 0.0.2 decode the new frames (checked on ooffice, x-ray, sao and samba at `-9 -T 10`).
Frames are the same at every thread count (`benchmark/determinism`, 1 vs 4 threads; `tests/test_native.c`: 1 vs 4 threads at levels 6 and 8 on 1.25 MiB, with a test hook checking that the threaded finder starts).

**Status: sizes and timings measured.** Sections 1-3 are sizes. Section 4 is the timed run: 49 files (891 MB), done in the
coordinator's quiet window on 2026-11-26 from 03:44 to 04:28 UTC. The 1-minute load was 2.6-9.8 for every row (the limit is 12).

**Summary.** `-9` output is 0.06% smaller in total and 0.28% smaller on Silesia. `-9` compression is 6.9% slower at `-T 1`
and 10.6% slower at `-T 10`; on the 10 files whose frames change it is 10.6% and 17.5% slower. Decompression speed and
peak memory are unchanged. `-6` and `-7` frames are byte-identical, and their timings match within 1%.

## 1. `-9`'s KLZMA blocks

These are the 39 KLZMA inputs that `kolmo -9 -T 1` codes for Silesia's mozilla, ooffice, samba, sao and x-ray (R19's
dump, 83,673,102 bytes), coded at level 8 with a harness that links `src/algo/klzma.c` and checks every round trip.

| variant | bytes | change |
|---|---:|---:|
| main (nice 192, no composite paths, refresh 128) | 25,037,829 | |
| composite paths only | 24,937,026 | -0.40% |
| nice 273 only | 25,034,169 | -0.01% |
| composite paths + nice 273 | 24,935,846 | -0.41% |
| + refresh 32 (**this change**) | **24,918,308** | **-0.48%** |
| composite paths + nice 273 + depth 152 | 24,931,341 | -0.43% |
| composite paths + nice 273 + depth 152 + refresh 32 | 24,920,583 | -0.47% |
| composite paths + nice 273 + refresh 64 / 16 / 8 / 1 | 24,927,282 / 24,918,986 / 24,918,148 / 24,915,478 | |

Depth 152 was dropped: it adds nothing on top of refresh 32. Refresh 32 was kept: refreshing more often gains less
than 0.01% more.

## 2. Raw KLZMA level 8 against raw xz -9e (Silesia)

xz is run as `xz --format=raw --lzma1=preset=9e,dict=64MiB -T1`: the same lc3/lp0/pb2 payload, with no container bytes.

| file | xz -9e raw | KLZMA main | KLZMA this change | vs main | vs xz -9e |
|---|---:|---:|---:|---:|---:|
| dickens | 2,830,713 | 2,828,883 | 2,828,718 | -0.01% | -0.07% |
| mozilla | 13,378,857 | 13,380,143 | 13,306,569 | -0.55% | -0.54% |
| mr | 2,751,410 | 2,749,539 | 2,744,567 | -0.18% | -0.25% |
| nci | 1,448,990 | 1,490,905 | 1,445,915 | -3.02% | -0.21% |
| ooffice | 2,426,791 | 2,443,576 | 2,421,840 | -0.89% | -0.20% |
| osdb | 2,844,054 | 2,852,369 | 2,843,167 | -0.32% | -0.03% |
| reymont | 1,315,333 | 1,314,532 | 1,314,382 | -0.01% | -0.07% |
| samba | 3,738,893 | 3,757,889 | 3,730,794 | -0.72% | -0.22% |
| sao | 4,424,921 | 4,403,197 | 4,402,251 | -0.02% | -0.51% |
| webster | 8,367,317 | 8,365,700 | 8,363,329 | -0.03% | -0.05% |
| x-ray | 4,490,511 | 4,478,563 | 4,477,247 | -0.03% | -0.30% |
| xml | 434,764 | 437,274 | 434,203 | -0.70% | -0.13% |
| **total** | **48,452,554** | **48,502,570** | **48,312,982** | **-0.39%** | **-0.29%** |

Before: KLZMA level 8 was 0.10% larger than xz -9e. Now it is 0.29% smaller in total and smaller on every file.

## 3. Product frames (`kolmo -9 -T 1`)

These sizes come from the timed run in section 4. The binary is this branch's head; its hash matches `build/kolmo` at f066aa4d.
Silesia files not listed here, and nci and xml, give identical frames at `-9`.

| file | main | this change | change |
|---|---:|---:|---:|
| mozilla | 13,915,668 | 13,825,852 | -0.65% |
| ooffice | 2,105,453 | 2,086,328 | -0.91% |
| samba | 3,945,893 | 3,941,180 | -0.12% |
| sao | 2,190,606 | 2,190,636 | +0.00% (+30 B) |
| x-ray | 3,619,134 | 3,619,240 | +0.00% (+106 B) |
| **Silesia total** | **41,230,750** | **41,117,232** | **-0.28%** |

Outside Silesia, 5 more files change: energy_test.csv -0.32%, owid-co2-data.csv -0.31%, loghub_all.log -0.25%,
llm_bf16_47mb.safetensors -0.12% and population.i64 -0.02%. enwik8 and the 6 domain files are unchanged.

Only `-9` frames change; `-1` to `-8` and ultra are byte-identical (`benchmark/determinism`, `benchmark/ledger`).

The CI size ledger (`benchmark/ledger/sizes.tsv`) changes only in `-9` rows, which this change refreshes:

| kind | before | after | change |
|---|---:|---:|---:|
| executable | 513,235 | 507,331 | -1.150% |
| multiblock, 4 MiB blocks | 2,992,405 | 2,975,533 | -0.564% |
| csv | 248,034 | 247,934 | -0.040% |
| video | 12,172 | 12,166 | -0.049% |
| binary | 206,760 | 206,687 | -0.035% |
| log | 29,021 | 29,011 | -0.034% |
| numeric | 71,034 | 71,026 | -0.011% |
| image | 874,577 | 874,570 | -0.001% |
| multiblock | 2,994,186 | 2,994,410 | +0.007% |

Two entries grew: concat.bin `-9` by 224 bytes and stations.csv `-9` by 4 bytes. Both are within the ledger tolerance.

## 4. Timed comparison (quiet window)

How the run was done:
- Harness: `review40b/work/R19-93/run.sh` (`bench.py`, `summ.py`), run under `$V/tools/timed.sh`. The raw rows are in
  `benchmark/klzma/timed-rows.tsv` (784 rows).
- Inputs: 49 files, 891 MB: Silesia (12), enwik8, the review corpus (16), holdout (14) and domain (6).
- Binaries: main a1d39559 ("base") and this branch ("new"), at `-6`, `-7` and `-9` with `-T 1` and `-T 10`. They ran interleaved, so any drift
  affects both. References: `xz -9e -T1/-T10 -k -c` and `7zz a -t7z -mx9 -mmt=1/10`.
- Measures: wall time, and peak RSS from `/usr/bin/time -l`.
- Every row is a full round trip, checked with `cmp`: 0 failures.
- Each row records the 1-minute load before and after its file. All rows fall in 2.6-9.8, so none were dropped.
- Frames identical, main vs this change: `-6` and `-7` on 49 of 49 files at both thread counts; `-9` on 39 of 49 (10 changed),
  the same at T1 and T10.

The `-6` and `-7` rows run the same code, so they show the noise: their total compress time differs by +0.0% (`-6 T1`),
+0.2% (`-6 T10`), +1.0% (`-7 T1`) and +0.4% (`-7 T10`). Per set, the `-7` noise reaches 9% (enwik `-T 1`), so only
the totals and the per-file times of the changed files are meaningful.

`-9` compression time, summed:

| files | T | main s | this change s | change |
|---|---:|---:|---:|---:|
| 10 files whose `-9` frame changes | 1 | 34.51 | 38.16 | +10.6% |
| 39 files whose `-9` frame is unchanged | 1 | 33.36 | 34.40 | +3.1% |
| 10 files whose `-9` frame changes | 10 | 10.93 | 12.84 | +17.5% |
| 39 files whose `-9` frame is unchanged | 10 | 18.08 | 19.24 | +6.4% |

The files with unchanged frames also slow down because `-9` still runs KLZMA as a candidate and keeps another codec.
Per file at `-T 1`, the slowdown is largest where KLZMA codes most of the file: x-ray +19%, owid-co2-data.csv +26%,
energy_test.csv +25%, loghub_all.log +24%, retina.ppm +14%, mozilla +14%, ooffice +10% and samba +13%.

### 4a. kolmo, main vs this change

Size is the sum of compressed bytes; speeds are total input MB over total wall seconds; memory is the largest peak RSS over the set's files.

| set | MB | setting | T | base size | new size | change | base c MB/s | new c MB/s | base d MB/s | new d MB/s | base c peak MiB | new c peak MiB | new d peak MiB |
|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| silesia | 211.9 | -6 | 1 | 48,507,716 | 48,507,716 | +0.00% | 6.92 | 7.01 | 1412.9 | 1513.8 | 230 | 225 | 42 |
| silesia | 211.9 | -6 | 10 | 48,507,716 | 48,507,716 | +0.00% | 21.22 | 21.34 | 2354.9 | 2649.2 | 523 | 516 | 85 |
| silesia | 211.9 | -7 | 1 | 48,012,296 | 48,012,296 | +0.00% | 5.07 | 5.03 | 1412.9 | 1412.9 | 291 | 292 | 58 |
| silesia | 211.9 | -7 | 10 | 48,012,296 | 48,012,296 | +0.00% | 13.55 | 13.45 | 2119.4 | 2119.4 | 671 | 685 | 85 |
| silesia | 211.9 | -9 | 1 | 41,230,750 | 41,117,232 | -0.28% | 10.52 | 9.44 | 72.1 | 72.1 | 514 | 515 | 210 |
| silesia | 211.9 | -9 | 10 | 41,230,750 | 41,117,232 | -0.28% | 26.56 | 23.87 | 157.0 | 157.0 | 913 | 892 | 235 |
| enwik | 100.0 | -6 | 1 | 25,720,382 | 25,720,382 | +0.00% | 5.02 | 4.85 | 909.1 | 909.1 | 476 | 476 | 215 |
| enwik | 100.0 | -6 | 10 | 25,720,382 | 25,720,382 | +0.00% | 17.21 | 16.75 | 909.1 | 909.1 | 485 | 485 | 215 |
| enwik | 100.0 | -7 | 1 | 25,076,822 | 25,076,822 | +0.00% | 3.69 | 3.35 | 714.3 | 714.3 | 527 | 527 | 213 |
| enwik | 100.0 | -7 | 10 | 25,076,822 | 25,076,822 | +0.00% | 12.79 | 12.89 | 714.3 | 714.3 | 537 | 537 | 213 |
| enwik | 100.0 | -9 | 1 | 20,798,338 | 20,798,338 | +0.00% | 31.15 | 30.49 | 37.9 | 38.5 | 738 | 738 | 498 |
| enwik | 100.0 | -9 | 10 | 20,798,338 | 20,798,338 | +0.00% | 67.57 | 65.79 | 172.4 | 163.9 | 782 | 785 | 527 |
| corpus | 139.9 | -6 | 1 | 29,212,965 | 29,212,965 | +0.00% | 7.98 | 8.02 | 1399.4 | 1399.4 | 266 | 266 | 150 |
| corpus | 139.9 | -6 | 10 | 29,212,965 | 29,212,965 | +0.00% | 20.73 | 20.67 | 1999.1 | 2332.3 | 413 | 414 | 152 |
| corpus | 139.9 | -7 | 1 | 28,685,666 | 28,685,666 | +0.00% | 3.00 | 3.02 | 1399.4 | 1399.4 | 261 | 261 | 152 |
| corpus | 139.9 | -7 | 10 | 28,685,666 | 28,685,666 | +0.00% | 7.65 | 7.62 | 2332.3 | 2332.3 | 555 | 554 | 158 |
| corpus | 139.9 | -9 | 1 | 26,678,808 | 26,676,129 | -0.01% | 11.97 | 10.98 | 58.6 | 59.3 | 282 | 282 | 247 |
| corpus | 139.9 | -9 | 10 | 26,678,808 | 26,676,129 | -0.01% | 19.57 | 17.54 | 98.5 | 101.4 | 302 | 302 | 265 |
| holdout | 223.9 | -6 | 1 | 96,703,393 | 96,703,393 | +0.00% | 3.81 | 3.80 | 799.7 | 799.7 | 341 | 341 | 178 |
| holdout | 223.9 | -6 | 10 | 96,703,393 | 96,703,393 | +0.00% | 15.38 | 15.37 | 1865.9 | 1865.9 | 552 | 541 | 191 |
| holdout | 223.9 | -7 | 1 | 95,591,727 | 95,591,727 | +0.00% | 3.01 | 3.01 | 746.3 | 772.1 | 365 | 365 | 178 |
| holdout | 223.9 | -7 | 10 | 95,591,727 | 95,591,727 | +0.00% | 11.07 | 11.08 | 1492.7 | 1492.7 | 659 | 682 | 191 |
| holdout | 223.9 | -9 | 1 | 87,623,647 | 87,586,217 | -0.04% | 8.92 | 8.48 | 30.2 | 30.2 | 591 | 591 | 337 |
| holdout | 223.9 | -9 | 10 | 87,623,647 | 87,586,217 | -0.04% | 24.82 | 22.06 | 91.0 | 90.3 | 595 | 595 | 361 |
| domain | 215.1 | -6 | 1 | 73,514,555 | 73,514,555 | +0.00% | 3.96 | 3.98 | 651.8 | 651.8 | 341 | 341 | 178 |
| domain | 215.1 | -6 | 10 | 73,514,555 | 73,514,555 | +0.00% | 16.20 | 16.24 | 1654.5 | 1536.3 | 552 | 540 | 191 |
| domain | 215.1 | -7 | 1 | 72,311,996 | 72,311,996 | +0.00% | 2.52 | 2.51 | 651.8 | 651.8 | 365 | 365 | 178 |
| domain | 215.1 | -7 | 10 | 72,311,996 | 72,311,996 | +0.00% | 9.62 | 9.53 | 1536.3 | 1536.3 | 659 | 692 | 191 |
| domain | 215.1 | -9 | 1 | 66,414,687 | 66,414,687 | +0.00% | 27.82 | 28.01 | 28.7 | 29.1 | 431 | 431 | 337 |
| domain | 215.1 | -9 | 10 | 66,414,687 | 66,414,687 | +0.00% | 63.63 | 60.59 | 116.3 | 115.0 | 485 | 485 | 361 |
| all | 890.9 | -6 | 1 | 273,659,011 | 273,659,011 | +0.00% | 4.92 | 4.92 | 918.4 | 928.0 | 476 | 476 | 215 |
| all | 890.9 | -6 | 10 | 273,659,011 | 273,659,011 | +0.00% | 17.68 | 17.65 | 1713.2 | 1746.8 | 552 | 541 | 215 |
| all | 890.9 | -7 | 1 | 269,678,507 | 269,678,507 | +0.00% | 3.23 | 3.20 | 873.4 | 882.0 | 527 | 527 | 213 |
| all | 890.9 | -7 | 10 | 269,678,507 | 269,678,507 | +0.00% | 10.56 | 10.52 | 1509.9 | 1509.9 | 671 | 692 | 213 |
| all | 890.9 | -9 | 1 | 242,746,230 | 242,592,603 | -0.06% | 13.13 | 12.28 | 39.0 | 39.2 | 738 | 738 | 498 |
| all | 890.9 | -9 | 10 | 242,746,230 | 242,592,603 | -0.06% | 30.71 | 27.77 | 116.3 | 115.8 | 913 | 892 | 527 |

### 4b. `-9` against xz -9e and 7-Zip -mx9

| set | T | tool | size | vs kolmo -9 new | c MB/s | d MB/s | c peak MiB | d peak MiB |
|---|---:|---|---:|---:|---:|---:|---:|---:|
| silesia | 1 | kolmo -9 (main) | 41,230,750 | +0.28% | 10.52 | 72.1 | 514 | 210 |
| silesia | 1 | kolmo -9 (this PR) | 41,117,232 | +0.00% | 9.44 | 72.1 | 515 | 210 |
| silesia | 1 | xz -9e | 48,456,004 | +17.85% | 3.36 | 136.7 | 506 | 51 |
| silesia | 1 | 7zz -mx9 | 48,757,480 | +18.58% | 4.17 | 194.4 | 508 | 66 |
| silesia | 10 | kolmo -9 (main) | 41,230,750 | +0.28% | 26.56 | 157.0 | 913 | 236 |
| silesia | 10 | kolmo -9 (this PR) | 41,117,232 | +0.00% | 23.87 | 157.0 | 892 | 235 |
| silesia | 10 | xz -9e | 48,456,100 | +17.85% | 3.42 | 140.4 | 568 | 113 |
| silesia | 10 | 7zz -mx9 | 48,758,631 | +18.58% | 6.42 | 192.7 | 513 | 66 |
| enwik | 1 | kolmo -9 (main) | 20,798,338 | +0.00% | 31.15 | 37.9 | 738 | 498 |
| enwik | 1 | kolmo -9 (this PR) | 20,798,338 | +0.00% | 30.49 | 38.5 | 738 | 498 |
| enwik | 1 | xz -9e | 24,831,648 | +19.39% | 1.87 | 137.0 | 674 | 66 |
| enwik | 1 | 7zz -mx9 | 24,799,182 | +19.24% | 1.93 | 212.8 | 990 | 124 |
| enwik | 10 | kolmo -9 (main) | 20,798,338 | +0.00% | 67.57 | 172.4 | 782 | 527 |
| enwik | 10 | kolmo -9 (this PR) | 20,798,338 | +0.00% | 65.79 | 163.9 | 785 | 527 |
| enwik | 10 | xz -9e | 24,831,656 | +19.39% | 2.13 | 156.2 | 793 | 185 |
| enwik | 10 | 7zz -mx9 | 24,799,463 | +19.24% | 2.77 | 217.4 | 995 | 124 |
| corpus | 1 | kolmo -9 (main) | 26,678,808 | +0.01% | 11.97 | 58.6 | 282 | 247 |
| corpus | 1 | kolmo -9 (this PR) | 26,676,129 | +0.00% | 10.98 | 59.3 | 282 | 247 |
| corpus | 1 | xz -9e | 34,472,824 | +29.23% | 3.47 | 122.8 | 458 | 46 |
| corpus | 1 | 7zz -mx9 | 34,546,819 | +29.50% | 5.17 | 177.1 | 459 | 57 |
| corpus | 10 | kolmo -9 (main) | 26,678,808 | +0.01% | 19.57 | 98.5 | 302 | 265 |
| corpus | 10 | kolmo -9 (this PR) | 26,676,129 | +0.00% | 17.54 | 101.4 | 302 | 265 |
| corpus | 10 | xz -9e | 34,472,952 | +29.23% | 3.46 | 126.1 | 511 | 98 |
| corpus | 10 | 7zz -mx9 | 34,546,819 | +29.50% | 7.86 | 174.9 | 464 | 57 |
| holdout | 1 | kolmo -9 (main) | 87,623,647 | +0.04% | 8.92 | 30.2 | 591 | 337 |
| holdout | 1 | kolmo -9 (this PR) | 87,586,217 | +0.00% | 8.48 | 30.2 | 591 | 337 |
| holdout | 1 | xz -9e | 101,380,604 | +15.75% | 2.40 | 70.2 | 472 | 47 |
| holdout | 1 | 7zz -mx9 | 101,326,313 | +15.69% | 2.74 | 99.5 | 473 | 79 |
| holdout | 10 | kolmo -9 (main) | 87,623,647 | +0.04% | 24.82 | 91.0 | 595 | 361 |
| holdout | 10 | kolmo -9 (this PR) | 87,586,217 | +0.00% | 22.06 | 90.3 | 595 | 361 |
| holdout | 10 | xz -9e | 101,380,716 | +15.75% | 2.41 | 71.5 | 541 | 120 |
| holdout | 10 | 7zz -mx9 | 101,325,619 | +15.69% | 4.41 | 99.5 | 478 | 79 |
| domain | 1 | kolmo -9 (main) | 66,414,687 | +0.00% | 27.82 | 28.7 | 431 | 337 |
| domain | 1 | kolmo -9 (this PR) | 66,414,687 | +0.00% | 28.01 | 29.1 | 431 | 337 |
| domain | 1 | xz -9e | 81,102,504 | +22.12% | 2.57 | 79.7 | 458 | 46 |
| domain | 1 | 7zz -mx9 | 81,388,833 | +22.55% | 3.14 | 115.6 | 460 | 79 |
| domain | 10 | kolmo -9 (main) | 66,414,687 | +0.00% | 63.63 | 116.3 | 485 | 361 |
| domain | 10 | kolmo -9 (this PR) | 66,414,687 | +0.00% | 60.59 | 115.0 | 485 | 361 |
| domain | 10 | xz -9e | 81,102,552 | +22.12% | 2.57 | 80.9 | 532 | 120 |
| domain | 10 | 7zz -mx9 | 81,388,253 | +22.55% | 4.41 | 116.3 | 465 | 79 |
| all | 1 | kolmo -9 (main) | 242,746,230 | +0.06% | 13.13 | 39.0 | 738 | 498 |
| all | 1 | kolmo -9 (this PR) | 242,592,603 | +0.00% | 12.28 | 39.2 | 738 | 498 |
| all | 1 | xz -9e | 290,243,584 | +19.64% | 2.67 | 95.7 | 674 | 66 |
| all | 1 | 7zz -mx9 | 290,818,627 | +19.88% | 3.18 | 137.9 | 990 | 124 |
| all | 10 | kolmo -9 (main) | 242,746,230 | +0.06% | 30.71 | 116.3 | 913 | 527 |
| all | 10 | kolmo -9 (this PR) | 242,592,603 | +0.00% | 27.77 | 115.8 | 892 | 527 |
| all | 10 | xz -9e | 290,243,976 | +19.64% | 2.73 | 98.4 | 793 | 185 |
| all | 10 | 7zz -mx9 | 290,818,785 | +19.88% | 4.78 | 137.9 | 995 | 124 |

Notes on 4b:
- Across all 49 files, kolmo `-9` is 19.6% smaller than xz -9e and 19.9% smaller than 7zz -mx9.
- At `-T 1` it compresses 4.6x faster than xz -9e. It decompresses 2.4x slower (39 vs 96 MB/s) and uses more decode memory
  (peak 498 vs 66 MiB).
- kolmo `-9` is still larger than xz -9e on 3 files: mozilla (+3.4%), samba (+5.4%) and llm_bf16_47mb.safetensors (+4.5%).
  Raw KLZMA alone beats xz on mozilla and samba (section 2), so the gap comes from how `-9` splits and routes those files, not from the parse.

The per-file table (all 49 files at `-9 -T 1`) is in `review40b/work/R19-93/summary.md`; `timed-rows.tsv` has every row.

## 5. Not done here

- R19's #94 (mixed literals, dual-rate counters) and #95 (nibble rANS) change the decoder, so they need a new codec id.
  This change is encoder-only.
- R19 F5 (the KLZMA level-1 slice probe) is planner work.
- Level 9 (depth 152) is still reached only through the C API: `native_level` maps role levels 6-9 to 8. `klzma.h` now
  says so.
