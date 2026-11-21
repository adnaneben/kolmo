4279 samples
| # | function | share | owner |
|---:|---|---:|---|
| 1 | `syscall pwrite (libc.so.6, kernel time)` | 15.6% | system |
| 2 | `ZSTD_decodeSequence` | 13.0% | vendored: zstd |
| 3 | `ZSTD_wildcopy` | 9.1% | vendored: zstd |
| 4 | `img16_inv` | 7.7% | Kolmo |
| 5 | `lines_decode` | 6.1% | Kolmo |
| 6 | `_HUF_decompress4X1_usingDTable_internal_fast_asm_loop` | 5.3% | vendored: zstd |
| 7 | `BIT_reloadDStream_internal` | 3.9% | vendored: zstd |
| 8 | `ZSTD_execSequence` | 3.8% | vendored: zstd |
| 9 | `BIT_getMiddleBits` | 3.3% | vendored: zstd |
| 10 | `syscall pread (libc.so.6, kernel time)` | 3.3% | system |
| 11 | `HUF_decompress4X2_usingDTable_internal_fast_asm_loop` | 3.0% | vendored: zstd |
| 12 | `XXH3_accumulate_512_sse2` | 2.9% | vendored: xxhash |
| 13 | `ZSTD_copy16` | 2.8% | vendored: zstd |
| 14 | `ZSTD_buildFSETable_body` | 2.3% | vendored: zstd |
| 15 | `ZSTD_updateFseStateWithDInfo` | 2.1% | vendored: zstd |

top 15: 84.1% of samples; all Kolmo-owned code 16.9%, vendored 61.6%, system libraries 21.5%
