# Kolmo sample report: Wikimedia pageview_complete, 2025-01-15 automated (first 100 MB)

Generated 2026-11-23 23:41 by `docs/outreach/sample_report.py`. Sample: 1 files, 100 MB (99,999,958 bytes).

## Summary

- Best Kolmo setting on this sample: **kolmo -9**, ratio **6.57** against 5.85 for bzip2 -9: Kolmo's output is **1.12x smaller** (11% less storage).
- Kolmo's default level (-3): ratio 4.29, compress 167 MiB/s, decompress 792 MiB/s on one thread.
- Every output of every setting was decompressed and compared byte for byte with its original: 7 round trips, 0 mismatches.
- Projected for 1.76 TB of this data at $0.023/GB-month: **$74/year** with kolmo -9 against $83/year as stored today (bzip2 -9), a saving of **$9/year**.

## Totals (whole sample)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 | 23,331,852 | 4.29 | 166.9 | 792.1 | verified |
| kolmo -6 | 18,523,051 | 5.40 | 4.0 | 903.6 | verified |
| kolmo -9 | 15,226,021 | 6.57 | 18.1 | 36.1 | verified |
| zstd -3 | 23,364,607 | 4.28 | 335.0 | 906.1 | verified |
| zstd -19 | 17,901,630 | 5.59 | 1.7 | 1153.4 | verified |
| xz -9 | 16,604,524 | 6.02 | 2.0 | 71.5 | verified |
| bzip2 -9 | 17,083,263 | 5.85 | 12.8 | 40.6 | verified |

## Projected storage cost

Stated volume: 1.76 TB uncompressed (derived from 300 GB stored as bzip2 -9). Price: $0.023/GB-month (GB = 10^9 bytes), 12 months. Baseline: as stored today (bzip2 -9).

| Setting | Stored volume | Yearly cost | Saving vs baseline | Saving |
|---|---:|---:|---:|---:|
| uncompressed | 1.76 TB | $485 | -$402 | -485% |
| kolmo -3 | 410 GB | $113 | -$30 | -37% |
| kolmo -6 | 325 GB | $90 | -$7 | -8% |
| kolmo -9 | 267 GB | $74 | $9 | 11% |
| zstd -3 | 410 GB | $113 | -$30 | -37% |
| zstd -19 | 314 GB | $87 | -$4 | -5% |
| xz -9 | 292 GB | $80 | $2 | 3% |
| bzip2 -9 | 300 GB | $83 | $0 | 0% |

The projection applies the sample's total ratio to the whole volume. It holds only if the sample is representative; storage requests, transfer and CPU cost are not included.

## Per file

Ratio per setting (the best per file in bold); speeds per file are in the JSON output.

| File | Size | kolmo -3 | kolmo -6 | kolmo -9 | zstd -3 | zstd -19 | xz -9 | bzip2 -9 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| pageviews-20250115-automated | 100 MB | 4.29 | 5.40 | **6.57** | 4.28 | 5.59 | 6.02 | 5.85 |

## Method

- Machine: Darwin 27.2.0, arm64, 10 cores.
- Tools: kolmo (kolmo 0.0.2 (container version 12, Kolmo-native codecs)); zstd (Zstandard CLI (64-bit) v1.5.7, by Yann Collet); xz (xz (XZ Utils) 5.8.4); bzip2 (bzip2, a block-sorting file compressor.  Version 1.0.8, 13-Jul-2019.).
- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file through stdin/stdout; best of 1 timed run(s). Speeds are original MiB over wall time, start-up and file I/O included.
- Every compressed output was decompressed by the same tool and compared byte for byte with the original; the run stops at the first mismatch or tool error.
- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 300GB --price 0.023 --current bzip2-9 --volume-is current --bzip2` in a checkout of github.com/adnaneben/kolmo.
