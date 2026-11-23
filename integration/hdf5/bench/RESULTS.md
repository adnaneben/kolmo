# hdf5 benchmark output, 2026-11-23

Apple M5 (10 cores), macOS; Kolmo main 7b2eb08 built portable (ARCH=), plugin built by `make -C integration/hdf5`. Command:

```
make -C integration/hdf5 bench BENCH_ARGS="-r 5 corpus/owid_energy_f64.npy:f64:128 corpus/population.i64:i64 corpus/esc50-1-100032-A-0.wav:i16:44 synth:f32_grid synth:i64_ticks synth:i16_wave"
```

corpus/ is the 0.0.2 market review corpus (not in the repository). Load average before and after the run (the machine was shared with other jobs):

    22:31  up 18 days,  2:34, 4 users, load averages: 12.87 6.23 3.83
    22:33  up 18 days,  2:36, 4 users, load averages: 6.80 6.12 4.10

HDF5 library version: 2.2.0; filters on one thread, 1048576-byte chunks, best of 5; MB/s of uncompressed data

### owid_energy_f64.npy (f64, 23.94 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 1 | 7.63 | 304 | 2147 |
| gzip 6 | 8.39 | 141 | 2394 |
| shuffle + gzip 6 | 5.03 | 72 | 990 |
| szip nn | 2.87 | 217 | 188 |
| zstd 3 | 8.49 | 804 | 3482 |
| shuffle + zstd 3 | 5.03 | 857 | 1673 |
| shuffle + zstd 9 | 5.20 | 188 | 1752 |
| shuffle + zstd 19 | 5.34 | 7 | 1828 |
| kolmo 1 | 6.73 | 1824 | 6963 |
| kolmo 3 | 15.38 | 271 | 2535 |
| kolmo 5 | 15.73 | 60 | 1872 |
| kolmo 7 | 16.05 | 4 | 2363 |
| shuffle + kolmo 3 | 4.98 | 288 | 1551 |

### population.i64 (i64, 0.15 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 1 | 2.08 | 161 | 854 |
| gzip 6 | 2.05 | 23 | 922 |
| shuffle + gzip 6 | 2.97 | 161 | 1260 |
| szip nn | 3.08 | 483 | 422 |
| zstd 3 | 2.33 | 540 | 1087 |
| shuffle + zstd 3 | 2.94 | 1608 | 2478 |
| shuffle + zstd 9 | 2.97 | 481 | 2256 |
| shuffle + zstd 19 | 3.05 | 22 | 1963 |
| kolmo 1 | 2.92 | 243 | 4319 |
| kolmo 3 | 3.83 | 46 | 1758 |
| kolmo 5 | 3.93 | 9 | 1172 |
| kolmo 7 | 3.94 | 2 | 1111 |
| shuffle + kolmo 3 | 2.92 | 102 | 1821 |

### esc50-1-100032-A-0.wav (i16, 0.44 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 1 | 15.09 | 845 | 4955 |
| gzip 6 | 16.00 | 398 | 3128 |
| shuffle + gzip 6 | 18.21 | 347 | 1853 |
| szip nn | 15.38 | 1521 | 1070 |
| zstd 3 | 16.93 | 4594 | 5803 |
| shuffle + zstd 3 | 18.90 | 2321 | 2436 |
| shuffle + zstd 9 | 19.12 | 1148 | 2864 |
| shuffle + zstd 19 | 19.99 | 190 | 2689 |
| kolmo 1 | 13.94 | 2005 | 6300 |
| kolmo 3 | 23.79 | 177 | 1646 |
| kolmo 5 | 24.15 | 45 | 1531 |
| kolmo 7 | 24.15 | 8 | 1465 |
| shuffle + kolmo 3 | 18.68 | 233 | 2492 |

### synth:f32_grid (f32, 16.78 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 1 | 2.40 | 201 | 1009 |
| gzip 6 | 2.64 | 61 | 1177 |
| shuffle + gzip 6 | 2.14 | 55 | 636 |
| szip nn | 1.63 | 424 | 335 |
| zstd 3 | 2.76 | 337 | 1324 |
| shuffle + zstd 3 | 2.20 | 742 | 1513 |
| shuffle + zstd 9 | 2.23 | 254 | 1536 |
| shuffle + zstd 19 | 2.35 | 23 | 1247 |
| kolmo 1 | 1.70 | 528 | 2887 |
| kolmo 3 | 5.49 | 158 | 1170 |
| kolmo 5 | 5.61 | 36 | 898 |
| kolmo 7 | 5.60 | 16 | 1100 |
| shuffle + kolmo 3 | 2.15 | 255 | 1353 |

### synth:i64_ticks (i64, 8.39 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 1 | 3.07 | 237 | 1186 |
| gzip 6 | 3.05 | 25 | 1003 |
| shuffle + gzip 6 | 6.90 | 192 | 2091 |
| szip nn | 5.69 | 527 | 446 |
| zstd 3 | 3.94 | 720 | 1560 |
| shuffle + zstd 3 | 6.65 | 1802 | 2572 |
| shuffle + zstd 9 | 6.91 | 419 | 2849 |
| shuffle + zstd 19 | 7.16 | 29 | 2725 |
| kolmo 1 | 6.39 | 1495 | 4120 |
| kolmo 3 | 9.09 | 231 | 3732 |
| kolmo 5 | 9.34 | 34 | 2553 |
| kolmo 7 | 9.11 | 3 | 3407 |
| shuffle + kolmo 3 | 6.37 | 266 | 1413 |

### synth:i16_wave (i16, 4.19 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 1 | 1.14 | 38 | 442 |
| gzip 6 | 1.09 | 27 | 452 |
| shuffle + gzip 6 | 1.59 | 26 | 1537 |
| szip nn | 1.53 | 453 | 271 |
| zstd 3 | 1.08 | 1112 | 1492 |
| shuffle + zstd 3 | 1.57 | 705 | 1451 |
| shuffle + zstd 9 | 1.59 | 131 | 1648 |
| shuffle + zstd 19 | 1.68 | 11 | 1392 |
| kolmo 1 | 1.00 | 5785 | 9709 |
| kolmo 3 | 1.75 | 98 | 1121 |
| kolmo 5 | 1.85 | 8 | 639 |
| kolmo 7 | 1.84 | 3 | 810 |
| shuffle + kolmo 3 | 1.56 | 251 | 1398 |

