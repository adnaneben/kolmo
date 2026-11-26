# First messages and follow-up

**Drafts only. Nothing here has been sent, and nothing may be sent without the owner's approval** (`README.md`).

Rules for every message:

- **Public route only:** the project's issue tracker, Discussions, forum or public mailing list, as listed in
  `candidates.md`. No personal email addresses, and no direct messages to individuals found through the project.
- **Read their contribution rules first.** Many trackers do not want product pitches as issues. Where Discussions or a
  forum exists, use it, and use the category for ideas or show-and-tell. If the rules forbid it, do not post.
- **One message per project**, and one follow-up at most, after two weeks. Then stop.
- **Every number links to its method** in `CLAIMS.md`. Use only the claims listed there, with the qualifiers given there
  (machine, one thread, which file). Never claim: faster than zstd, best for model weights, beats OpenZL, good on
  small files.
- **The offer** is always the same: we run Kolmo on a sample they choose and send a free report
  (`sample-report-template.md`), or they run the one-command script themselves and nothing leaves their machine.
- Fill every `<...>`, and replace the example numbers with numbers measured on **their** public data where possible.
  Run `sample_report.py` on a public file of theirs first, so the first message already carries their own number.

Links used below (they resolve once the outreach kit is merged to main):

- Method and claims: `https://github.com/adnaneben/kolmo/blob/main/docs/outreach/CLAIMS.md`
- Report script: `https://github.com/adnaneben/kolmo/blob/main/docs/outreach/sample_report.py`
- Format spec and promise: `https://github.com/adnaneben/kolmo/blob/main/FORMAT.md`,
  `https://github.com/adnaneben/kolmo/blob/main/COMPATIBILITY.md`

---

## A. Open-data maintainer (CSV and tabular dumps)

**Title:** Smaller downloads for \<dataset\>: an offer to measure, free

> Hi \<project\> maintainers,
>
> I work on Kolmo, an open-source (Apache-2.0) lossless compressor that detects table structure in CSV by itself,
> with no schema or configuration. I ran it on one of your public files, \<file, URL\>:
>
> - gzip -6: \<X\> MB; zstd -19: \<Y\> MB; Kolmo -6: \<Z\> MB (\<N\>x smaller than gzip).
> - One-thread speeds, compress / decompress: \<from the report\>.
> - Every output was decompressed and compared byte for byte. Method and command: \<CLAIMS.md link\>.
>
> For comparison, on NOAA's GHCN-Daily `by_year/2024.csv`, which Kolmo was never tuned on, Kolmo's default level
> is 1.8x smaller than gzip -6, and level 6 is 2.9x smaller (\<CLAIMS.md link, claim 3b\>).
>
> The offer: if you point me at a representative sample, I'll send a free report with per-file sizes, one-thread
> speeds and the projected storage and egress saving for your total volume. Or you can run the script yourself
> (\<sample_report.py link\>); nothing has to leave your machines.
>
> Limits, so you can judge: Kolmo compresses more slowly than zstd, users need the `kolmo` tool (Homebrew, PyPI) to
> decompress, and it is a young project (0.0.x, one maintainer), though the format is specified and frozen
> (FORMAT.md, COMPATIBILITY.md).
>
> If this is the wrong place for this, sorry, and feel free to close it.

## B. Scientific-data engineer (HDF5, Zarr, NetCDF, NumPy)

**Title:** Measuring a structure-aware codec on \<dataset\> arrays (numcodecs / HDF5 filter)

> Hi \<project\> team,
>
> Kolmo is an open-source lossless compressor that finds structure in numeric data by itself: decimal-valued
> float64, integer series, fixed records and 16-bit images, with no filter pipeline to choose. On a published panel
> (one thread, every round trip verified; \<CLAIMS.md link, claim 2\>), its default level against zstd -3 made:
>
> - a NumPy float64 table 1.8x smaller (ratio 15.6 against 8.7);
> - int64 timestamps 2.6x smaller (24.2 against 9.4).
>
> (Files that are already compressed inside, such as deflated NetCDF-4 variables, do not shrink with any tool; Kolmo
> would replace that inner compression, through the filter or codec, not run on top of it.)
>
> A numcodecs codec for Zarr and an HDF5 filter are in review in our repository; Blosc2 support is in progress.
> \<Say which one fits them, and its status on the day you send this.\>
>
> The offer: we run it on a sample of your chunks (or you run one command, \<sample_report.py link\>) and send a free
> report: ratio and one-thread speed against zstd, Blosc + shuffle and xz, with the storage cost for your volume.
>
> Limits: decompression at levels 2-7 is 30-40% slower than zstd on one thread (it scales across cores), and Kolmo
> gives no gain on ML weights. We would rather know now if it does not help your data.

## C. Observability / log-archive team

**Title:** Cold log archives: xz-class size, much faster reads; a free measurement offer

> Hi \<project\> maintainers,
>
> Kolmo is an open-source lossless compressor that splits log lines into templates and variables by itself
> (timestamps, IPs and IDs become integer columns). For cold archives that still get read back:
>
> - On the Silesia corpus, Kolmo -6 is smaller than xz -9 and decompresses 8.6x faster, on one thread
>   (\<CLAIMS.md link, claim 1\>).
> - On public NASA HTTP access logs, which Kolmo was never tuned on, Kolmo -9 is 1.8x smaller than gzip -6 and
>   1.2x smaller than xz -9 (\<CLAIMS.md link, claim 3\>). The gain on logs shows at levels 6-9; at the default level
>   Kolmo roughly ties zstd -3.
>
> It works as `tar -I kolmo`, as a CLI and from Python. \<Name the route that fits their storage: chunk files in object
> storage, rotated files, Parquet is not a target.\>
>
> The offer: send a sample of archived chunks, or run one command (\<sample_report.py link\>), and we will send a free
> report with sizes, one-thread speeds and the yearly storage cost at your volume and price.
>
> Limits: Kolmo is not a hot-path codec. zstd compresses faster, and at levels 2-7 Kolmo decodes about 30-40% slower
> on one thread.

---

## Follow-up (once, two weeks later, in the same thread)

> Hi again, following up once on this. In case a concrete number helps, here is the report for \<the public file we
> measured\>: \<link to a gist or attached Markdown made by sample_report.py\>. The offer stands: a free report on a
> sample you choose, or the one-command script.
>
> If it is not useful for \<project\>, no reply is needed; I won't post again.

## After a yes

1. Agree on the sample: size (1-10 GB is enough), representative files, how it is handed over, and that it is deleted
   after the report. Prefer they run the script themselves if the data is not public.
2. Run `sample_report.py` with their volume, current compressor and price; write sections 1, 2 and 8 of
   `sample-report-template.md` by hand.
3. Owner reviews the report before it is sent.
4. If they adopt it, propose a case study (`case-study-template.md`); publish only with their written approval.
