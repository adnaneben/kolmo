# First-wave candidates: Kolmo on their own public files

Measured 2026-11-23 with `docs/outreach/sample_report.py`, before any contact. Nobody was contacted.

- **Setup:** Kolmo 0.0.2, Apple M5, every tool on one thread as a command-line process. Every output was decompressed
  and compared byte for byte: 35 of 35 round trips matched. One timed run per setting; sizes do not vary between runs.
- **Samples:** files downloaded from each publisher's canonical location. Each `*.source.json` records:
  - the URL and the sha256 of the download;
  - the file's full uncompressed size;
  - the sha256 of the sample.
- **Sample size:** the first 100 MB of uncompressed data, cut at a line end. Backblaze is one whole day's file.
- **Reproduce:** fetch the URL in a `*.source.json`, keep its first 100,000,000 bytes, cut at the last newline, and run
  `sample_report.py` on that file.

## Results

Each row is a ratio (original size / compressed size; bigger is better). "As published" is the ratio of the
publisher's own whole file, for comparison with the measured gzip or bzip2 row.

| Candidate, sample | As published | gzip -6 or bzip2 -9 | zstd -3 | zstd -19 | xz -9 | kolmo -3 | kolmo -6 | kolmo -9 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| [NOAA GHCN-Daily](ghcn-by-year.md), `by_year/2024.csv` | 7.87 (gzip) | 7.65 | 6.99 | 11.85 | 13.26 | **13.53** | **22.29** | **28.99** |
| [Loghub](loghub-bgl.md), `BGL.log` | 12.93 (zip) | 12.36 | 11.00 | 22.64 | 23.18 | 15.45 | 24.02 | **33.59** |
| [Backblaze Drive Stats](backblaze.md), `2025-12-31.csv` (whole day) | 10.49 (zip) | 9.62 | 9.24 | 13.60 | 13.13 | **18.03** | **20.18** | **22.72** |
| [ESA Gaia DR3](gaia.md), `gaia_source` ECSV | 2.87 (gzip) | 2.87 | 2.77 | 3.35 | 3.38 | 2.77 | 3.29 | 3.78 |
| [Wikimedia pageviews](wikimedia.md), 2025-01-15 automated | 5.50 (bzip2) | 5.85 (bzip2) | 4.28 | 5.59 | 6.02 | 4.29 | 5.40 | 6.57 |

Speeds, one thread, MiB/s (compress / decompress):

| Candidate | kolmo -3 | kolmo -6 | kolmo -9 | zstd -19 | xz -9 | gzip -6 / bzip2 -9 |
|---|---|---|---|---|---|---|
| GHCN-Daily | 197 / 521 | 13 / 724 | 5.3 / 330 | 3.0 / 1237 | 2.4 / 152 | 40 / 400 |
| Loghub BGL | 196 / 757 | 8.5 / 912 | 2.9 / 308 | 1.7 / 1569 | 3.4 / 202 | 77 / 1892 |
| Backblaze | 106 / 265 | 26 / 306 | 7.9 / 202 | 1.9 / 1991 | 2.8 / 135 | 65 / 1474 |
| Gaia | 52 / 459 | 1.6 / 657 | 19 / 21 | 1.3 / 911 | 1.1 / 63 | 31 / 650 |
| Wikimedia | 167 / 792 | 4.0 / 904 | 18 / 36 | 1.7 / 1153 | 2.0 / 72 | 13 / 41 (bzip2) |

## What it means for each candidate

- **GHCN-Daily: strongest case.**
  - Kolmo's default level -3 is 1.8x smaller than gzip -6 and as small as xz -9, at 197 MiB/s.
  - Level 6 is 2.9x smaller than gzip -6 and 1.9x smaller than zstd -19.
  - Level 9 is 3.8x smaller than gzip -6 and 2.2x smaller than xz -9, decoding about 2x faster than xz.
  - Kolmo -3 also decodes faster than gzip on this file (521 against 400 MiB/s).
- **Backblaze Drive Stats: strong, even at the default level.**
  - Level 3 is 1.9x smaller than gzip -6 and 1.3x smaller than zstd -19, compressing 55x faster than zstd -19.
  - Level 9 is 2.2x smaller than Backblaze's own zip.
  - The cost: Kolmo decodes this wide, sparse CSV at only 200-300 MiB/s.
- **Loghub BGL: strong at levels 6 and 9.**
  - Level 6 matches xz -9's size (+4%) and decodes 4.5x faster.
  - Level 9 is 2.7x smaller than gzip -6 and 1.45x smaller than xz -9.
  - Level 3 is 1.4x smaller than zstd -3.
- **Gaia: weak.**
  - Level 3 ties zstd -3, and level 6 ties zstd -19 while compressing very slowly (1.6 MiB/s).
  - Only level 9 wins: 1.3x smaller than gzip and 1.1x smaller than xz -9. It decodes at 21 MiB/s, slower than gzip.
  - The ECSV rows are long, high-entropy floats, mostly with no decimal structure. **Demoted to Medium.**
- **Wikimedia pageviews: weak.**
  - Level 6 is 8% larger than their bzip2 (but decodes 22x faster).
  - Level 9 is 11% smaller than bzip2, with no decode advantage (36 against 41 MiB/s).
  - The data is mostly page titles, i.e. text. **Demoted to Low.**

**Storage dollars are small for these public volumes**: Loghub is 83 GiB and Backblaze about 90 GB, so savings are a
few dollars a year at S3 prices. For open-data publishers the pitch is smaller downloads and mirrors, plus the
published comparison. For a partner who stores 100 TB of similar data, the same ratios become real money; the
per-report projections in this folder show the arithmetic.

## Notes

- The projections in the reports use public volumes:
  - Gaia: 757 GB as stored.
  - Wikimedia: ~300 GB a year as stored, derived from the daily file sizes.
  - Loghub: 83 GiB raw.
  - Backblaze: 90 GB raw.
- Each projection applies one sample's ratio to the whole volume, so it is approximate.
- The measured gzip -6 is within 5% of the publisher's own whole-file ratio for GHCN, Gaia and Loghub, so the samples
  are a fair stand-in for what they ship. Backblaze's zip (10.49) is 9% better than gzip -6 (9.62); use the published
  figure when writing to them.
