# blosc2 benchmark output, 2026-11-23

Apple M5 (10 cores), macOS; Kolmo main 7b2eb08 built portable (ARCH=), plugin built by `make -C integration/blosc2`. Command:

```
make -C integration/blosc2 bench BENCH_ARGS="-r 5 corpus/owid_energy_f64.npy:f64:128 corpus/population.i64:i64 corpus/esc50-1-100032-A-0.wav:i16:44 synth:f32_grid synth:i64_ticks synth:i16_wave"
```

corpus/ is the 0.0.2 market review corpus (not in the repository). Load average before and after the run (the machine was shared with other jobs):

    22:33  up 18 days,  2:36, 4 users, load averages: 6.80 6.12 4.10
    22:36  up 18 days,  2:39, 4 users, load averages: 12.46 9.13 5.65

c-blosc2 3.8.0, one thread, 4194304-byte chunks, best of 5; MB/s of uncompressed data

### owid_energy_f64.npy (f64, typesize 8, 23.94 MB)

| codec | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| blosclz 5 + shuffle | 4.22 | 2052 | 8364 |
| lz4 5 + shuffle | 4.10 | 4475 | 11021 |
| lz4 5 + bitshuffle | 2.72 | 1209 | 3430 |
| zstd 1 + shuffle | 5.00 | 1647 | 3105 |
| zstd 5 + shuffle | 5.16 | 239 | 3465 |
| zstd 9 + shuffle | 5.37 | 3 | 3264 |
| zstd 5 + bitshuffle | 3.97 | 91 | 1965 |
| kolmo 1 | 5.68 | 132 | 5467 |
| kolmo 3 | 13.18 | 41 | 1607 |
| kolmo 5 | 14.05 | 18 | 1306 |
| kolmo 3 + shuffle | 4.74 | 43 | 1896 |
| kolmo 3, block = chunk | 14.30 | 314 | 2509 |
| kolmo 5, block = chunk | 14.98 | 48 | 1945 |

### population.i64 (i64, typesize 8, 0.15 MB)

| codec | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| blosclz 5 + shuffle | 2.77 | 4085 | 18894 |
| lz4 5 + shuffle | 2.73 | 7198 | 18894 |
| lz4 5 + bitshuffle | 2.67 | 3876 | 6046 |
| zstd 1 + shuffle | 2.85 | 2852 | 8397 |
| zstd 5 + shuffle | 2.95 | 491 | 8397 |
| zstd 9 + shuffle | 3.05 | 8 | 3359 |
| zstd 5 + bitshuffle | 2.85 | 192 | 1778 |
| kolmo 1 | 1.65 | 94 | 1913 |
| kolmo 3 | 3.71 | 25 | 1350 |
| kolmo 5 | 3.87 | 8 | 1014 |
| kolmo 3 + shuffle | 2.83 | 52 | 2191 |
| kolmo 3, block = chunk | 3.83 | 38 | 1574 |
| kolmo 5, block = chunk | 3.93 | 7 | 1128 |

### esc50-1-100032-A-0.wav (i16, typesize 2, 0.44 MB)

| codec | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| blosclz 5 + shuffle | 16.52 | 8018 | 15750 |
| lz4 5 + shuffle | 16.28 | 9188 | 14700 |
| lz4 5 + bitshuffle | 18.20 | 5128 | 3021 |
| zstd 1 + shuffle | 18.43 | 4200 | 6785 |
| zstd 5 + shuffle | 19.36 | 942 | 6485 |
| zstd 9 + shuffle | 19.96 | 97 | 6300 |
| zstd 5 + bitshuffle | 20.04 | 906 | 2689 |
| kolmo 1 | 14.36 | 2436 | 20046 |
| kolmo 3 | 16.42 | 261 | 3973 |
| kolmo 5 | 24.06 | 45 | 3196 |
| kolmo 3 + shuffle | 18.18 | 217 | 6391 |
| kolmo 3, block = chunk | 23.74 | 149 | 1441 |
| kolmo 5, block = chunk | 24.10 | 36 | 1413 |

### synth:f32_grid (f32, typesize 4, 16.78 MB)

| codec | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| blosclz 5 + shuffle | 1.33 | 4420 | 9921 |
| lz4 5 + shuffle | 1.39 | 2362 | 9326 |
| lz4 5 + bitshuffle | 1.46 | 2631 | 5123 |
| zstd 1 + shuffle | 2.06 | 934 | 2030 |
| zstd 5 + shuffle | 2.19 | 99 | 1496 |
| zstd 9 + shuffle | 2.35 | 9 | 1389 |
| zstd 5 + bitshuffle | 1.55 | 276 | 2633 |
| kolmo 1 | 1.30 | 50 | 2294 |
| kolmo 3 | 4.14 | 22 | 1071 |
| kolmo 5 | 4.42 | 9 | 851 |
| kolmo 3 + shuffle | 1.79 | 44 | 1276 |
| kolmo 3, block = chunk | 5.26 | 131 | 1063 |
| kolmo 5, block = chunk | 4.68 | 9 | 903 |

### synth:i64_ticks (i64, typesize 8, 8.39 MB)

| codec | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| blosclz 5 + shuffle | 5.95 | 4263 | 11815 |
| lz4 5 + shuffle | 5.87 | 5343 | 16811 |
| lz4 5 + bitshuffle | 4.75 | 3333 | 5992 |
| zstd 1 + shuffle | 6.46 | 3403 | 5883 |
| zstd 5 + shuffle | 6.90 | 343 | 7151 |
| zstd 9 + shuffle | 7.17 | 11 | 7182 |
| zstd 5 + bitshuffle | 5.70 | 389 | 2140 |
| kolmo 1 | 6.84 | 54 | 4917 |
| kolmo 3 | 8.94 | 38 | 2962 |
| kolmo 5 | 9.08 | 11 | 2451 |
| kolmo 3 + shuffle | 6.72 | 46 | 869 |
| kolmo 3, block = chunk | 9.09 | 250 | 3720 |
| kolmo 5, block = chunk | 9.39 | 35 | 2700 |

### synth:i16_wave (i16, typesize 2, 4.19 MB)

| codec | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| blosclz 5 + shuffle | 1.36 | 726 | 3026 |
| lz4 5 + shuffle | 1.25 | 1284 | 4136 |
| lz4 5 + bitshuffle | 1.27 | 2341 | 5323 |
| zstd 1 + shuffle | 1.48 | 680 | 1968 |
| zstd 5 + shuffle | 1.56 | 73 | 2510 |
| zstd 9 + shuffle | 1.68 | 8 | 1954 |
| zstd 5 + bitshuffle | 1.43 | 298 | 2152 |
| kolmo 1 | 1.00 | 498 | 56680 |
| kolmo 3 | 1.74 | 32 | 1145 |
| kolmo 5 | 1.84 | 5 | 783 |
| kolmo 3 + shuffle | 1.47 | 42 | 1553 |
| kolmo 3, block = chunk | 1.75 | 97 | 1013 |
| kolmo 5, block = chunk | 1.85 | 5 | 388 |

