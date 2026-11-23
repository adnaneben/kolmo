# Design-partner outreach kit

Phase 4 of the adoption plan: find one to three design partners who store large volumes of the data Kolmo is best
at (CSV and tabular dumps, logs, numeric arrays in HDF5/Zarr/NetCDF, pickles), measure Kolmo on their data, and, with
their permission, publish their before/after storage cost. One published production user is worth more than any
benchmark.

**Status: drafts. Nobody has been contacted and nothing has been sent or posted.** Every first message, follow-up,
report and case study needs the owner's approval before it leaves the repository.

| File | What it is |
|---|---|
| [`candidates.md`](candidates.md) | 20 candidate data owners and 4 channels (registries, catalogues) from public sources: what they store and how much, why Kolmo fits, the integration they would use, a public contact route, and a priority |
| [`CLAIMS.md`](CLAIMS.md) | The only claims the messages may make, each with its numbers, method, raw data and reproduce command; and the claims to avoid |
| [`message-templates.md`](message-templates.md) | First message per audience (open-data maintainer, scientific-data engineer, observability team), one follow-up, and the steps after a yes |
| [`sample_report.py`](sample_report.py) | Runs Kolmo -3/-6/-9, zstd -3/-19 and xz -9 (and gzip -6) on a directory of sample files, one thread, verifies every round trip, and writes the per-file and total report with the projected yearly storage cost |
| [`sample-report-template.md`](sample-report-template.md) | The report we send: generated sections and the hand-written ones around them |
| [`case-study-template.md`](case-study-template.md) | The structure of a published case study: before/after, method, costs, limits, quotes, approval record |
| [`evidence/`](evidence/) | Raw results behind CLAIMS.md: lzbench CSVs (Silesia, one thread) and the structured-data panel JSON with its script |

## Workflow

1. Pick a candidate from `candidates.md` (High priority first). Download one or two **public** files of theirs and run
   `sample_report.py` on them, so the first message carries a number measured on their own data.
2. Fill the matching template in `message-templates.md`. Check every number against `CLAIMS.md` or the report.
3. **Owner approves the message**, then it is posted on the candidate's public route (never a personal address).
4. On a yes: agree on the sample, run the report, write the hand-written sections, owner reviews, send.
5. On adoption: case study, published only with the partner's written approval and the owner's.

```sh
python3 docs/outreach/sample_report.py SAMPLE_DIR --volume 40TB --price 0.023 --current gzip --out report.md
```
