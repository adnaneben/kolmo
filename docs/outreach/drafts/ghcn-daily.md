# NOAA NCEI GHCN-Daily: contact form (not posted)

**Subject:** GHCN-Daily by_year files: about 3x smaller downloads measured with an open-source compressor

---

Hello,

I work on Kolmo, an open-source (Apache-2.0) lossless compressor that detects table structure in CSV by itself. We
measured it on the first 100 MB of your public `by_year/2024.csv`. All tools ran on one thread, and every output was
decompressed and compared byte for byte.

| Tool | Ratio | Compress / decompress MiB/s |
|---|---:|---|
| your 2024.csv.gz (whole file) | 7.87 | — |
| gzip -6 | 7.65 | 40 / 400 |
| xz -9 | 13.26 | 2.4 / 152 |
| Kolmo -3 (default) | 13.53 | 197 / 521 |
| Kolmo -6 | 22.29 | 13 / 724 |

- At its default level, Kolmo makes the file 1.8x smaller than gzip.
- At level 6 it is 2.9x smaller, and it still decodes faster than gzip.
- For the by_year archive and its mirrors, that is roughly a third of the download size.

Report and method: <https://github.com/adnaneben/kolmo/blob/main/docs/outreach/evidence/first-wave/ghcn-by-year.md>.

The offer: we will run the same free report on the full by_year set, or on any other GHCN product, and send it to
whoever you name. Alternatively, your team can run the one-command script itself
(<https://github.com/adnaneben/kolmo/blob/main/docs/outreach/sample_report.py>).

We understand users need gzip-compatible files. The realistic first step would be an additional `.kolmo` copy for
bulk users, not a replacement.

Limits: Kolmo is a young project (0.0.x, one maintainer). Its format is specified and frozen (FORMAT.md,
COMPATIBILITY.md), and decoding needs the `kolmo` tool (Homebrew, PyPI).

Thank you for keeping GHCN-Daily open.
