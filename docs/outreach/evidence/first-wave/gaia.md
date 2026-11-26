# Kolmo sample report: ESA Gaia DR3 gaia_source (first 100 MB of GaiaSource_000000-003111.csv)

Generated 2026-11-23 23:30 by `docs/outreach/sample_report.py`. Sample: 1 files, 100 MB (99,999,474 bytes).

## Summary

- Best Kolmo setting on this sample: **kolmo -9**, ratio **3.78** against 2.87 for gzip -6: Kolmo's output is **1.32x smaller** (24% less storage).
- Kolmo's default level (-3): ratio 2.77, compress 52 MiB/s, decompress 458 MiB/s on one thread.
- Every output of every setting was decompressed and compared byte for byte with its original: 7 round trips, 0 mismatches.
- Projected for 2.17 TB of this data at $0.023/GB-month: **$159/year** with kolmo -9 against $209/year as stored today (gzip -6), a saving of **$50/year**.

## Totals (whole sample)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 | 36,094,946 | 2.77 | 52.0 | 458.5 | verified |
| kolmo -6 | 30,352,305 | 3.29 | 1.6 | 656.6 | verified |
| kolmo -9 | 26,457,192 | 3.78 | 19.3 | 20.7 | verified |
| zstd -3 | 36,135,965 | 2.77 | 196.5 | 721.6 | verified |
| zstd -19 | 29,882,096 | 3.35 | 1.3 | 910.6 | verified |
| xz -9 | 29,615,408 | 3.38 | 1.1 | 62.7 | verified |
| gzip -6 | 34,835,494 | 2.87 | 31.0 | 650.0 | verified |

## Projected storage cost

Stated volume: 2.17 TB uncompressed (derived from 757 GB stored as gzip -6). Price: $0.023/GB-month (GB = 10^9 bytes), 12 months. Baseline: as stored today (gzip -6).

| Setting | Stored volume | Yearly cost | Saving vs baseline | Saving |
|---|---:|---:|---:|---:|
| uncompressed | 2.17 TB | $600 | -$391 | -187% |
| kolmo -3 | 784 GB | $216 | -$8 | -4% |
| kolmo -6 | 660 GB | $182 | $27 | 13% |
| kolmo -9 | 575 GB | $159 | $50 | 24% |
| zstd -3 | 785 GB | $217 | -$8 | -4% |
| zstd -19 | 649 GB | $179 | $30 | 14% |
| xz -9 | 644 GB | $178 | $31 | 15% |
| gzip -6 | 757 GB | $209 | $0 | 0% |

The projection applies the sample's total ratio to the whole volume. It holds only if the sample is representative; storage requests, transfer and CPU cost are not included.

## Per file

Ratio per setting (the best per file in bold); speeds per file are in the JSON output.

| File | Size | kolmo -3 | kolmo -6 | kolmo -9 | zstd -3 | zstd -19 | xz -9 | gzip -6 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| GaiaSource_000000-003111.csv | 100 MB | 2.77 | 3.29 | **3.78** | 2.77 | 3.35 | 3.38 | 2.87 |

## Method

- Machine: Darwin 27.2.0, arm64, 10 cores.
- Tools: kolmo (kolmo 0.0.2 (container version 12, Kolmo-native codecs)); zstd (Zstandard CLI (64-bit) v1.5.7, by Yann Collet); xz (xz (XZ Utils) 5.8.4); gzip (Apple gzip 487.40.2).
- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file through stdin/stdout; best of 1 timed run(s). Speeds are original MiB over wall time, start-up and file I/O included.
- Every compressed output was decompressed by the same tool and compared byte for byte with the original; the run stops at the first mismatch or tool error.
- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 757GB --price 0.023 --current gzip-6 --volume-is current --gzip` in a checkout of github.com/adnaneben/kolmo.
