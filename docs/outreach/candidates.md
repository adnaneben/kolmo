# Design-partner candidates

20 organisations and open projects that publicly store large volumes of the data Kolmo is best at, plus 4 channels
(registries and hosts) that lead to many such owners. Researched from public pages on 2026-11-23. **Nobody has been
contacted.**

How to read it:

- **Sources.** Each size or format comes with a link. **[V]** means the linked page was opened and states the fact.
  **[S]** means it was seen only in a search-result summary; re-check the page before quoting it. **[NV]** means it is
  not verified.
- **Contact routes are public only:** issue trackers, Discussions, forums, public mailing lists and help desks. No
  personal addresses. Read each project's contribution rules before posting (`message-templates.md`).
- **Fit** uses only `CLAIMS.md`.
  - CSV gains are proven on held-out data: on NOAA GHCN-Daily, Kolmo -6 is 2.1x smaller than gzip -6.
  - Log gains appear at levels 6-9: on NASA logs, Kolmo -9 is 1.8x smaller than gzip -6.
  - JSON gains are modest.
  - Files already compressed inside (NetCDF-4 with deflate, Parquet) gain nothing unless Kolmo replaces the inner
    codec.
  - Model weights, small files and image data are not claimed.
- **Integration routes:**
  - released: CLI and `tar -I kolmo`, Python `kolmoc`;
  - in review: numcodecs/Zarr (PR #21), HDF5 filter and Blosc2 (PR #23).
- **Priority** weighs data fit, volume, openness to a new codec, and how reachable a public route is.

## Summary

| # | Candidate | Data | Volume (source) | Route | Priority |
|---|---|---|---|---|---|
| 1 | NOAA NCEI GHCN-Daily | gzip CSV | ~175 MB gzip per recent year file, 1763-2026 [V] | CLI, `tar -I` | **High** (measured) |
| 2 | ESA Gaia Archive DR3 | gzip ECSV | gaia_source 757 GB compressed, 1.81 bn rows [V] | CLI, kolmoc | Medium (measured: weak) |
| 3 | GH Archive | gzip JSON lines | hourly since 2011; total not stated [V] | CLI | **High** |
| 4 | Loghub (logpai) | raw logs, zip / tar.gz | ~83 GiB raw [V] | CLI, `tar -I` | **High** (measured) |
| 5 | Pangeo CMIP6 cloud | Zarr, NetCDF | ~100 TB in cloud (2019) [S] | numcodecs/Zarr | **High** |
| 6 | Wikimedia pageview dumps | bz2 text lines | ~0.9 GB/day compressed [V] | CLI | Low (measured: weak) |
| 7 | Backblaze Drive Stats | zipped CSV | >90 GB raw CSV, ~300 M rows [V] | CLI, kolmoc | **High** (measured) |
| 8 | ARCO-ERA5 | Zarr | ~5 PB; one set 53.2 TB [V] | numcodecs/Zarr | Medium-High |
| 9 | Source Cooperative / data.gov archive | mixed, much CSV | >5 PB; data.gov archive 16 TB [V] | CLI, Zarr | Medium |
| 10 | scverse / AnnData | Zarr, HDF5 (h5ad) | user-held | numcodecs, HDF5 filter | Medium |
| 11 | OME-Zarr / IDR | Zarr uint16 images | ~500 TB in 2024 challenge [V] | numcodecs/Zarr | Medium |
| 12 | NASA EOSDIS / Earthdata | HDF5, NetCDF-4 | 225.7 PB archive [V] | HDF5 filter | Medium |
| 13 | ECMWF / Copernicus CDS | NetCDF-4 | same data as #8 | HDF5 filter | Medium |
| 14 | Vector (log pipeline) | S3 log archives | user-held | Rust crate, sink option | Medium |
| 15 | OpenAQ archive | gzip CSV per location-day | not stated [V] | CLI, kolmoc | Medium |
| 16 | Grafana Loki | log chunks | user-held | CLI for archives only | Low-Medium |
| 17 | Common Crawl indexes | CDXJ gz, Parquet zstd | index ~0.22 TiB per crawl [V] | CLI on CDXJ | Low-Medium |
| 18 | US Census ACS PUMS | zipped CSV | 283 MB per state zip (2016) [S] | CLI | Low-Medium |
| 19 | Eurostat bulk download | gzip TSV / SDMX-CSV | not stated | CLI | Low |
| 20 | NYC TLC trip records | Parquet | monthly since 2009 [V] | none today | Low |
| C1 | Zarr / numcodecs (zarr-extensions) | codec registry | — | numcodecs/Zarr | channel, **High** |
| C2 | HDF Group filter registry | filter IDs | — | HDF5 filter | channel, **High** |
| C3 | Blosc2 plugin registry | codec IDs | — | Blosc2 | channel, Medium |
| C4 | AWS Registry of Open Data | dataset index | >300 PB [S] | discovery | channel, Medium |

Section numbers below match this table; sections are grouped by kind of data.

**Measured on their own public files** (2026-11-23, `CLAIMS.md` claim 3b, `evidence/first-wave/`): GHCN-Daily,
Backblaze and Loghub are strong; Gaia and Wikimedia are weak and were demoted.

**Suggested first wave**, once the owner approves: GHCN-Daily (#1), Backblaze Drive Stats (#7) and Loghub (#4), each
with its own measured numbers; then Pangeo (#5) once the Zarr codec is merged, with a measurement on a CMIP6 variable
through the codec. C1 and C2 are registrations for the integration sessions (PR #21, #23), not outreach. Like
everything here, they wait for the owner.

---

## A. Open-data portals (CSV and tabular dumps)

### 1. NOAA NCEI GHCN-Daily: High

- **What and how much:**
  - Daily station observations, 1763-2026.
  - `by_year/YYYY.csv.gz`: a recent year is ~175 MB gzipped (2024.csv.gz = 175,771,409 B) [V]
    <https://www.ncei.noaa.gov/pub/data/ghcn/daily/by_year/>.
  - Also `by_station` CSVs, fixed-width `.dly` files and `ghcnd_all.tar.gz` (~3.7 GB [S]); readme [V]
    <https://www.ncei.noaa.gov/pub/data/ghcn/daily/readme.txt>.
  - Mirrored on AWS [V] <https://registry.opendata.aws/noaa-ghcn/>.
- **Current compression:** gzip CSV and tar.gz [V].
- **Why Kolmo fits:** this is the strongest measured case. On two of its station CSVs, never used in tuning:
  - Kolmo -6 is 2.1x smaller than gzip -6;
  - Kolmo -9 is 2.5-2.6x smaller than gzip -6 (`CLAIMS.md` claim 3).
  - Downloads and mirrors pay for transfer, not just storage.
- **Integration:** CLI per file, `tar -I kolmo` for the all-stations tarball. Readers need `kolmo` or `kolmoc` to
  decompress, so the realistic first step is an additional `.kolmo` copy of the tarball, not a replacement.
- **Public contact:** the data-questions route named on the AWS registry entry (NOAA GHCN site), and the NOAA Open
  Data Dissemination (NODD) program's generic contact listed there [V]. The NCEI contact form is on ncei.noaa.gov.
- **Measured** on their main product (first 100 MB of `by_year/2024.csv`, `evidence/first-wave/ghcn-by-year.md`):
  Kolmo -3 13.53, -6 22.29, -9 28.99 against 7.87 for their gzip and 13.26 for xz -9.
- **Priority: High.** Pure gzip CSV, updated daily, and we already have their numbers. A US agency is slow to adopt;
  aim first at a published comparison they can link.

### 2. ESA Gaia Archive, DR3 bulk files: Medium (measured: weak)

- **What and how much:**
  - `gaia_source`: 1,811,709,771 rows, 757 GB compressed [V]
    <https://gea.esac.esa.int/archive/documentation/GDR3/Catalogue_consolidation/chap_cu9cva/sec_cu9cva_consolidation/ssec_cu9cva_counts.html>.
  - Larger tables: xp_continuous ~3.7 TB, mcmc_samples ~3.3 TB [S].
  - Bulk files at <https://cdn.gea.esac.esa.int/Gaia/gdr3/>.
- **Current compression:** gzip ECSV, 3,386 HEALPix-split files per table [V] (mirror README
  <https://sdsc-users.flatironinstitute.org/~gaia/dr3/README.txt>).
- **Why Kolmo fits:** multi-TB numeric CSV, Kolmo's best data type. Kolmo also beat zstd and xz on the SAO star
  catalogue (`CLAIMS.md` claim 2 panel), though that is binary, not ECSV.
  - A University of Barcelona page titled "Gaia EDR3 bulk catalogue available in a better compression format"
    suggests interest. The site was unreachable, so this is not verified [NV] <https://gaia.ub.edu/?p=10558>.
- **Integration:** CLI (`.csv.kolmo`); `kolmoc` for Python readers.
- **Public contact:** Gaia Helpdesk ticket system [S] <https://www.cosmos.esa.int/web/gaia/gaia-helpdesk>; mirror
  operators (Flatiron) via their public pages.
- **Measured** (first 100 MB of `GaiaSource_000000-003111.csv`, `evidence/first-wave/gaia.md`): level 3 ties zstd -3
  (2.77), level 6 ties zstd -19 (3.29 against 3.35), and only level 9 wins (3.78 against 2.87 for gzip -6 and 3.38 for
  xz -9) while decoding at 21 MiB/s. The ECSV float columns carry full-precision values with little decimal
  structure.
- **Priority: Medium.** The volume is real but the gain is small; not a first-wave partner.

### 6. Wikimedia pageview dumps: Low (measured: weak)

- **What and how much:**
  - Daily "user" and "automated" files from 2007 to now.
  - Example: pageviews-20250115-user.bz2 = 696,518,811 B plus automated 237,632,987 B, ~0.9 GB/day, ~300 GB/year
    compressed [V] <https://dumps.wikimedia.org/other/pageview_complete/2025/2025-01/>.
- **Current compression:** bzip2 [V] (readme on the dumps site).
- **Why Kolmo fits:**
  - Count lines (project, title, counts) are tabular text.
  - bzip2 decodes slowly; Kolmo levels 1-7 decode at LZ speed.
  - **Measured** (first 100 MB of 2025-01-15 automated, `evidence/first-wave/wikimedia.md`): level 6 is 8% larger
    than bzip2 -9 (it decodes 22x faster); level 9 is 11% smaller with no decode advantage (36 against 41 MiB/s).
    The lines are mostly page titles, i.e. text.
- **Integration:** CLI.
- **Public contact:**
  - the Analytics public mailing list, named on <https://meta.wikimedia.org/wiki/Research:Data> [S];
  - Phabricator, including the dumps-future consultation <https://phabricator.wikimedia.org/T337887> [S].
- **Priority: Low.** A good community, but the measured gain is too small to lead with.

### 7. Backblaze Drive Stats: High (measured)

- **What and how much:** nearly 300 M records, over 90 GB of raw CSV. Q1 2022 was 6.63 GB of CSV zipped to 1.06 GB [V]
  <https://www.backblaze.com/blog/storing-and-querying-analytical-data-in-backblaze-b2/>. Download page:
  <https://www.backblaze.com/cloud-storage/resources/hard-drive-test-data>.
- **Current compression:** ZIP per quarter [V]; an Iceberg/Parquet copy also exists [S].
- **Why Kolmo fits:** wide numeric CSV (one row per drive per day, SMART columns). The publisher is a storage company
  that already publishes its compression ratios, so it is a ready-made "before/after in $" story.
- **Integration:** CLI, `kolmoc`.
- **Public contact:** the dataset's Hugging Face discussions <https://huggingface.co/datasets/backblaze/Drive_Stats>
  [S]; comments on the Drive Stats blog posts.
- **Measured** (whole file `2025-12-31.csv`, 133 MB, `evidence/first-wave/backblaze.md`): Kolmo -3 has ratio 18.03
  against 10.49 for Backblaze's own zip and 13.60 for zstd -19, compressing at 106 MiB/s; level 9 reaches 22.72.
  Decoding is 200-300 MiB/s, slower than their zip.
- **Priority: High.**

### 15. OpenAQ archive: Medium

- **What and how much:** daily gzipped CSV per location in `s3://openaq-data-archive`, updated hourly. Size not
  stated [V] <https://registry.opendata.aws/openaq/>.
- **Current compression:** gzip CSV [V].
- **Why Kolmo fits:** sensor time series in CSV. **Risk:** many small files (one per location-day), which is Kolmo's
  weak spot. Check the file sizes first; per-month bundles would fit better.
- **Integration:** CLI, `kolmoc`.
- **Public contact:** GitHub organisation <https://github.com/openaq> [NV]; the contact listed on the registry page [V].
- **Priority: Medium.**

### 18. US Census Bureau, ACS PUMS: Low-Medium

- **What and how much:** CSV and SAS zips per state. The 2016 5-year California CSV zip is 283 MB [S]; directory
  <https://www2.census.gov/programs-surveys/acs/data/pums/2016/5-Year/>. Access page:
  <https://www.census.gov/programs-surveys/acs/microdata/access.html>.
- **Current compression:** ZIP [S].
- **Why Kolmo fits:** microdata CSV with many small-integer columns.
- **Integration:** CLI.
- **Public contact:** the ACS Data Users community forum <https://acsdatacommunity.prb.org/discussion-forum> [S].
- **Priority: Low-Medium.** Good data, slow institution. A forum post with a measured file can still help.

### 19. Eurostat bulk download: Low

- **What and how much:** bulk download in gzipped TSV, SDMX-CSV and SDMX-ML. The total is not stated [S]
  <https://ec.europa.eu/eurostat/web/user-guides/data-browser/download-data/bulk-download>.
- **Current compression:** gzip [S].
- **Integration:** CLI.
- **Public contact:** Eurostat user-support form, from the help pages [NV].
- **Priority: Low.** Hard to reach, though the data fits.

### 20. NYC TLC trip records: Low

- **What and how much:** monthly trip files, 2009 to Sep 2026. They moved to Parquet "due to the size of the
  datasets" [V] <https://www.nyc.gov/site/tlc/about/tlc-trip-record-data.page>.
- **Current compression:** Parquet; the codec is not stated [V].
- **Why it is low:** Kolmo has no Parquet integration, and compressed Parquet will not shrink further. It is useful as
  a public benchmark only.
- **Public contact:** the TLC contact page [V].

## B. Scientific archives (HDF5, Zarr, NetCDF)

Measure through the codec or filter on **uncompressed chunks**. A NetCDF-4 file with internal deflate gives 1.01 for
every tool (`CLAIMS.md` claim 3).

### 5. Pangeo CMIP6 cloud data: High

- **What and how much:**
  - CMIP6 as Zarr in `gs://cmip6` and `s3://cmip6-pds`, with a NetCDF copy in `s3://esgf-world` [V]
    <https://registry.opendata.aws/cmip6/>.
  - About 100 TB in the cloud in 2019, and full CMIP6 eventually ~20 PB [S]
    <https://cloud.google.com/blog/products/data-analytics/new-climate-model-data-now-google-public-datasets/>.
- **Current compression:** not stated; likely Zarr's Blosc default [NV].
- **Why Kolmo fits:** huge float arrays. The community openly discusses codecs and S3 cost:
  - "Xarray and compression options for large NetCDF files": 6.4 GB became 3.5 GB with zlib-1 and 1.5 GB with
    Zarr + Blosc; the thread asks for a compression guide [V]
    <https://discourse.pangeo.io/t/xarray-and-compression-options-for-large-netcdf-files/1154>;
  - S3 cost of Zarr stores [S]
    <https://discourse.pangeo.io/t/any-tips-on-avoiding-high-aws-s3-cost-when-storing-zarrs-with-lots-of-objects/1977>.
  - **Not yet proven:** Kolmo's gain on model-output float32 fields. Its decimal-float trick targets values written in
    decimal, so measure first.
- **Integration:** numcodecs/Zarr (PR #21).
- **Public contact:** Pangeo Discourse <https://discourse.pangeo.io> [V]; issues and Discussions at
  <https://github.com/pangeo-data/pangeo-cmip6-cloud> [V].
- **Priority: High** as a community; post a measured result on Discourse rather than an offer alone.

### 8. ARCO-ERA5 (Google Research, ECMWF data): Medium-High

- **What and how much:** full ERA5 as Zarr is ~5 PB, hourly from 1979. The single-level forecast set alone is 53.2 TB
  in 2.17 MB chunks [V] <https://github.com/google-research/arco-era5>.
- **Current compression:** the codec is not stated in the README [V].
- **Why Kolmo fits:** the largest gridded-float Zarr store in public. Same "measure float fields first" caveat as #5.
- **Integration:** numcodecs/Zarr.
- **Public contact:** GitHub issues on the repository [V].
- **Priority: Medium-High.**

### 13. ECMWF / Copernicus Climate Data Store: Medium

- **What and how much:** ERA5 and other reanalyses, delivered as NetCDF-4 with internal compression since the 2024 CDS
  converter [S].
- **Why Kolmo fits:** only through an HDF5 filter in NetCDF-4. Users report read-performance problems with the new
  format [S] <https://forum.ecmwf.int/t/huge-performance-drop-when-reading-new-era5-netcdf4-format/12831>.
- **Integration:** HDF5 filter (PR #23; needs a registered ID first, channel C2).
- **Public contact:** ECMWF public forum <https://forum.ecmwf.int> [S].
- **Priority: Medium.** Institutional; after C2.

### 12. NASA EOSDIS / Earthdata: Medium

- **What and how much:** a 225.7 PB archive, 152.1 PB of it in the cloud, growing 174 TB/day (FY Oct 2025-Sep 2026) [V]
  <https://www.earthdata.nasa.gov/about/data-metrics>. Mostly HDF5 and NetCDF-4.
- **Current compression:** not stated [NV].
- **Integration:** HDF5 filter.
- **Public contact:** Earthdata Forum <https://forum.earthdata.nasa.gov> (posting needs an Earthdata Login) [S].
- **Priority: Medium.** The largest volume of all, but slow adoption; go through the HDF5 filter registry first.

### 11. OME-Zarr / OME-NGFF and the Image Data Resource: Medium

- **What and how much:** ~500 TB of OME-Zarr 0.5 data in the 2024 NGFF challenge [V]
  <https://ngff.openmicroscopy.org/resources/data/index.html>; IDR converted 17 studies, ~47 TB [S].
- **Current compression:** Zarr defaults (Blosc/zstd). A published codec benchmark exists [S]
  <https://arxiv.org/pdf/2503.18037>.
- **Why Kolmo fits:** uint16 microscopy. Kolmo's 16-bit image model won on Silesia's x-ray and mr files, but Kolmo makes
  no general image claim, so measure first.
- **Integration:** numcodecs/Zarr.
- **Public contact:** <https://github.com/ome/ngff/issues> [V]; image.sc forum [NV].
- **Priority: Medium.**

### 10. scverse / AnnData: Medium

- **What and how much:** a format and toolkit; the data is held by users.
- **Current compression:** the docs discuss zarr-python 3's switch from Blosc to zstd, and recommend Blosc for the old
  behaviour [V] <https://anndata.scverse.org/en/latest/tutorials/zarr-v3.html>.
- **Why Kolmo fits:** sparse count matrices are integer columns, a strong data type for Kolmo, and codec choice is
  already debated publicly.
- **Integration:** numcodecs/Zarr; HDF5 filter for `.h5ad`.
- **Public contact:** <https://discourse.scverse.org>, the scverse Zulip, GitHub issues [S].
- **Priority: Medium.**

## C. Logs and observability

### 4. Loghub (logpai): High

- **What and how much:** raw sizes listed in the README sum to ~83 GiB, from Linux at 2.25 MiB to Thunderbird at
  29.6 GiB [V] <https://github.com/logpai/loghub>. Papers cite 77 GB and 440 M lines [S]
  <https://arxiv.org/pdf/2008.06448>.
- **Current compression:** zip and tar.gz on Zenodo [V].
- **Why Kolmo fits:** the standard log benchmark. Kolmo's panel log file comes from Loghub (`CLAIMS.md` claim 2:
  1.4x smaller than zstd -3 at level 3, 1.6x at level 5). The log-compression research community reads it.
- **Integration:** CLI, `tar -I kolmo`.
- **Public contact:** GitHub issues [V].
- **Measured** (first 100 MB of `BGL.log`, `evidence/first-wave/loghub-bgl.md`): Kolmo -3 15.45, -6 24.02, -9 33.59,
  against 12.93 for their zip, 22.64 for zstd -19 and 23.18 for xz -9.
- **Priority: High** for credibility (a published comparison), less as a storage-cost partner.

### 3. GH Archive: High

- **What and how much:** hourly `.json.gz` files since 2011-02-12; the total is not stated [V]
  <https://www.gharchive.org>. A third-party subset was 103.9 GB of NDJSON [S].
- **Current compression:** gzip [V].
- **Why Kolmo fits:** measured (`CLAIMS.md` claim 3). On one hour of 2015 data, Kolmo -9's ratio is 13.19 against 6.88
  for gzip -6, 1.9x smaller. JSON gains over zstd -19 and xz -9 are only up to 12%, so lead with the gzip comparison
  and the storage they serve.
- **Integration:** CLI.
- **Public contact:** <https://github.com/igrigorik/gharchive.org/issues> [V].
- **Priority: High.** A large, open, single-maintainer project still on gzip.

### 14. Vector (log pipeline): Medium

- **What and how much:** a pipeline; it stores nothing itself.
- **Current compression:** the `aws_s3` sink offers gzip (the default), none, snappy, zlib and zstd [V]
  <https://vector.dev/docs/reference/configuration/sinks/aws_s3/>.
- **Why Kolmo fits:** archiving logs to S3 is exactly the cold-log case (levels 6-9).
- **Integration:** the Rust `kolmo` crate as a sink compression option. That needs a feature request and buy-in, and
  is real engineering work.
- **Public contact:** GitHub issues and Discussions at <https://github.com/vectordotdev/vector>.
- **Priority: Medium.** Better after one production user exists.

### 16. Grafana Loki: Low-Medium

- **Current compression:** chunk encodings gzip, snappy and zstd; the docs recommend snappy [S]
  <https://grafana.com/docs/loki/latest/configure/bp-configure/>.
- **Demand signal:** "Will Loki be considering CLP?" asks for better log compression; no replies [V]
  <https://community.grafana.com/t/will-loki-be-considering-clp/77022>.
- **Integration:** Loki is Go; Kolmo has no Go binding. CLI for exported archives only.
- **Public contact:** <https://community.grafana.com>, GitHub issues.
- **Priority: Low-Medium.**

### 17. Common Crawl index files: Low-Medium

- **What and how much:** the July 2026 crawl is 2.14 bn pages, 364 TiB uncompressed. Indexes: URL-index Parquet
  0.20 TiB, cc-index (CDXJ) 0.22 TiB [V] <https://blog.commoncrawl.org/blog/july-2026-crawl-archive-now-available>.
- **Current compression:** Parquet moved to zstd from CC-MAIN-2026-30 [V]; CDXJ is gzip.
- **Why Kolmo fits:** only the CDXJ text index (sorted URL keys, timestamps, JSON fields). WARC page content is mostly
  text and HTML, a weak fit.
- **Public contact:** the Common Crawl public Google Group [NV].
- **Priority: Low-Medium.** Open to codec changes, narrow fit.

## D. Dataset hosts

### 9. Source Cooperative and the data.gov archive: Medium

- **What and how much:** Source Cooperative hosts over 5 PB [V] <https://github.com/source-cooperative/source.coop>.
  It holds the Harvard Library Innovation Lab's data.gov archive: 16 TB, 311k+ datasets [V]
  <https://lil.law.harvard.edu/blog/2025/02/06/announcing-data-gov-archive>.
- **Current compression:** mixed, per publisher [NV].
- **Why Kolmo fits:** a cost-conscious open host with a large mixed CSV corpus. The data.gov archive is a natural
  "whole-corpus" test.
- **Integration:** CLI; Zarr for array datasets.
- **Public contact:** GitHub issues on source.coop [V]; <https://github.com/harvard-lil/data-vault> for the archive
  tooling [V].
- **Priority: Medium.**

## E. Channels (registries and catalogues, not partners)

These are owned by the integration sessions and are listed so outreach does not duplicate them. **No registration is
sent without the owner's approval.**

- **C1. Zarr / numcodecs.**
  - zarr-python 3 switched its default codec from Blosc to zstd [V] (anndata page above).
  - A Zarr 3 codec name is registered by a PR to <https://github.com/zarr-developers/zarr-extensions> (a `codecs/`
    directory with README and schema) [V].
  - Interoperability debate: <https://github.com/zarr-developers/zarr-specs/issues/293> [S].
  - One registration reaches #5, #8, #10 and #11. **High.**
- **C2. HDF Group filter registry.**
  - Registered filters include LZ4 32004, Zstandard 32015 and Blosc2 32026 [V]; the request goes to the HDF Group
    help desk, as described at <https://github.com/HDFGroup/hdf5_plugins/blob/master/docs/RegisteredFilterPlugins.md>
    [V].
  - It is needed before #12 and #13. **High.**
- **C3. Blosc2 plugin registry.**
  - Global IDs need review by the Blosc team, per `plugins/README.md` in <https://github.com/Blosc/c-blosc2> [V];
    <https://www.blosc.org/posts/registering-plugins/> [V].
  - **Medium.**
- **C4. AWS Registry of Open Data.**
  - The registry is at <https://github.com/awslabs/open-data-registry> [V]; its size, >300 PB, comes from a third-party
    blog [S].
  - Each entry names a dataset's managed-by contact; it is how we found #1, #5 and #15. **Medium**, as discovery.

## Considered and left out

- **Hugging Face Hub:** ~20 PB moved to Xet chunk deduplication [V] <https://huggingface.co/blog/migrating-the-hub-to-xet>.
  It is a host, not a codec adopter, and much of it is model weights, a claim to avoid.
- **Zenodo:** a 5 PB disk cluster [V] <https://about.zenodo.org/infrastructure/>; files are opaque uploads.
- **VictoriaLogs:** already columnar with zstd, written in Go.
- **World Bank WDI, Our World in Data:** too small to matter for storage cost (OWID data is already in Kolmo's panel).
- **Dryad:** no total size stated.
- **Rubin/LSST, SDSS (FITS), 1000 Genomes / Ensembl (VCF with bgzip), Kaggle:** weak fits or no public route for a
  codec change. Genomics could return once a FASTQ/VCF measurement exists; FASTA at level 3 only ties zstd -3.
