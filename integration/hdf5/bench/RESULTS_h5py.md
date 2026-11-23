# h5py + hdf5plugin benchmark output, 2026-11-23

Apple M5, shared machine (load average 11-16). h5py 3.14.0 (bundled HDF5 1.14.6), hdf5plugin 7.1.0, numpy 2.0.2 in a venv; BLOSC_NTHREADS=1. Inputs written by `bench_hdf5 -w`. owid_energy was rerun (best of 3) after fixing the NaN-unsafe comparison; the other inputs are best of 5.

### owid_energy_f64.npy (f64, 23.94 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 6 | 8.39 | 98 | 675 |
| shuffle + gzip 6 | 5.03 | 84 | 391 |
| hdf5plugin Zstd 3 | 8.49 | 898 | 3018 |
| shuffle + Zstd 3 | 5.03 | 562 | 1306 |
| shuffle + Zstd 19 | 5.34 | 5 | 1265 |
| LZ4 | 6.47 | 1480 | 7699 |
| Bitshuffle + LZ4 | 2.97 | 422 | 728 |
| Bitshuffle + Zstd 3 | 3.96 | 253 | 463 |
| Blosc lz4 5 + shuffle | 4.09 | 1257 | 1851 |
| Blosc zstd 5 + shuffle | 5.02 | 154 | 1125 |
| Blosc zstd 5 + bitshuffle | 3.97 | 70 | 785 |
| Blosc2 zstd 5 + shuffle | 5.16 | 156 | 1827 |
| Blosc2 zstd 9 + shuffle | 5.37 | 2 | 2250 |
| kolmo 1 | 6.73 | 1223 | 5798 |
| kolmo 3 | 15.38 | 192 | 1803 |
| kolmo 5 | 15.73 | 40 | 1513 |

| kolmo 1 | (failed: RuntimeError: round trip differs) | | |
| kolmo 3 | (failed: RuntimeError: round trip differs) | | |
| kolmo 5 | (failed: RuntimeError: round trip differs) | | |

### population.i64 (i64, 0.15 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 6 | 2.05 | 17 | 330 |
| shuffle + gzip 6 | 2.97 | 120 | 505 |
| hdf5plugin Zstd 3 | 2.33 | 405 | 774 |
| shuffle + Zstd 3 | 2.94 | 1033 | 1672 |
| shuffle + Zstd 19 | 3.05 | 17 | 1014 |
| LZ4 | 1.49 | 505 | 1702 |
| Bitshuffle + LZ4 | 2.64 | 368 | 417 |
| Bitshuffle + Zstd 3 | 2.73 | 354 | 716 |
| Blosc lz4 5 + shuffle | 2.71 | 1428 | 1983 |
| Blosc zstd 5 + shuffle | 2.97 | 235 | 1016 |
| Blosc zstd 5 + bitshuffle | 2.60 | 56 | 922 |
| Blosc2 zstd 5 + shuffle | 2.94 | 431 | 3310 |
| Blosc2 zstd 9 + shuffle | 3.04 | 8 | 889 |
| kolmo 1 | 2.92 | 161 | 1078 |
| kolmo 3 | 3.83 | 31 | 795 |
| kolmo 5 | 3.93 | 6 | 666 |

### esc50-1-100032-A-0.wav (i16, 0.44 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 6 | 16.00 | 358 | 1283 |
| shuffle + gzip 6 | 18.21 | 290 | 889 |
| hdf5plugin Zstd 3 | 16.93 | 3537 | 3927 |
| shuffle + Zstd 3 | 18.90 | 1171 | 1424 |
| shuffle + Zstd 19 | 19.99 | 128 | 1902 |
| LZ4 | 14.00 | 8969 | 11356 |
| Bitshuffle + LZ4 | 17.31 | 847 | 487 |
| Bitshuffle + Zstd 3 | 18.47 | 695 | 721 |
| Blosc lz4 5 + shuffle | 15.60 | 2389 | 2660 |
| Blosc zstd 5 + shuffle | 19.35 | 819 | 1797 |
| Blosc zstd 5 + bitshuffle | 20.03 | 562 | 1151 |
| Blosc2 zstd 5 + shuffle | 19.22 | 661 | 3114 |
| Blosc2 zstd 9 + shuffle | 19.80 | 105 | 2753 |
| kolmo 1 | 13.94 | 1598 | 4614 |
| kolmo 3 | 23.79 | 127 | 1236 |
| kolmo 5 | 24.15 | 27 | 962 |

