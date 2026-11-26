# Claims the outreach may make, and their method

Every number in an outreach message must come from this page or from a `sample_report.py` run on the recipient's own
data, quoted with its qualifiers (machine, one thread, which data). If a claim is not here, do not make it.

All runs: Kolmo 0.0.2, Apple M5 (arm64, 10 cores), macOS. **x86-64 confirmation is pending** (an x86 lzbench run is
in progress); until it lands, say "on an Apple M5" when quoting a speed. Sizes are the same on every machine (Kolmo's
output is byte-identical on x86 and Apple silicon; README, "How it works").

## Claim 1: level 6 is smaller than `xz -9` and decodes 8.6x faster (Silesia, one thread)

| Setting | Compressed bytes | Ratio | Compress MiB/s | Decompress MiB/s |
|---|---:|---:|---:|---:|
| kolmo 0.0.2 -6 | 48,535,166 | 4.367 | 8.8 | **1463** |
| xz 5.8.4 -9 | 48,766,532 | 4.346 | 5.1 | 169 |
| zstd 1.5.7 -19 | 52,834,954 | 4.012 | 6.0 | 2190 |

- **Say:** "On the Silesia corpus, Kolmo -6 is slightly smaller than xz -9 and decompresses 8.6x faster, one thread,
  Apple M5." The size gain over xz is small (0.5%); the claim is the decode speed at xz's size.
- **Method:** lzbench 2.4.1, in-memory, one thread, Silesia as one tar (211,947,520 bytes); Kolmo through
  `benchmark/lzbench/` (a plugin calling libkolmo; its sizes match the CLI byte for byte). All codecs built with
  lzbench's flags.
- **Raw data:** [`evidence/lzb-slow-silesia.tar.csv`](evidence/lzb-slow-silesia.tar.csv) (and
  [`lzb-fast-silesia.tar.csv`](evidence/lzb-fast-silesia.tar.csv) for levels 1-3 against zstd -1/-3).

## Claim 2: on structured files, Kolmo's default level beats zstd -3 by 1.4-3.4x with no configuration

Ratios from the published panel, one thread, CLI processes file to file, best of 3, every round trip verified:

| File | Data | zstd -3 | xz -9 | kolmo -3 (default) | kolmo -5 | kolmo -3 vs zstd -3 |
|---|---|---:|---:|---:|---:|---:|
| owid-co2-data.csv | CSV, 14 MB | 3.17 | 4.62 | 10.75 | 11.53 | **3.4x smaller** |
| energy_test.csv | CSV, 7 MB | 3.34 | 4.28 | 9.51 | 10.01 | **2.8x** |
| wide_test.csv | CSV, `;`, 316 columns | 3.97 | 5.79 | 7.15 | 8.41 | 1.8x |
| log_timestamps_ms.i64 | int64 timestamps | 9.35 | 10.56 | 24.16 | 24.75 | **2.6x** |
| population.i64 | int64 column | 2.33 | 2.75 | 3.83 | 3.93 | 1.6x |
| owid_energy.pkl | pandas pickle, 24 MB | 8.64 | 12.10 | 15.77 | 16.88 | 1.8x |
| owid_energy_f64.npy | NumPy float64, 24 MB | 8.65 | 12.84 | 15.59 | 16.45 | 1.8x |
| loghub_all.log | logs, 4.5 MB | 9.87 | 13.78 | 13.66 | 15.77 | 1.4x |

- **Say:** "1.4-3.4x smaller than zstd -3 at Kolmo's default level on CSV, pickles, integer columns and logs", or quote
  one row. **Do not say "2-4x" for logs, pickles or integers**: only the CSVs and the timestamps reach 2x at level 3.
- **Caveat to keep:** these files come from data used during Kolmo's development (tuning bias possible). Prefer
  claim 3, which uses data Kolmo was never tuned on.
- **Method and raw data:** [`evidence/openzl_panel.py`](evidence/openzl_panel.py) (the script, paths made
  configurable) and [`evidence/openzl-panel-0.0.2.json`](evidence/openzl-panel-0.0.2.json) (every measurement,
  including OpenZL 0.3.0, untrained).

## Claim 3: public data Kolmo was never tuned on (the sample report run on four public sources)

`sample_report.py` on five public files, one thread, CLI processes, every round trip verified (35 of 35). Report:
[`evidence/holdout-sample-report.md`](evidence/holdout-sample-report.md), raw numbers in the JSON beside it.

