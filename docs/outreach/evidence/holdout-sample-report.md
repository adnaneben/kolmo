# Kolmo sample report: public holdout sample (NOAA GHCN-Daily, NASA HTTP logs, NCEP NetCDF, GH Archive)

Generated 2026-11-23 23:12 by `docs/outreach/sample_report.py`. Sample: 5 files, 99.2 MB (99,161,876 bytes).

## Summary

- Best Kolmo setting on this sample: **kolmo -9**, ratio **3.87** against 3.39 for gzip -6: Kolmo's output is **1.14x smaller** (12% less storage).
- Kolmo's default level (-3): ratio 3.53, compress 231 MiB/s, decompress 583 MiB/s on one thread.
- Every output of every setting was decompressed and compared byte for byte with its original: 35 round trips, 0 mismatches.
- Projected for 40 TB of this data at $0.023/GB-month: **$2,853/year** with kolmo -9 against $3,256/year as stored today (gzip -6), a saving of **$403/year**.

## Totals (whole sample)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 | 28,060,989 | 3.53 | 230.9 | 582.8 | verified |
| kolmo -6 | 26,044,135 | 3.81 | 3.6 | 619.0 | verified |
| kolmo -9 | 25,622,163 | 3.87 | 7.4 | 53.0 | verified |
| zstd -3 | 29,147,850 | 3.40 | 510.2 | 1121.4 | verified |
| zstd -19 | 26,340,165 | 3.76 | 2.1 | 1141.3 | verified |
| xz -9 | 26,603,672 | 3.73 | 2.5 | 110.9 | verified |
| gzip -6 | 29,242,034 | 3.39 | 92.2 | 736.6 | verified |

## Projected storage cost

Stated volume: 40 TB uncompressed. Price: $0.023/GB-month (GB = 10^9 bytes), 12 months. Baseline: as stored today (gzip -6).

| Setting | Stored volume | Yearly cost | Saving vs baseline | Saving |
|---|---:|---:|---:|---:|
| uncompressed | 40 TB | $11,040 | -$7,784 | -239% |
| kolmo -3 | 11.3 TB | $3,124 | $131 | 4% |
| kolmo -6 | 10.5 TB | $2,900 | $356 | 11% |
| kolmo -9 | 10.3 TB | $2,853 | $403 | 12% |
| zstd -3 | 11.8 TB | $3,245 | $10 | 0% |
| zstd -19 | 10.6 TB | $2,933 | $323 | 10% |
| xz -9 | 10.7 TB | $2,962 | $294 | 9% |
| gzip -6 | 11.8 TB | $3,256 | $0 | 0% |

The projection applies the sample's total ratio to the whole volume. It holds only if the sample is representative; storage requests, transfer and CPU cost are not included.

## Per file

Ratio per setting (the best per file in bold); speeds per file are in the JSON output.

| File | Size | kolmo -3 | kolmo -6 | kolmo -9 | zstd -3 | zstd -19 | xz -9 | gzip -6 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| csv/ghcn-USW00023174-USW00023174.csv | 10.9 MB | 22.66 | 35.87 | **43.18** | 13.95 | 25.50 | 23.13 | 17.01 |
| csv/ghcn-USW00094728-USW00094728.csv | 17.8 MB | 26.80 | 38.22 | **46.20** | 15.13 | 27.01 | 26.08 | 17.92 |
| json/gharchive-2015-01-01-12.json | 18.4 MB | 8.81 | 11.81 | **13.19** | 8.59 | 11.74 | 11.95 | 6.88 |
| logs/NASA_access_log_Jul95.part | 30 MB | 10.13 | 15.29 | **17.90** | 9.67 | 15.60 | 15.01 | 9.85 |
| netcdf/ncep-air-2022-air.sig995.2022.nc | 22 MB | 1.01 | **1.01** | 1.01 | 1.00 | 1.01 | 1.01 | 1.01 |

## Method

- Machine: Darwin 27.2.0, arm64, 10 cores.
- Tools: kolmo (kolmo 0.0.2 (container version 12, Kolmo-native codecs)); zstd (Zstandard CLI (64-bit) v1.5.7, by Yann Collet); xz (xz (XZ Utils) 5.8.4); gzip (Apple gzip 487.40.2).
- Each tool ran as a command-line process on one thread (kolmo `-T 1`, zstd and xz `-T1`), file to file through stdin/stdout; best of 1 timed run(s). Speeds are original MiB over wall time, start-up and file I/O included.
- Every compressed output was decompressed by the same tool and compared byte for byte with the original; the run stops at the first mismatch or tool error.
- Reproduce: `python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 40TB --price 0.023 --current gzip-6 --gzip` in a checkout of github.com/adnaneben/kolmo.
