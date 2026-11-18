# Wave 4, LZ lane: Kolmo's LZ coders against LZ4, zstd and liblzma

Branch `claude/w4-lz-20261118`, from `claude/release-rc3-20261116` (`5071f38`) with `claude/rc3w3-decmap-20261118`
merged. Scope: KLZ (codec 28) against LZ4 -1, KLZH (codec 29) against zstd -1/-3 (and -19), KLZMA (codec 24) against
liblzma 9e. Everything stays opt-in (`KOLMO_NATIVE`); no default output changed (determinism: same hashes).

**Machine:** cloud VM, Intel Xeon 2.10 GHz, 4 vCPUs (AVX-512), 15 GiB, Linux 6.18, GCC 13.3, `-O3 -march=native`
(the repository's flags). No hardware counters were available (no `perf`); the VM's run-to-run noise is ±5–15% per
process, so every decision below was taken on interleaved A/B runs in one process (two builds of `klz.c` under
renamed symbols, alternating on the same buffers), and the tables are the per-file component runs.

**Data** (outside the repository, downloads unpacked with `python3 -I`):
- Silesia (12 files, 211,938,580 bytes);
- enwik8 (100,000,000 bytes);
- structured: OWID `owid-co2-data.csv` (15.2 MB), `owid-energy-data.csv` (9.2 MB), `owid-covid-latest.json`
  (0.5 MB), `owid-co2.json` (103 MB: the first 40,000 rows of the CO2 CSV as JSON records, indented), `logs.log`
  (Loghub Apache, HDFS, Linux, OpenStack, Spark, Thunderbird 2k samples joined, four times: 7.2 MB).

**Method** (`perfile.py`, `table.py`, `run_all.sh`; `build/natbench` from `benchmark/rc3w2/native/build.sh`): each
file cut into blocks as Kolmo cuts them (4 MiB for `speed` and `balanced`, 16 MiB for `ratio`), the blocks coded on
1 or 4 workers (separate runs, never mixed), every block decoded and compared; compress best of the runs for the
fast coders (one pass for LZMA-class), decode best of the runs. Each (coder, file, thread count) is its own process,
so the RSS column is the process peak (`wait4`): the coder's memory plus the harness buffers (input, one output and
one decode buffer per block), which are the same for both coders of a pair. Totals: sums of bytes, MB/s from the
sum of per-file times.

    sh benchmark/rc3w2/native/build.sh
    sh benchmark/w4/lz/run_all.sh DATA_DIR             # results/speed.tsv, balanced.tsv, ratio.tsv
    python3 benchmark/w4/lz/table.py benchmark/w4/lz/results/speed.tsv lz4:klz1

## Against the targets

| coder | rival | target | measured, 1 thread | measured, 4 threads | met |
|---|---|---|---|---|---|
| KLZ1 | LZ4 -1 | ≥ 5% smaller, compress ≥ 1.0×, decode ≥ 1.0×, RSS ≤ 1.5× | −5.87%, compress 0.98×, decode 1.05×, RSS 1.00–1.05× | −5.87%, 0.95×, 1.06×, RSS 1.00–1.08× | size, decode, memory yes; compress 2–5% short |
| KLZH1 | zstd -1 | size ≤, compress ≥ 0.9×, decode ≥ 0.95× | −1.02%, 0.78×, 1.06× | −1.02%, 0.78×, 0.94× | size yes; compress no; decode at 1 thread only |
| KLZH3 | zstd -3 | same | +1.47%, 0.82×, 0.95× | +1.47%, 0.78×, 0.81× | no |
| KLZH (optimal parse) | zstd -19 | ≤ size at ≥ 0.9× decode | not built | — | no |
| KLZMA8 | liblzma 9e | size ≤ +0.1%, decode ≥ 0.9×, compress ≥ 1.0× | +0.24%, decode 0.73×, compress 1.15× | +0.24%, 0.74×, 1.23× | compress only |

Wave-2 baseline (`benchmark/rc3w2/native/RESULTS.md`, Silesia): KLZ1 −6.9% at compress 0.79× / decode 0.77×; KLZH1
compress 0.65× and +9.9% on structured data; KLZMA decode 0.76× / 0.70×.

## KLZ against LZ4 -1 (4 MiB blocks)