| File (source) | gzip -6 | zstd -3 | zstd -19 | xz -9 | kolmo -3 | kolmo -6 | kolmo -9 |
|---|---:|---:|---:|---:|---:|---:|---:|
| GHCN-Daily USW00094728.csv, 17.8 MB ([NOAA NCEI](https://www.ncei.noaa.gov/data/global-historical-climatology-network-daily/access/USW00094728.csv)) | 17.92 | 15.13 | 27.01 | 26.08 | 26.80 | 38.22 | **46.20** |
| GHCN-Daily USW00023174.csv, 10.9 MB ([NOAA NCEI](https://www.ncei.noaa.gov/data/global-historical-climatology-network-daily/access/USW00023174.csv)) | 17.01 | 13.95 | 25.50 | 23.13 | 22.66 | 35.87 | **43.18** |
| NASA HTTP log, Jul 1995, first 30 MB ([Internet Traffic Archive](https://ita.ee.lbl.gov/html/contrib/NASA-HTTP.html)) | 9.85 | 9.67 | 15.60 | 15.01 | 10.13 | 15.29 | **17.90** |
| GH Archive 2015-01-01 hour 12, JSON lines, 18.4 MB ([gharchive.org](https://data.gharchive.org/2015-01-01-12.json.gz)) | 6.88 | 8.59 | 11.74 | 11.95 | 8.81 | 11.81 | **13.19** |
| NCEP reanalysis air.sig995.2022.nc, 22 MB ([NOAA PSL](https://downloads.psl.noaa.gov/Datasets/ncep.reanalysis/surface/air.sig995.2022.nc)) | 1.01 | 1.00 | 1.01 | 1.01 | 1.01 | 1.01 | 1.01 |

One-thread speeds on the GHCN files (MiB/s, compress / decompress, CLI process): kolmo -6 14-17 / 414-482,
kolmo -9 9 / 316-327, zstd -19 2.7 / 988-1880, xz -9 7-8 / 300-352, gzip -6 136 / 1176-1282.

- **Say (CSV, against what open-data portals ship):** "On NOAA's GHCN-Daily station CSVs, Kolmo -6 is 2.1x smaller
  than gzip -6 and 1.4x smaller than zstd -19, and compresses 5-6x faster than zstd -19; Kolmo -9 is 2.5-2.6x smaller
  than gzip -6 and 1.8x smaller than xz -9, decoding at about xz's speed."
- **Say (logs):** "On the NASA HTTP logs, Kolmo -9 is 1.8x smaller than gzip -6 and 1.2x smaller than xz -9." At level 6
  Kolmo only ties zstd -19 and xz -9 in size (it decodes 2x faster than xz -9); at level 3 it ties zstd -3. **Do not
  claim a large log gain at the default level.**
- **JSON:** level 3 ties zstd -3; levels 6 and 9 tie or beat zstd -19 and xz -9 by up to 12%. Not a lead claim.
- **NetCDF-4 / HDF5 files compressed inside** (the NCEP file is HDF5 with deflate) do not shrink with any tool. Kolmo
  helps there only as the HDF5 filter or Zarr codec, on uncompressed chunks. Ask partners for uncompressed arrays, or
  measure through the filter.

## Claim 3b: the first-wave candidates' own public files

`sample_report.py` on 100 MB samples of five candidates' published files (Backblaze: one whole day), one thread, 35 of
35 round trips verified. Summary, per-candidate reports and download provenance:
[`evidence/first-wave/`](evidence/first-wave/README.md).

| Candidate file | As published | kolmo -3 | kolmo -6 | kolmo -9 | Say |
|---|---:|---:|---:|---:|---|
| NOAA GHCN-Daily `by_year/2024.csv` | 7.87 (gzip) | 13.53 | 22.29 | 28.99 | "Kolmo's default level is 1.8x smaller than gzip -6 on your 2024 file; level 6 is 2.9x smaller" |
| Backblaze Drive Stats `2025-12-31.csv` | 10.49 (zip) | 18.03 | 20.18 | 22.72 | "the default level is 1.7x smaller than your zip, 1.3x smaller than zstd -19" |
| Loghub `BGL.log` | 12.93 (zip) | 15.45 | 24.02 | 33.59 | "level 6 is as small as xz -9 and decodes 4.5x faster; level 9 is 2.6x smaller than your zip" |
| ESA Gaia DR3 `gaia_source` ECSV | 2.87 (gzip) | 2.77 | 3.29 | 3.78 | no lead claim: only level 9 wins (1.3x over gzip), decoding at 21 MiB/s |
| Wikimedia pageviews (automated) | 5.50 (bzip2) | 4.29 | 5.40 | 6.57 | no lead claim: level 9 is 11% smaller than bzip2 with no decode advantage |

"As published" is the publisher's whole-file ratio; the Kolmo columns are on the sample. Compare against it only as a
guide, and quote the measured gzip -6 / bzip2 -9 row from the report when exactness matters.

## Claim 4: on many cores Kolmo's decoder scales and zstd's does not (Silesia, CLI)

| Setting | Decompress MiB/s, 1 thread | all 10 cores |
|---|---:|---:|
| kolmo -3 | 1262 | 3792 |
| zstd -3 | 1411 | 1405 |
| kolmo -7 | 1175 | 2771 |

- **Say:** "On 10 cores Kolmo -3 decompresses about 2.7x faster than zstd -3 (Apple M5, Silesia)." Always pair it with
  the one-thread fact: on one thread Kolmo decodes slower than zstd at levels 2-7. Memory rises with threads (kolmo -3:
  171 MB to decode on 10 cores, zstd 8 MB).
- **Raw data:** [`evidence/multithread-silesia-0.0.2.json`](evidence/multithread-silesia-0.0.2.json) (best of 3, RSS
  recorded).

## Claim 5: trust facts

- Container version 12 is specified in [`FORMAT.md`](../../FORMAT.md); [`COMPATIBILITY.md`](../../COMPATIBILITY.md)
  promises every later release reads every version-12 frame written by 0.0.1 or later; a frozen corpus of 0.0.2
  frames is checked in CI.
- Every block carries a checksum; corrupt input is reported, never returned as data (README).
- Licence: Apache-2.0. Install: `brew install adnaneben/kolmo/kolmo`, `pip install kolmoc`.

Pending in open pull requests (name them as "in review", never as released): numcodecs/Zarr codec (#21), HDF5 filter
and Blosc2 plugin (#23), continuous fuzzing and signed releases (#19).

## Claims to avoid

- "Faster than zstd" (zstd compresses faster at every level and decodes faster on one thread at levels 2-7).
- "Best for LLM / ML weights" (every tool lands at about the same ratio; panel rows magika.*, llm_bf16_*).
- "Beats OpenZL" (OpenZL ran untrained; a trained rematch is pending).
- Anything about small files (on Canterbury, Kolmo -3 is 4% larger than zstd -3).
- "2-4x smaller than zstd" as a blanket statement: it holds for some CSVs and timestamp columns, not for logs or JSON
  at the default level (claims 2 and 3).
- Any production-use claim: there are no published production users yet.
