# Kolmo sample report: Backblaze Drive Stats, 2025-12-31.csv (one whole day)

Generated 2026-11-23 23:35 by `docs/outreach/sample_report.py`. Sample: 1 files, 133 MB (133,373,544 bytes).

## Summary

- Best Kolmo setting on this sample: **kolmo -9**, ratio **22.72** against 9.62 for gzip -6: Kolmo's output is **2.36x smaller** (58% less storage).
- Kolmo's default level (-3): ratio 18.03, compress 106 MiB/s, decompress 265 MiB/s on one thread.
- Every output of every setting was decompressed and compared byte for byte with its original: 7 round trips, 0 mismatches.
- Projected for 90 GB of this data at $0.023/GB-month: **$1/year** with kolmo -9 against $3/year as stored today (gzip -6), a saving of **$1/year**.

## Totals (whole sample)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 | 7,395,342 | 18.03 | 105.7 | 264.9 | verified |
| kolmo -6 | 6,608,926 | 20.18 | 26.3 | 306.0 | verified |
| kolmo -9 | 5,870,422 | 22.72 | 7.9 | 201.9 | verified |
| zstd -3 | 14,441,098 | 9.24 | 623.0 | 1571.3 | verified |
| zstd -19 | 9,804,971 | 13.60 | 1.9 | 1990.8 | verified |
| xz -9 | 10,157,012 | 13.13 | 2.8 | 134.9 | verified |
| gzip -6 | 13,868,862 | 9.62 | 64.6 | 1473.6 | verified |

## Projected storage cost

Stated volume: 90 GB uncompressed. Price: $0.023/GB-month (GB = 10^9 bytes), 12 months. Baseline: as stored today (gzip -6).

| Setting | Stored volume | Yearly cost | Saving vs baseline | Saving |
|---|---:|---:|---:|---:|
| uncompressed | 90 GB | $25 | -$22 | -862% |
| kolmo -3 | 4.99 GB | $1 | $1 | 47% |
| kolmo -6 | 4.46 GB | $1 | $1 | 52% |
| kolmo -9 | 3.96 GB | $1 | $1 | 58% |
| zstd -3 | 9.74 GB | $3 | -$0 | -4% |
| zstd -19 | 6.62 GB | $2 | $1 | 29% |
| xz -9 | 6.85 GB | $2 | $1 | 27% |
| gzip -6 | 9.36 GB | $3 | $0 | 0% |

The projection applies the sample's total ratio to the whole volume. It holds only if the sample is representative; storage requests, transfer and CPU cost are not included.

## Per file

Ratio per setting (the best per file in bold); speeds per file are in the JSON output.

| File | Size | kolmo -3 | kolmo -6 | kolmo -9 | zstd -3 | zstd -19 | xz -9 | gzip -6 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 2025-12-31.csv | 133 MB | 18.03 | 20.18 | **22.72** | 9.24 | 13.60 | 13.13 | 9.62 |

## Method

- Machine: Darwin 27.2.0, arm64, 10 cores.
- Tools: kolmo (kolmo 0.0.2 (container version 12, Kolmo-native codecs)); zstd (Zstandard CLI (64-bit) v1.5.7, by Yann Collet); xz (xz (XZ Utils) 5.8.4); gzip (Apple gzip 487.40.2).
- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file through stdin/stdout; best of 1 timed run(s). Speeds are original MiB over wall time, start-up and file I/O included.
- Every compressed output was decompressed by the same tool and compared byte for byte with the original; the run stops at the first mismatch or tool error.
- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 90GB --price 0.023 --current gzip-6 --gzip` in a checkout of github.com/adnaneben/kolmo.