**klz1 against lz4, T1**

| file | lz4 bytes | klz1 bytes | size | lz4 c / d MB/s | klz1 c / d MB/s | compress | decompress | RSS KiB lz4 / klz1 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 6,431,106 | 5,625,147 | -12.53% | 312 / 2302 | 273 / 2590 | 0.88x | 1.13x | 28732 / 29216 |
| enwik8 | 57,285,873 | 52,558,896 | -8.25% | 363 / 2520 | 331 / 2632 | 0.91x | 1.04x | 253992 / 250416 |
| logs.log | 1,268,040 | 1,264,731 | -0.26% | 847 / 3412 | 946 / 3200 | 1.12x | 0.94x | 17492 / 18132 |
| mozilla | 26,441,724 | 25,810,020 | -2.39% | 497 / 2038 | 472 / 2046 | 0.95x | 1.00x | 128400 / 128476 |
| mr | 5,442,495 | 5,448,632 | +0.11% | 384 / 2751 | 446 / 2201 | 1.16x | 0.80x | 27216 / 28060 |
| nci | 5,539,301 | 5,262,764 | -4.99% | 721 / 2769 | 880 / 3261 | 1.22x | 1.18x | 73248 / 73476 |
| ooffice | 4,339,312 | 4,287,293 | -1.20% | 396 / 1742 | 407 / 1884 | 1.03x | 1.08x | 18756 / 19400 |
| osdb | 5,261,215 | 4,492,015 | -14.62% | 358 / 2359 | 500 / 2508 | 1.39x | 1.06x | 27452 / 27276 |
| owid-co2-data.csv | 7,552,419 | 7,243,741 | -4.09% | 367 / 2182 | 361 / 2459 | 0.98x | 1.13x | 39524 / 40736 |
| owid-co2.json | 11,659,489 | 11,220,141 | -3.77% | 1163 / 3887 | 1206 / 3769 | 1.04x | 0.97x | 215280 / 215372 |
| owid-covid-latest.json | 58,927 | 53,817 | -8.67% | 860 / 3488 | 1016 / 4115 | 1.18x | 1.18x | 13484 / 13484 |
| owid-energy-data.csv | 4,582,419 | 4,513,997 | -1.49% | 437 / 2042 | 379 / 2095 | 0.87x | 1.03x | 25116 / 25684 |
| reymont | 3,182,828 | 2,843,710 | -10.65% | 292 / 2206 | 277 / 2561 | 0.95x | 1.16x | 18696 / 19700 |
| samba | 7,721,713 | 7,385,174 | -4.36% | 507 / 2330 | 517 / 2814 | 1.02x | 1.21x | 52056 / 52748 |
| sao | 6,790,441 | 6,548,174 | -3.57% | 507 / 3023 | 385 / 2958 | 0.76x | 0.98x | 23108 / 23768 |
| webster | 20,149,194 | 18,433,986 | -8.51% | 335 / 2343 | 335 / 2554 | 1.00x | 1.09x | 103036 / 102608 |
| x-ray | 8,406,607 | 8,474,143 | +0.80% | 1385 / 6074 | 6998 / 9783 | 5.05x | 1.61x | 27248 / 27232 |
| xml | 1,228,083 | 1,119,761 | -8.82% | 630 / 2698 | 658 / 2739 | 1.04x | 1.02x | 14092 / 14480 |
| **total** | 183,341,186 | 172,586,142 | -5.87% | 494 / 2649 | 485 / 2775 | 0.98x | 1.05x | |

**klz1 against lz4, T4**

