2765 samples
| # | function | share | owner |
|---:|---|---:|---|
| 1 | `syscall pwrite (libc.so.6, kernel time)` | 21.5% | system |
| 2 | `fi_inv_4_2_0` | 19.5% | Kolmo |
| 3 | `memcpy/memmove/memset (libc.so.6, stripped)` | 12.5% | system |
| 4 | `HUF_decodeSymbolX2` | 7.7% | vendored: zstd |
| 5 | `kg_unzz` | 7.4% | Kolmo |
| 6 | `syscall rename (libc.so.6, kernel time)` | 5.5% | system |
| 7 | `BIT_lookBitsFast` | 5.3% | vendored: zstd |
| 8 | `syscall pread (libc.so.6, kernel time)` | 5.1% | system |
| 9 | `XXH3_accumulate_512_sse2` | 4.8% | vendored: xxhash |
| 10 | `fi_inv_4_2_1` | 3.1% | Kolmo |
| 11 | `HUF_decompress4X2_usingDTable_internal_body` | 2.8% | vendored: zstd |
| 12 | `BIT_skipBits` | 1.2% | vendored: zstd |
| 13 | `BIT_reloadDStream_internal` | 0.7% | vendored: zstd |
| 14 | `BIT_reloadDStreamFast` | 0.5% | vendored: zstd |
| 15 | `syscall munmap (libc.so.6, kernel time)` | 0.5% | system |

top 15: 98.2% of samples; all Kolmo-owned code 30.2%, vendored 24.3%, system libraries 45.5%
