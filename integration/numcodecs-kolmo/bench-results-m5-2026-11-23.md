Machine: Darwin arm64, Python 3.12.8, numcodecs 0.17.1, kolmoc from site-packages; arrays of 8 MiB, best of 3, one thread.

**temperature grid, f4 (0.01 K steps)**, (16, 256, 512), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 4.25 | 1.70x | 289 | 1163 |
| kolmo 5 | 4.27 | 1.71x | 60 | 776 |
| kolmo 9 | 4.32 | 1.73x | 3 | 47 |
| zstd 3 | 1.97 | 0.79x | 335 | 1113 |
| zstd 19 | 2.50 | 1.00x | 7 | 1002 |
| blosc lz4 5 shuffle | 1.71 | 0.68x | 1915 | 2683 |
| blosc zstd 5 shuffle | 2.12 | 0.85x | 183 | 1229 |
| lz4 | 1.40 | 0.56x | 684 | 5242 |

**temperature grid, f4 (full precision)**, (16, 256, 512), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 1.96 | 1.41x | 212 | 1911 |
| kolmo 5 | 1.97 | 1.41x | 11 | 1365 |
| kolmo 9 | 1.98 | 1.41x | 4 | 812 |
| zstd 3 | 1.25 | 0.89x | 615 | 1723 |
| zstd 19 | 1.40 | 1.00x | 11 | 690 |
| blosc lz4 5 shuffle | 1.71 | 1.22x | 1923 | 2794 |
| blosc zstd 5 shuffle | 1.85 | 1.32x | 253 | 2267 |
| lz4 | 1.00 | 0.71x | 9204 | 56347 |

**accelerometer, i2 x3 axes**, (1398101, 3), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 2.62 | 1.43x | 408 | 600 |
| kolmo 5 | 2.73 | 1.49x | 22 | 652 |
| kolmo 9 | 2.84 | 1.55x | 27 | 27 |
| zstd 3 | 1.52 | 0.83x | 483 | 1155 |
| zstd 19 | 1.83 | 1.00x | 8 | 675 |
| blosc lz4 5 shuffle | 1.62 | 0.89x | 1177 | 2357 |
| blosc zstd 5 shuffle | 2.21 | 1.21x | 123 | 1134 |
| lz4 | 1.10 | 0.60x | 2075 | 10237 |

**detector counts, u2 (Poisson)**, (4194304,), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 2.38 | 1.12x | 559 | 1206 |
| kolmo 5 | 2.38 | 1.12x | 24 | 1291 |
| kolmo 9 | 2.35 | 1.10x | 12 | 24 |
| zstd 3 | 1.96 | 0.92x | 315 | 1057 |
| zstd 19 | 2.13 | 1.00x | 13 | 836 |
| blosc lz4 5 shuffle | 1.79 | 0.84x | 2349 | 3116 |
| blosc zstd 5 shuffle | 2.18 | 1.03x | 238 | 1769 |
| lz4 | 1.17 | 0.55x | 870 | 5785 |

**salinity series, f8 (random walk)**, (1048576,), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 1.77 | 1.11x | 549 | 1709 |
| kolmo 5 | 1.79 | 1.13x | 10 | 1960 |
| kolmo 9 | 1.79 | 1.13x | 8 | 2064 |
| zstd 3 | 1.22 | 0.77x | 425 | 1235 |
| zstd 19 | 1.59 | 1.00x | 9 | 1534 |
| blosc lz4 5 shuffle | 1.59 | 1.00x | 3295 | 3610 |
| blosc zstd 5 shuffle | 1.61 | 1.01x | 763 | 2177 |
| lz4 | 1.00 | 0.63x | 30662 | 58885 |

**land-cover classes, u1**, (2896, 2896), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 17.37 | 0.70x | 286 | 1291 |
| kolmo 5 | 17.16 | 0.69x | 26 | 2111 |
| kolmo 9 | 28.03 | 1.13x | 3 | 723 |
| zstd 3 | 12.12 | 0.49x | 1365 | 2509 |
| zstd 19 | 24.87 | 1.00x | 4 | 3581 |
| blosc lz4 5 shuffle | 4.63 | 0.19x | 1396 | 3008 |
| blosc zstd 5 shuffle | 14.17 | 0.57x | 135 | 2891 |
| lz4 | 4.70 | 0.19x | 1466 | 3120 |

**event timestamps, i8 ns**, (1048576,), 8.0 MiB

| codec | ratio | vs zstd 19 | encode MB/s | decode MB/s |
|---|---:|---:|---:|---:|
| kolmo 3 | 4.72 | 1.27x | 643 | 1618 |
| kolmo 5 | 4.75 | 1.27x | 61 | 3648 |
| kolmo 9 | 4.70 | 1.26x | 21 | 143 |
| zstd 3 | 3.70 | 0.99x | 697 | 1504 |
| zstd 19 | 3.73 | 1.00x | 6 | 1449 |
| blosc lz4 5 shuffle | 3.48 | 0.93x | 2388 | 2616 |
| blosc zstd 5 shuffle | 3.79 | 1.02x | 643 | 1706 |
| lz4 | 1.59 | 0.43x | 770 | 4129 |