| file | lz4 bytes | klz1 bytes | size | lz4 c / d MB/s | klz1 c / d MB/s | compress | decompress | RSS KiB lz4 / klz1 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 6,431,106 | 5,625,147 | -12.53% | 772 / 4739 | 660 / 6276 | 0.86x | 1.32x | 28488 / 30888 |
| enwik8 | 57,285,873 | 52,558,896 | -8.25% | 1381 / 9703 | 1226 / 10100 | 0.89x | 1.04x | 253992 / 254604 |
| logs.log | 1,268,040 | 1,264,731 | -0.26% | 1422 / 4427 | 1626 / 5203 | 1.14x | 1.18x | 17720 / 18128 |
| mozilla | 26,441,724 | 25,810,020 | -2.39% | 1979 / 10097 | 1853 / 9281 | 0.94x | 0.92x | 128508 / 131544 |
| mr | 5,442,495 | 5,448,632 | +0.11% | 1112 / 5314 | 973 / 7152 | 0.88x | 1.35x | 27348 / 28872 |
| nci | 5,539,301 | 5,262,764 | -4.99% | 2376 / 11567 | 2737 / 10051 | 1.15x | 0.87x | 73568 / 75344 |
| ooffice | 4,339,312 | 4,287,293 | -1.20% | 601 / 3206 | 593 / 3403 | 0.99x | 1.06x | 18968 / 19620 |
| osdb | 5,261,215 | 4,492,015 | -14.62% | 956 / 4961 | 951 / 5964 | 0.99x | 1.20x | 27356 / 28348 |
| owid-co2-data.csv | 7,552,419 | 7,243,741 | -4.09% | 1119 / 6462 | 1017 / 6697 | 0.91x | 1.04x | 39536 / 42956 |
| owid-co2.json | 11,659,489 | 11,220,141 | -3.77% | 3835 / 13253 | 4419 / 14409 | 1.15x | 1.09x | 215228 / 217412 |
| owid-covid-latest.json | 58,927 | 53,817 | -8.67% | 858 / 3104 | 1062 / 3860 | 1.24x | 1.24x | 13412 / 13412 |
| owid-energy-data.csv | 4,582,419 | 4,513,997 | -1.49% | 917 / 4447 | 880 / 4075 | 0.96x | 0.92x | 25088 / 26684 |
| reymont | 3,182,828 | 2,843,710 | -10.65% | 458 / 3261 | 404 / 3839 | 0.88x | 1.18x | 18696 / 20120 |
| samba | 7,721,713 | 7,385,174 | -4.36% | 1366 / 6709 | 1401 / 8425 | 1.03x | 1.26x | 52436 / 55428 |
| sao | 6,790,441 | 6,548,174 | -3.57% | 849 / 5188 | 653 / 4916 | 0.77x | 0.95x | 23372 / 23948 |
| webster | 20,149,194 | 18,433,986 | -8.51% | 1195 / 8258 | 1101 / 8894 | 0.92x | 1.08x | 103072 / 106136 |
| x-ray | 8,406,607 | 8,474,143 | +0.80% | 2443 / 11856 | 12494 / 17492 | 5.11x | 1.48x | 27124 / 27388 |
| xml | 1,228,083 | 1,119,761 | -8.82% | 952 / 3673 | 973 / 3075 | 1.02x | 0.84x | 14288 / 14872 |
| **total** | 183,341,186 | 172,586,142 | -5.87% | 1520 / 8267 | 1449 / 8746 | 0.95x | 1.06x | |

### What changed and why (each step an interleaved A/B, first 4 MiB of each Silesia file unless noted)

1. **Streams instead of one interleaved byte stream.** The rc3 format put each token after the previous literal run,
   so the decoder's next load depended on the previous lengths. Now literals, tokens, offsets and extensions are
   separate; the loop carries only pointer additions.
2. **The same parse decoded both ways** (LZ4's sequences re-emitted in KLZ's format): the wave-2 decoder was 0.875×
   LZ4 on the same parse. GCC kept two pointers of the hot loop on the stack (a store-to-load chain per sequence):
   the fast loop is now its own function with few live values, and the room tests are a budget of sequences
   (32 output and 16 input bytes each) computed once, so a short sequence costs one test (its token).
3. **16-bit offsets.** The 16 MiB window with a 3-byte flag cost about 25% more work per sequence (an extra load and
   six operations). A sweep showed the size gain over LZ4 came from the bigger hash table, not the window: with
   LZ4's table size KLZ was within 0.1% of LZ4 even with 8 MiB offsets. Same parse, 16-bit offsets: decode 1.02× LZ4.
4. **Token 4 + 4 bits** (was 3 + 4 + wide): runs of 7–14 literals no longer need an extension byte (mozilla had 20% of
   its sequences on the slow path).