### synth_f32_grid (f32, 16.78 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 6 | 2.64 | 39 | 400 |
| shuffle + gzip 6 | 2.14 | 37 | 276 |
| hdf5plugin Zstd 3 | 2.76 | 142 | 539 |
| shuffle + Zstd 3 | 2.20 | 396 | 929 |
| shuffle + Zstd 19 | 2.35 | 8 | 945 |
| LZ4 | 1.38 | 533 | 3872 |
| Bitshuffle + LZ4 | 1.44 | 555 | 677 |
| Bitshuffle + Zstd 3 | 1.48 | 251 | 541 |
| Blosc lz4 5 + shuffle | 1.38 | 1382 | 1825 |
| Blosc zstd 5 + shuffle | 2.09 | 81 | 699 |
| Blosc zstd 5 + bitshuffle | 1.55 | 243 | 879 |
| Blosc2 zstd 5 + shuffle | 2.19 | 75 | 1107 |
| Blosc2 zstd 9 + shuffle | 2.35 | 6 | 966 |
| kolmo 1 | 1.70 | 294 | 2009 |
| kolmo 3 | 5.49 | 83 | 702 |
| kolmo 5 | 5.61 | 16 | 666 |

### synth_i64_ticks (i64, 8.39 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 6 | 3.05 | 14 | 358 |
| shuffle + gzip 6 | 6.90 | 102 | 646 |
| hdf5plugin Zstd 3 | 3.94 | 368 | 1192 |
| shuffle + Zstd 3 | 6.65 | 1433 | 1976 |
| shuffle + Zstd 19 | 7.16 | 18 | 2199 |
| LZ4 | 1.64 | 594 | 2395 |
| Bitshuffle + LZ4 | 4.24 | 654 | 786 |
| Bitshuffle + Zstd 3 | 4.71 | 301 | 597 |
| Blosc lz4 5 + shuffle | 5.78 | 1596 | 1481 |
| Blosc zstd 5 + shuffle | 6.27 | 306 | 1549 |
| Blosc zstd 5 + bitshuffle | 5.70 | 227 | 1204 |
| Blosc2 zstd 5 + shuffle | 6.89 | 295 | 5079 |
| Blosc2 zstd 9 + shuffle | 7.16 | 10 | 5087 |
| kolmo 1 | 6.39 | 1148 | 3372 |
| kolmo 3 | 9.09 | 125 | 2971 |
| kolmo 5 | 9.34 | 20 | 2105 |

### synth_i16_wave (i16, 4.19 MB)

| filter | ratio | compress MB/s | decompress MB/s |
|---|---:|---:|---:|
| gzip 6 | 1.09 | 34 | 219 |
| shuffle + gzip 6 | 1.59 | 27 | 668 |
| hdf5plugin Zstd 3 | 1.08 | 1019 | 1239 |
| shuffle + Zstd 3 | 1.57 | 468 | 1254 |
| shuffle + Zstd 19 | 1.68 | 6 | 850 |
| LZ4 | 1.00 | 4517 | 17099 |
| Bitshuffle + LZ4 | 1.25 | 396 | 540 |
| Bitshuffle + Zstd 3 | 1.32 | 315 | 476 |
| Blosc lz4 5 + shuffle | 1.29 | 581 | 1871 |
| Blosc zstd 5 + shuffle | 1.58 | 62 | 1374 |
| Blosc zstd 5 + bitshuffle | 1.43 | 313 | 1140 |
| Blosc2 zstd 5 + shuffle | 1.56 | 60 | 1392 |
| Blosc2 zstd 9 + shuffle | 1.68 | 6 | 1516 |
| kolmo 1 | 1.00 | 3340 | 7117 |
| kolmo 3 | 1.75 | 58 | 687 |
| kolmo 5 | 1.85 | 5 | 504 |

