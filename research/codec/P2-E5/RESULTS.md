# P2-E5 — type-first canonical-integer CSV (column-first typing), development

Status: **development** (tables derived from observed inputs by `make_tables.py`; `results/inputs_manifest.json`).
Not P2 evidence. Same in-memory boundary and cost model as P2-E1/E2; two independent 5-round runs.

## Pipeline (`p2e5.c`)

Exact typing inside the timed encode: header line + >= 16 rows of comma-separated canonical int64 decimals
(`-?(0|[1-9][0-9]*)`, no `-0`, in range) with a constant column count and `\n` endings; anything else is opaque
(whole-input zstd, scan charged). Canonical spelling makes reconstruction exact by construction; the original's
production block checksum is stored and verified. Columns (int64) go through P2-E2's identity-checked `fi_forward`
and an E02C residual leaf; decode is `fi_inverse` + two-digits-per-step decimal printing (production's `KG_D2` table).
`_lay`: tables with >= 16 columns and < 4096 rows are coded as one column-major stream (no trial; development rule).

Correctness: every pipeline reconstructs every table byte for byte; ASan/UBSan/LSan clean on all 5 tables. Two
defects found and fixed during development before any timing was recorded: a NULL pointer passed to a zero-length
`memcpy` (UBSan) and an over-strict output bound in the printer.

## Results vs zstd-3 (bytes / median cost; * = fits on every one of 5 pairs), run 1 | run 2

| Table (rows × cols) | `csv_planes_huf_lay` | `csv_prod3_lay` | production `kolmo_fast` | zstd-19 |
|---|---|---|---|---|
| dem (344 × 403) | −35.8% / −19%* \| −19%* | −44.8% / −11%* \| −12%* | −36.8% / +146% | −16.3% |
| goog (1047 × 2) | −55.4% / −27%* \| −29%* | −54.6% / +0% \| −1% | −41.3% / +114% | −34.4% |
| fs (150686 × 3) | −25.8% / −5% \| −2%* | −52.7% / +14% \| +21% | −45.6% / +7% | −39.2% |
| syn (300000 × 3) | −35.5% / −6% \| −1% | −51.0% / +31% \| +34% | −18.2% / +29% | −12.1% |
| mri (256 × 256) | −25.4% / +44% \| +41% | −42.2% / +57% \| +54% | +1.1% / +116% | −13.4% |

## Findings

1. **Column-first typing works for tables under P2.** With per-plane Huffman, the typed CSV path is smaller than
   zstd-3 on all five tables (−25% to −55%) and inside or at the edge of its round-trip budget on four; production
   `kolmo_fast` (which has its own FIELDS/DECIMAL table transform) is never inside the budget.
2. **The zstd-coded leaf is much smaller on tall tables** (fs −52.7%, syn −51.0%; better than production `kolmo_fast`
   and zstd-19) but 14–34% over budget: a strong archive-mode candidate, not a speed-mode one.
3. **Layout is a real planning decision**: one column-major stream for wide short tables turns dem from +25% to −19%
   cost and mri from −6.1% to −25.4% bytes; applied to a narrow table (goog) it costs 30 points of bytes.
4. Decimal parsing/printing is a first-order cost on tables (it was about half the decode before the two-digit
   printer); remaining overruns are mri (wide, small) and the zstd leaf on tall tables.

## Not done / next

- Fresh, source-disjoint integer tables (registered before reading) to validate the frozen layout rule.
- Non-canonical spellings (leading zeros, `+`, spaces, quoted fields, CRLF) via exact lexical side streams, as in
  `competitor-policy-v2/typed_columns.py` — currently opaque fallback.
- OpenZL's CSV profile on the same tables (N1 boundary) for the archive comparison.