5. **Parse:** a table of 16-bit positions (enough with 16-bit offsets; half the cache), 2^15 entries, 6-byte keys
   (5-byte keys find many 5-byte matches that cost a decoded sequence for 2 bytes: −6.2% size at compress 0.85×;
   6-byte keys: −5.1% at 0.97×), and after a literal run of 64 bytes a match needs 7 bytes (x-ray: compress 5×,
   decode 1.5×, +0.8% size).
6. **`__builtin_memcpy`**: see KLZH below.

Measured and dropped: a decaying skip step after a match (no gain); a repeat-offset probe at the next byte (no gain);
the 32-byte second copy for matches (store-forwarding stalls when 16 ≤ offset < 32); 8 MiB / 1 MiB / 256 KiB windows
(no decode difference). GCC against clang on the same C: clang compresses 4% faster (1.01× LZ4) — the remaining
compress gap is code generation in the probe and emit loop; an assembly kernel for it is the next step.

## KLZH against zstd -1 and -3 (4 MiB blocks)


**klzh1 against zstd1, T1**

| file | zstd1 bytes | klzh1 bytes | size | zstd1 c / d MB/s | klzh1 c / d MB/s | compress | decompress | RSS KiB zstd1 / klzh1 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 4,264,635 | 4,079,369 | -4.34% | 204 / 683 | 164 / 728 | 0.80x | 1.07x | 27164 / 29788 |
| enwik8 | 40,694,488 | 39,327,167 | -3.36% | 247 / 919 | 185 / 910 | 0.75x | 0.99x | 238716 / 242232 |
| logs.log | 683,734 | 376,848 | -44.88% | 735 / 1735 | 1124 / 2820 | 1.53x | 1.63x | 18080 / 17980 |
| mozilla | 19,988,639 | 19,913,529 | -0.38% | 292 / 851 | 284 / 881 | 0.97x | 1.04x | 122964 / 129212 |
| mr | 3,814,844 | 3,770,601 | -1.16% | 333 / 969 | 231 / 930 | 0.69x | 0.96x | 26644 / 33244 |
| nci | 2,855,235 | 3,043,997 | +6.61% | 647 / 1142 | 611 / 1544 | 0.94x | 1.35x | 71476 / 73900 |
| ooffice | 3,593,815 | 3,431,111 | -4.53% | 294 / 658 | 188 / 524 | 0.64x | 0.80x | 18808 / 27764 |
| osdb | 3,738,340 | 3,652,739 | -2.29% | 359 / 1014 | 320 / 1093 | 0.89x | 1.08x | 26600 / 32568 |
| owid-co2-data.csv | 4,190,292 | 4,545,015 | +8.47% | 314 / 938 | 214 / 889 | 0.68x | 0.95x | 37128 / 40876 |
| owid-co2.json | 4,314,454 | 4,901,993 | +13.62% | 1076 / 1904 | 1068 / 2720 | 0.99x | 1.43x | 208780 / 211300 |
| owid-covid-latest.json | 31,952 | 35,727 | +11.81% | 705 / 2190 | 670 / 1976 | 0.95x | 0.90x | 13524 / 13524 |
| owid-energy-data.csv | 2,494,340 | 2,658,622 | +6.59% | 394 / 863 | 169 / 554 | 0.43x | 0.64x | 23660 / 25208 |
| reymont | 2,151,356 | 2,183,172 | +1.48% | 239 / 786 | 186 / 804 | 0.78x | 1.02x | 18220 / 21912 |
| samba | 5,501,656 | 5,555,722 | +0.98% | 398 / 1187 | 282 / 1134 | 0.71x | 0.96x | 50904 / 57868 |
| sao | 6,251,181 | 6,007,850 | -3.89% | 235 / 639 | 186 / 478 | 0.79x | 0.75x | 23540 / 35764 |
| webster | 13,675,709 | 13,443,073 | -1.70% | 295 / 769 | 199 / 1007 | 0.68x | 1.31x | 97536 / 102464 |
| x-ray | 6,772,708 | 6,779,862 | +0.11% | 586 / 896 | 573 / 787 | 0.98x | 0.88x | 26548 / 41208 |
| xml | 694,115 | 726,045 | +4.60% | 547 / 1223 | 438 / 1527 | 0.80x | 1.25x | 14252 / 16408 |
| **total** | 125,711,493 | 124,432,442 | -1.02% | 367 / 1030 | 286 / 1096 | 0.78x | 1.06x | |

