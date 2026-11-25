# KLZMA parse: composite paths and nice 273 (review40 #93, R19)

Encoder-only change to KLZMA (codec 24) at parse levels 6-9; the product's `-9` uses level 8. Levels 1-5 are unchanged.
- The optimal parse prices "match, literal, rep 0" and "rep k, literal, rep 0" (one node per match's or rep's longest length).
- Level 8's nice length is 273 (was 192), as in xz -9e.
- Length and distance prices are refreshed every 32 coded lengths (was 128).

The payload format is unchanged. 0.0.1 and 0.0.2 decode the new frames (checked on ooffice, x-ray, sao and samba at `-9 -T 10`).
Frames are the same at every thread count (`benchmark/determinism`, 1 vs 4 threads; `tests/test_native.c`: 1 vs 4 threads at levels 6 and 8 on 1.25 MiB, with a test hook checking that the threaded finder starts).

**Status: sizes only.** These results come from a heavily loaded shared machine (load average 40-170 on 10 cores), so
speed and memory figures are not given here. The timed comparison is pending a quiet machine: kolmo `-6`, `-7` and `-9` at
`-T 1` and `-T 10`, against `xz -9e` and `7zz -mx9`, on Silesia, enwik8 and the review corpora.

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

| file | main | this change | change |
|---|---:|---:|---:|
| ooffice | 2,105,453 | 2,087,372 | -0.86% |
| x-ray | 3,619,134 | 3,618,863 | -0.01% |
| sao | 2,190,606 | 2,190,472 | -0.01% |
| nci, xml | unchanged | unchanged | (coded by BWT/CM at `-9`, not KLZMA) |

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

## 4. Not done here

- R19's #94 (mixed literals, dual-rate counters) and #95 (nibble rANS) change the decoder, so they need a new codec id.
  This change is encoder-only.
- R19 F5 (the KLZMA level-1 slice probe) is planner work.
- Level 9 (depth 152) is still reached only through the C API: `native_level` maps role levels 6-9 to 8. `klzma.h` now
  says so.
