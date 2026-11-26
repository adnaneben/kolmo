# Loghub: GitHub Discussion (not posted)

**Category:** Show and tell (or Ideas, if Show and tell does not exist)

**Title:** Compression results on BGL with a log-structure-aware compressor (and a free measurement offer)

---

Hi Loghub maintainers,

Thank you for Loghub. It is the standard log corpus, and we used it while developing Kolmo, an open-source (Apache-2.0)
lossless compressor. Kolmo splits log lines into templates and variables by itself, with no parser configuration.

We measured it on the first 100 MB of `BGL.log` from your Zenodo record. All tools ran on one thread, and every output
was decompressed and compared byte for byte.

| Tool | Ratio | Compress / decompress MiB/s |
|---|---:|---|
| your zip (whole file) | 12.93 | — |
| gzip -6 | 12.36 | 77 / 1892 |
| zstd -19 | 22.64 | 1.7 / 1569 |
| xz -9 | 23.18 | 3.4 / 202 |
| Kolmo -6 | 24.02 | 8.5 / 912 |
| Kolmo -9 | 33.59 | 2.9 / 308 |

- Kolmo -6 is about as small as xz -9 and decodes 4.5x faster.
- Kolmo -9 is 2.6x smaller than the published zip.
- At Kolmo's default level (-3) the gain is smaller: 1.4x over zstd -3.

Full report, download provenance and the script:
<https://github.com/adnaneben/kolmo/blob/main/docs/outreach/evidence/first-wave/loghub-bgl.md>.
Method: <https://github.com/adnaneben/kolmo/blob/main/docs/outreach/CLAIMS.md>.

Two offers, if useful:
1. We can run the same report on all Loghub datasets and post it here, for anyone choosing a compressor for log
   archives.
2. If you ever repackage the Zenodo files, we can measure the whole set first, so you can judge whether Kolmo is worth
   it.

Limits, so you can judge:
- zstd compresses much faster;
- level 9 decodes at about 300 MiB/s;
- readers would need the `kolmo` tool (Homebrew, PyPI) to decompress;
- Kolmo is young (0.0.x, one maintainer), though its format is specified and frozen (FORMAT.md, COMPATIBILITY.md).

If this is not the right place, please feel free to close it.