**klzh1 against zstd1, T4**

| file | zstd1 bytes | klzh1 bytes | size | zstd1 c / d MB/s | klzh1 c / d MB/s | compress | decompress | RSS KiB zstd1 / klzh1 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 4,264,635 | 4,079,369 | -4.34% | 594 / 2415 | 352 / 1178 | 0.59x | 0.49x | 28020 / 37496 |
| enwik8 | 40,694,488 | 39,327,167 | -3.36% | 902 / 3488 | 752 / 2827 | 0.83x | 0.81x | 239408 / 248468 |
| logs.log | 683,734 | 376,848 | -44.88% | 1186 / 2835 | 1881 / 4274 | 1.59x | 1.51x | 18056 / 19464 |
| mozilla | 19,988,639 | 19,913,529 | -0.38% | 1050 / 2675 | 1064 / 2894 | 1.01x | 1.08x | 123972 / 142588 |
| mr | 3,814,844 | 3,770,601 | -1.16% | 785 / 2191 | 490 / 2095 | 0.62x | 0.96x | 27032 / 38748 |
| nci | 2,855,235 | 3,043,997 | +6.61% | 1864 / 4315 | 1778 / 4305 | 0.95x | 1.00x | 72100 / 81976 |
| ooffice | 3,593,815 | 3,431,111 | -4.53% | 447 / 1060 | 352 / 1064 | 0.79x | 1.00x | 19152 / 31644 |
| osdb | 3,738,340 | 3,652,739 | -2.29% | 909 / 2374 | 530 / 2123 | 0.58x | 0.89x | 27004 / 43284 |
| owid-co2-data.csv | 4,190,292 | 4,545,015 | +8.47% | 1199 / 2345 | 654 / 1942 | 0.55x | 0.83x | 38084 / 49732 |
| owid-co2.json | 4,314,454 | 4,901,993 | +13.62% | 4108 / 7761 | 4217 / 11734 | 1.03x | 1.51x | 209452 / 218292 |
| owid-covid-latest.json | 31,952 | 35,727 | +11.81% | 617 / 2276 | 512 / 1880 | 0.83x | 0.83x | 13500 / 13500 |
| owid-energy-data.csv | 2,494,340 | 2,658,622 | +6.59% | 783 / 2283 | 352 / 2162 | 0.45x | 0.95x | 24208 / 27696 |
| reymont | 2,151,356 | 2,183,172 | +1.48% | 388 / 1641 | 296 / 1452 | 0.76x | 0.88x | 18456 / 23880 |
| samba | 5,501,656 | 5,555,722 | +0.98% | 1140 / 3585 | 1004 / 3446 | 0.88x | 0.96x | 51824 / 74952 |
| sao | 6,251,181 | 6,007,850 | -3.89% | 561 / 1304 | 311 / 1156 | 0.55x | 0.89x | 23808 / 40448 |
| webster | 13,675,709 | 13,443,073 | -1.70% | 876 / 3382 | 650 / 3163 | 0.74x | 0.94x | 98140 / 107472 |
| x-ray | 6,772,708 | 6,779,862 | +0.11% | 1203 / 1696 | 1107 / 1537 | 0.92x | 0.91x | 26844 / 49076 |
| xml | 694,115 | 726,045 | +4.60% | 810 / 1632 | 721 / 1967 | 0.89x | 1.21x | 14252 / 17668 |
| **total** | 125,711,493 | 124,432,442 | -1.02% | 1122 / 3260 | 876 / 3061 | 0.78x | 0.94x | |

**klzh3 against zstd3, T1**

