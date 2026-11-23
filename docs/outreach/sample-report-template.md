# Sample report template

The report we send a prospective design partner after running Kolmo on a sample of their data. Sections 3-7 come out
of `sample_report.py` as they are; we write sections 1, 2 and 8 by hand. **Draft only**: nothing is sent without the
owner's approval (see `README.md`).

```sh
# their files in one directory; volume = how much of this data they hold, uncompressed (or --volume-is current)
python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 40TB --price 0.023 --current gzip \
    --title "<Organisation>: <dataset>" --out report.md --json report.json
```

- `--current` is how they store the data today: `gzip` (= gzip -6), `zstd-3`, `zstd-19`, `xz-9`, or `none`
  (uncompressed, the default). Savings are computed against it.
- `--price` is their storage price per GB-month. Use their stated price, or a public list price, and name it:
  S3 Standard is about $0.023, and archive tiers are much lower.
- `--repeat 3` gives steadier speeds on small samples.

---

# Kolmo sample report: \<Organisation\>, \<dataset\>

## 1. What we were given (hand-written)

- Files: \<N files, total size, formats (CSV, NetCDF/HDF5, logs, ...)\>, chosen by \<them / us from their public bucket
  at URL\>, on \<date\>.
- How they are stored today: \<gzip -6 / zstd / uncompressed\>, according to \<their docs or what they told us\>.
- Volume of this kind of data: \<X TB\>, according to \<source\>. Storage price used: \<$/GB-month, source\>.
- Handling: the files were kept on one machine for this report and deleted on \<date\>. Nothing was shared.

## 2. Answer in one paragraph (hand-written)

\<Say "Kolmo -N stores this data in X% of the space gzip uses, which for Y TB is $Z a year at your price"; then say
what it costs: compress speed, decompress speed, memory. If Kolmo does not help on some files (model weights,
already-compressed data, small files), say so here too.\>

## 3. Summary (generated)

Best Kolmo setting, its ratio against the current setting, Kolmo's default level, the number of verified round trips,
and the projected yearly saving.

## 4. Totals for the whole sample (generated)

| Setting | Compressed | Ratio | Compress MiB/s | Decompress MiB/s | Round trip |
|---|---:|---:|---:|---:|---|
| kolmo -3 / -6 / -9 | | | | | verified |
| zstd -3 / -19, xz -9 (gzip -6 with `--gzip` or `--current gzip`) | | | | | verified |

## 5. Projected storage cost (generated, with `--volume`)

Stored volume, yearly cost and saving for every setting, against the current setting. The projection applies the
sample's total ratio to the whole volume. **It holds only if the sample is representative.** Request, transfer and
CPU costs are not included.

## 6. Per file (generated)

Ratio of every setting on every file, with the best in bold. Speeds per file are in the JSON.

## 7. Method (generated)

Machine, tool versions, one thread per tool, best of N runs, every output decoded and compared byte for byte, and the
command that reproduces the report.

## 8. Limits and next steps (hand-written)

- **Format.** Kolmo's container version 12 is specified in `FORMAT.md`. `COMPATIBILITY.md` promises that every later
  release reads every version-12 frame. Kolmo is at version 0.0.x and has one maintainer; say so.
- **Speed.** Kolmo compresses more slowly than zstd at the same level of effort, and decodes 30-40% slower than zstd at
  levels 2-7 on one thread. On many cores, its decoder scales and zstd's does not (CLAIMS.md, claim 4).
- **Memory.** Level 9 uses up to about 1.5 GB to compress.
- **How they would use it:** \<CLI or `tar -I kolmo`, Python `kolmoc`, numcodecs/Zarr, HDF5 filter, Blosc2\>, and what
  is already released and what is still a pull request.
- **Offer:** if they try it in production, we help, and with their permission we publish a case study
  (`case-study-template.md`).
