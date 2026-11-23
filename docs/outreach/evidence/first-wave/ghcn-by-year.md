# Kolmo sample report: NOAA GHCN-Daily by_year/2024.csv (first 100 MB)

Generated 2026-11-23 23:34 by `docs/outreach/sample_report.py`. Sample: 1 files, 100 MB (99,999,985 bytes).

## Summary

- Best Kolmo setting on this sample: **kolmo -9**, ratio **28.99** against 7.65 for gzip -6: Kolmo's output is **3.79x smaller** (74% less storage).
- Kolmo's default level (-3): ratio 13.53, compress 197 MiB/s, decompress 521 MiB/s on one thread.
- Every output of every setting was decompressed and compared byte for byte with its original: 7 round trips, 0 mismatches.

## Totals (whole sample)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 | 7,393,105 | 13.53 | 197.4 | 521.0 | verified |
| kolmo -6 | 4,485,967 | 22.29 | 13.0 | 723.5 | verified |
| kolmo -9 | 3,449,081 | 28.99 | 5.3 | 329.9 | verified |
| zstd -3 | 14,306,057 | 6.99 | 429.0 | 964.9 | verified |
| zstd -19 | 8,436,504 | 11.85 | 3.0 | 1236.5 | verified |
| xz -9 | 7,539,244 | 13.26 | 2.4 | 152.3 | verified |
| gzip -6 | 13,063,589 | 7.65 | 39.6 | 399.7 | verified |

## Per file

Ratio per setting (the best per file in bold); speeds per file are in the JSON output.

| File | Size | kolmo -3 | kolmo -6 | kolmo -9 | zstd -3 | zstd -19 | xz -9 | gzip -6 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 2024.csv | 100 MB | 13.53 | 22.29 | **28.99** | 6.99 | 11.85 | 13.26 | 7.65 |

## Method

- Machine: Darwin 27.2.0, arm64, 10 cores.
- Tools: kolmo (kolmo 0.0.2 (container version 12, Kolmo-native codecs)); zstd (Zstandard CLI (64-bit) v1.5.7, by Yann Collet); xz (xz (XZ Utils) 5.8.4); gzip (Apple gzip 487.40.2).
- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file through stdin/stdout; best of 1 timed run(s). Speeds are original MiB over wall time, start-up and file I/O included.
- Every compressed output was decompressed by the same tool and compared byte for byte with the original; the run stops at the first mismatch or tool error.
- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR --current gzip-6 --gzip` in a checkout of github.com/adnaneben/kolmo.