| file | zstd3 bytes | klzh3 bytes | size | zstd3 c / d MB/s | klzh3 c / d MB/s | compress | decompress | RSS KiB zstd3 / klzh3 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 3,681,733 | 3,691,715 | +0.27% | 138 / 866 | 124 / 764 | 0.89x | 0.88x | 27480 / 31024 |
| enwik8 | 35,566,782 | 35,582,546 | +0.04% | 154 / 784 | 120 / 737 | 0.79x | 0.94x | 234260 / 240024 |
| logs.log | 354,965 | 377,365 | +6.31% | 927 / 2580 | 972 / 2852 | 1.05x | 1.11x | 18260 / 20952 |
| mozilla | 18,304,336 | 18,849,249 | +2.98% | 230 / 817 | 172 / 852 | 0.75x | 1.04x | 122096 / 125180 |
| mr | 3,548,122 | 3,585,129 | +1.04% | 172 / 850 | 149 / 820 | 0.87x | 0.96x | 26888 / 29532 |
| nci | 2,846,755 | 2,971,425 | +4.38% | 551 / 1274 | 546 / 1032 | 0.99x | 0.81x | 72136 / 75204 |
| ooffice | 3,130,232 | 3,134,306 | +0.13% | 157 / 560 | 121 / 567 | 0.77x | 1.01x | 19056 / 22688 |
| osdb | 3,511,122 | 3,533,585 | +0.64% | 212 / 932 | 189 / 1137 | 0.89x | 1.22x | 27104 / 34212 |
| owid-co2-data.csv | 4,612,473 | 4,731,208 | +2.57% | 176 / 710 | 152 / 559 | 0.86x | 0.79x | 38320 / 43204 |
| owid-co2.json | 4,538,999 | 4,776,805 | +5.24% | 963 / 2382 | 959 / 2575 | 1.00x | 1.08x | 209700 / 214140 |
| owid-covid-latest.json | 31,800 | 31,965 | +0.52% | 278 / 2165 | 355 / 2491 | 1.28x | 1.15x | 13524 / 13524 |
| owid-energy-data.csv | 2,728,853 | 2,796,389 | +2.47% | 172 / 737 | 147 / 828 | 0.85x | 1.12x | 24712 / 26092 |
| reymont | 1,943,693 | 1,987,153 | +2.24% | 186 / 571 | 140 / 866 | 0.75x | 1.52x | 18688 / 22588 |
| samba | 4,982,897 | 5,146,499 | +3.28% | 280 / 1243 | 230 / 1059 | 0.82x | 0.85x | 51064 / 57748 |
| sao | 5,530,936 | 5,509,176 | -0.39% | 124 / 552 | 91 / 491 | 0.73x | 0.89x | 23468 / 34932 |
| webster | 12,151,695 | 12,252,033 | +0.83% | 164 / 861 | 135 / 828 | 0.82x | 0.96x | 96736 / 102648 |
| x-ray | 6,092,374 | 6,240,295 | +2.43% | 121 / 656 | 125 / 462 | 1.03x | 0.70x | 26448 / 35732 |
| xml | 636,164 | 672,831 | +5.76% | 453 / 1431 | 451 / 1167 | 1.00x | 0.82x | 15116 / 18780 |
| **total** | 114,193,931 | 115,869,674 | +1.47% | 234 / 996 | 192 / 950 | 0.82x | 0.95x | |

**klzh3 against zstd3, T4**

