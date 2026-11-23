# Case study template

The structure of a published design-partner case study. **Publish only with the partner's written approval of the
final text, numbers and quotes**, and only after the owner approves it. Every number must come from a report made with
`sample_report.py` (or from production measurements the partner gives us), with the command and the tool versions.

---

# \<Organisation\> stores \<dataset\> in \<X\>% less space with Kolmo

*\<One sentence: who they are, what data, how much, the saving in $/year.\>*

## At a glance

| | Before | After |
|---|---|---|
| Compressor and setting | \<gzip -6\> | \<kolmo -6\> |
| Stored volume | \<X TB\> | \<Y TB\> |
| Storage cost per year (at \<$/GB-month\>, \<provider and tier\>) | \<$A\> | \<$B\> |
| Compress speed, one thread | \<MiB/s\> | \<MiB/s\> |
| Decompress speed, one thread | \<MiB/s\> | \<MiB/s\> |
| Peak memory, compress / decompress | \<MB\> | \<MB\> |
| Round trips verified | \<N files, all byte-identical\> | |

## The organisation and the data

- Who they are, in their own words: \<link to their about page\>.
- What the data is: \<formats, record layout, how it grows, link to the public dataset if it is public\>.
- How it was stored before, and why: \<gzip for compatibility, zstd for speed, ...\>.
- Who reads it and how often: \<archive read rarely, or a dataset downloaded N times a month\>. This sets which level
  makes sense.

## What we did

1. **Sample.** \<N files, X GB, chosen how; why it is representative\>.
2. **Measurement.** `sample_report.py` on \<machine\>, one thread per tool, every output decoded and compared byte for
   byte. Command: `<exact command>`. Tool versions: \<...\>.
3. **Choice of level.** \<Why level N: the ratio against write and read speed for their workload\>.
4. **Integration.** \<CLI or `tar -I kolmo` in their archive job, Python `kolmoc`, numcodecs codec in Zarr, HDF5
   filter, Blosc2\>. \<What changed in their pipeline, in lines of code or config.\>
5. **Rollout.** \<Pilot on one bucket or one month of data, then ...; how they verify integrity (`kolmo -t`).\>

## Results

- Table of totals per setting, copied from the report (Kolmo levels, their current setting, zstd -19, xz -9).
- Per-file-type breakdown: \<where Kolmo gained most, and where it did not help\>.
- Cost: stored volume x price x 12 months, with the price and its source. In production, measured bucket size
  before and after, if they will share it.

## What did not work, or cost more

\<Required. For example: slower compression than zstd, memory at level 9, files that did not shrink, tooling that had
to change. Honest limits make the result believable.\>

## In their words

> \<Quote placeholder: one or two sentences from the partner, approved by them in writing.\>
>
> — \<Name, role, organisation, only with their permission; otherwise "an engineer at <organisation>"\>

## Reproduce it

- Kolmo version and install: `brew install adnaneben/kolmo/kolmo` or `pip install kolmoc`, \<version\>.
- Report script and method: `docs/outreach/sample_report.py`, `docs/outreach/CLAIMS.md`.
- Data: \<public URL of the sample, if it is public\>.

## Approval record (not published)

| Item | Approved by | Date |
|---|---|---|
| Numbers and method | \<partner contact role\> | |
| Quote and attribution | | |
| Organisation name and logo use | | |
| Owner approval to publish | owner | |
