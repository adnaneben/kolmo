# Kolmo sample report: Loghub BGL.log (first 100 MB)

Generated 2026-11-23 23:38 by `docs/outreach/sample_report.py`. Sample: 1 files, 100 MB (99,999,904 bytes).

## Summary

- Best Kolmo setting on this sample: **kolmo -9**, ratio **33.59** against 12.36 for gzip -6: Kolmo's output is **2.72x smaller** (63% less storage).
- Kolmo's default level (-3): ratio 15.45, compress 196 MiB/s, decompress 757 MiB/s on one thread.
- Every output of every setting was decompressed and compared byte for byte with its original: 7 round trips, 0 mismatches.
- Projected for 89.1 GB of this data at $0.023/GB-month: **$1/year** with kolmo -9 against $2/year as stored today (gzip -6), a saving of **$1/year**.

## Totals (whole sample)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 | 6,472,277 | 15.45 | 195.7 | 756.6 | verified |
| kolmo -6 | 4,162,472 | 24.02 | 8.5 | 911.5 | verified |
| kolmo -9 | 2,976,898 | 33.59 | 2.9 | 307.9 | verified |
| zstd -3 | 9,087,488 | 11.00 | 510.5 | 1177.2 | verified |
| zstd -19 | 4,416,027 | 22.64 | 1.7 | 1568.6 | verified |
| xz -9 | 4,313,784 | 23.18 | 3.4 | 201.8 | verified |
| gzip -6 | 8,088,293 | 12.36 | 76.9 | 1891.7 | verified |

## Projected storage cost

Stated volume: 89.1 GB uncompressed. Price: $0.023/GB-month (GB = 10^9 bytes), 12 months. Baseline: as stored today (gzip -6).

| Setting | Stored volume | Yearly cost | Saving vs baseline | Saving |
|---|---:|---:|---:|---:|
| uncompressed | 89.1 GB | $25 | -$23 | -1136% |
| kolmo -3 | 5.77 GB | $2 | $0 | 20% |
| kolmo -6 | 3.71 GB | $1 | $1 | 49% |
| kolmo -9 | 2.65 GB | $1 | $1 | 63% |
| zstd -3 | 8.1 GB | $2 | -$0 | -12% |
| zstd -19 | 3.94 GB | $1 | $1 | 45% |
| xz -9 | 3.84 GB | $1 | $1 | 47% |
| gzip -6 | 7.21 GB | $2 | $0 | 0% |

The projection applies the sample's total ratio to the whole volume. It holds only if the sample is representative; storage requests, transfer and CPU cost are not included.

## Per file

Ratio per setting (the best per file in bold); speeds per file are in the JSON output.

| File | Size | kolmo -3 | kolmo -6 | kolmo -9 | zstd -3 | zstd -19 | xz -9 | gzip -6 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| BGL.log | 100 MB | 15.45 | 24.02 | **33.59** | 11.00 | 22.64 | 23.18 | 12.36 |

## Method

- Machine: Darwin 27.2.0, arm64, 10 cores.
- Tools: kolmo (kolmo 0.0.2 (container version 12, Kolmo-native codecs)); zstd (Zstandard CLI (64-bit) v1.5.7, by Yann Collet); xz (xz (XZ Utils) 5.8.4); gzip (Apple gzip 487.40.2).
- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file through stdin/stdout; best of 1 timed run(s). Speeds are original MiB over wall time, start-up and file I/O included.
- Every compressed output was decompressed by the same tool and compared byte for byte with the original; the run stops at the first mismatch or tool error.
- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 83GiB --price 0.023 --current gzip-6 --gzip` in a checkout of github.com/adnaneben/kolmo.