| file | zstd3 bytes | klzh3 bytes | size | zstd3 c / d MB/s | klzh3 c / d MB/s | compress | decompress | RSS KiB zstd3 / klzh3 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 3,681,733 | 3,691,715 | +0.27% | 286 / 2124 | 252 / 1747 | 0.88x | 0.82x | 29460 / 38840 |
| enwik8 | 35,566,782 | 35,582,546 | +0.04% | 549 / 3053 | 520 / 2118 | 0.95x | 0.69x | 237120 / 244620 |
| logs.log | 354,965 | 377,365 | +6.31% | 1465 / 5448 | 1168 / 4359 | 0.80x | 0.80x | 19152 / 25460 |
| mozilla | 18,304,336 | 18,849,249 | +2.98% | 870 / 3027 | 513 / 2702 | 0.59x | 0.89x | 125132 / 132380 |
| mr | 3,548,122 | 3,585,129 | +1.04% | 430 / 1796 | 263 / 2004 | 0.61x | 1.12x | 28736 / 36740 |
| nci | 2,846,755 | 2,971,425 | +4.38% | 1564 / 4293 | 1156 / 3444 | 0.74x | 0.80x | 74704 / 88724 |
| ooffice | 3,130,232 | 3,134,306 | +0.13% | 243 / 1003 | 160 / 1068 | 0.66x | 1.06x | 20048 / 27064 |
| osdb | 3,511,122 | 3,533,585 | +0.64% | 520 / 2511 | 465 / 2733 | 0.90x | 1.09x | 28888 / 45468 |
| owid-co2-data.csv | 4,612,473 | 4,731,208 | +2.57% | 577 / 2792 | 424 / 2370 | 0.74x | 0.85x | 41332 / 55932 |
| owid-co2.json | 4,538,999 | 4,776,805 | +5.24% | 3844 / 8595 | 3018 / 7796 | 0.79x | 0.91x | 212224 / 229444 |
| owid-covid-latest.json | 31,800 | 31,965 | +0.52% | 575 / 1710 | 248 / 962 | 0.43x | 0.56x | 13500 / 13500 |
| owid-energy-data.csv | 2,728,853 | 2,796,389 | +2.47% | 402 / 1847 | 214 / 992 | 0.53x | 0.54x | 26584 / 35428 |
| reymont | 1,943,693 | 1,987,153 | +2.24% | 284 / 1563 | 233 / 1275 | 0.82x | 0.82x | 19772 / 26044 |
| samba | 4,982,897 | 5,146,499 | +3.28% | 792 / 3767 | 640 / 3508 | 0.81x | 0.93x | 53880 / 80836 |
| sao | 5,530,936 | 5,509,176 | -0.39% | 246 / 1092 | 187 / 895 | 0.76x | 0.82x | 24320 / 42700 |
| webster | 12,151,695 | 12,252,033 | +0.83% | 520 / 3148 | 468 / 2239 | 0.90x | 0.71x | 99528 / 106896 |
| x-ray | 6,092,374 | 6,240,295 | +2.43% | 240 / 1247 | 162 / 1140 | 0.67x | 0.91x | 28260 / 41404 |
| xml | 636,164 | 672,831 | +5.76% | 530 / 1421 | 486 / 2202 | 0.92x | 1.55x | 15916 / 20068 |
| **total** | 114,193,931 | 115,869,674 | +1.47% | 685 / 3140 | 536 / 2553 | 0.78x | 0.81x | |

### What changed

- **Level 1: a single-table parse** (6-byte keys, 2^15 positions, the most recent offset tried one byte ahead).
  The two-table parse of wave 2 was −5.2% against zstd -1 on Silesia but +7% on the structured set and 0.67×
  compress; the single table is −1.0% overall at 0.78×.
- **The second most recent offset right after each match** (all fast levels), repeatedly: on the 103 MB JSON zstd
  -1 used half its sequences as repeats, KLZH 13%. KLZH3 against zstd -3: +5.4% → +2.1% (A/B, Silesia + structured).
- **`__builtin_memcpy`** in KLZ, KLZH and khuf: Ubuntu's GCC defines `_FORTIFY_SOURCE=3`, which turns a 16-byte
  `memcpy` into a buffer whose size is known at run time into a call to `__memcpy_chk` (callgrind: 1.44 million calls,
  10% of KLZH's encoder instructions on 8 MB). +13% KLZH compress; the release build has the same flags.
- The sequence loop copies literals 16 bytes at a time and writes the extra bits without a test per field. A
  branch-free form of the offset move-to-front and length codes was 10% slower and was dropped.

Where the time goes (callgrind, webster, KLZH1): the parse 45%, the Huffman stream loop 17%, the sequence loop ~20%.
The parse alone runs at zstd -1's whole speed. Left: a two-position pipelined probe loop like zstd's, and
interleaving the entropy pass with the parse. The optimal parse for the levels facing zstd -19 was not built.

## KLZMA against liblzma 9e (16 MiB blocks, 7 files)


**klzma8 against lzma9e, T1**

| file | lzma9e bytes | klzma8 bytes | size | lzma9e c / d MB/s | klzma8 c / d MB/s | compress | decompress | RSS KiB lzma9e / klzma8 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 2,831,149 | 2,828,883 | -0.08% | 1 / 104 | 1 / 76 | 0.92x | 0.73x | 136760 / 160592 |
| logs.log | 126,169 | 125,926 | -0.19% | 1 / 636 | 3 / 468 | 2.15x | 0.74x | 88112 / 98672 |
| ooffice | 2,427,163 | 2,443,576 | +0.68% | 2 / 51 | 2 / 40 | 0.95x | 0.80x | 81820 / 91996 |
| osdb | 2,844,493 | 2,852,369 | +0.28% | 2 / 82 | 2 / 54 | 0.88x | 0.66x | 136800 / 159640 |
| owid-co2-data.csv | 3,158,182 | 3,177,680 | +0.62% | 1 / 65 | 1 / 58 | 1.22x | 0.88x | 171816 / 205412 |
| samba | 3,814,045 | 3,832,086 | +0.47% | 2 / 126 | 2 / 78 | 1.31x | 0.62x | 207528 / 223464 |
| x-ray | 4,491,203 | 4,478,563 | -0.28% | 2 / 36 | 2 / 25 | 1.00x | 0.71x | 122908 / 147252 |
| **total** | 19,692,404 | 19,739,083 | +0.24% | 1 / 79 | 2 / 58 | 1.15x | 0.73x | |

**klzma8 against lzma9e, T4**

| file | lzma9e bytes | klzma8 bytes | size | lzma9e c / d MB/s | klzma8 c / d MB/s | compress | decompress | RSS KiB lzma9e / klzma8 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| dickens | 2,831,149 | 2,828,883 | -0.08% | 1 / 105 | 1 / 70 | 1.09x | 0.67x | 136788 / 160880 |
| logs.log | 126,169 | 125,926 | -0.19% | 1 / 631 | 3 / 453 | 2.38x | 0.72x | 88052 / 98764 |
| ooffice | 2,427,163 | 2,443,576 | +0.68% | 2 / 56 | 2 / 41 | 1.05x | 0.74x | 81896 / 92156 |
| osdb | 2,844,493 | 2,852,369 | +0.28% | 2 / 69 | 2 / 54 | 1.00x | 0.78x | 136992 / 159728 |
| owid-co2-data.csv | 3,158,182 | 3,177,680 | +0.62% | 1 / 72 | 1 / 54 | 1.22x | 0.75x | 171788 / 205328 |
| samba | 3,814,045 | 3,832,086 | +0.47% | 2 / 144 | 3 / 107 | 1.53x | 0.75x | 208208 / 224364 |
| x-ray | 4,491,203 | 4,478,563 | -0.28% | 2 / 39 | 2 / 29 | 0.90x | 0.74x | 122844 / 147352 |
| **total** | 19,692,404 | 19,739,083 | +0.24% | 1 / 83 | 2 / 62 | 1.23x | 0.74x | |

Decoder experiments (decode only, cached blocks of 5 Silesia files, A/B in one process):

| variant | decode against the current decoder |
|---|---:|
| branch-free normalisation in the tree bits | 0.97× |
| padded input, normalisation without the end test | 0.98× (and unsafe on corrupt input: dropped) |
| select (`?:`) form of the tree bit | 0.79× |
| GCC `-O2`, `-funroll-loops`, `-fno-if-conversion`, `-fno-tree-pre` | 0.87–1.03× |
| **the same C built with clang 18** | **1.13×** (0.81× liblzma) |

The decoder's arithmetic is the same as liblzma's; the difference is code generation of the bit decoder, which
liblzma 5.6+ writes in inline x86-64 assembly. An assembly bit/literal kernel (`src/arch/x86_64/`, C reference kept)
is the next step; it was not started in this lane.

## Default switch

None of the three coders meets all of its targets on this VM, so no default is switched. The closest is KLZ for
`speed`: smaller (−5.9%) and faster to decode (1.05–1.06×) at the same memory, with compress 0.95–0.98× of LZ4.
Proposal: switch `speed` (codec 1 → 28, container format 2, `KOLMO_NATIVE` kept as the A/B switch in reverse) once the
compress loop reaches 1.0× — the clang build already does on this machine, and the integration session measures the
Apple M5 (clang) build, where the switch may already qualify. The switch needs no format work: codec 28 is already a
version-11 codec id and its output is deterministic (one parse per block, no thread or memory dependence).

## Tests

| test | result |
|---|---|
| `make check-native` (round trips at every level, corrupt and truncated payloads under ASan/UBSan, every mode with `KOLMO_NATIVE=all` and `all,strong` at 1 and 4 threads) | OK |
| `make check`, `make check-rc`, `make check-sanitizers`, `make experimental-check GPU_CHECK=`, `make kfam-check` | see the commit that adds this file |
| `benchmark/determinism/check.py` | default output unchanged (no expected hash regenerated) |
