# P2-E6 — type-first mixed CSV (integer, fixed-scale decimal, text columns), development

Extends P2-E5 so real CSVs with text and decimal columns can be typed (`p2e6.c`). Contract, checked inside the timed
encode: header line + >= 16 rows of k comma-separated fields, `\n` endings, no `"`, `\r` or NUL (otherwise opaque).
Per column: INT (canonical int64), DEC (canonical fixed-point, one scale 1–18 for the whole column, stored as an exact
int64 mantissa — no binary floats), or TEXT (field bytes, `\n`-terminated, one shared stream with the production leaf
rule). Numeric columns use P2-E2's identity-checked fast integer path; >= 16 numeric columns and < 4096 rows form one
column-major stream. Single-pass typing (running per-column type); checksum of the original verified on decode.

## Correctness (all local, ASan/UBSan/LSan)

- 300 generated edge-case CSVs (`gen_edge.py`, seed 20261113: `-0`, `-0.00`, leading zeros, `1.`, `.5`, mixed
  scales, empty and trailing fields, int64 extremes and overflow, quotes, CRLF, NUL, missing final newline) and 10
  development tables: every file round-trips exactly through `mcsv_planes_huf` and `mcsv_prod19`.
- Typing decisions agree with an independent Python oracle (`oracle.py`) on **310/310** files
  (`results/typing_agreement.txt`), including every opaque rejection.
- Two defects found during development and fixed before timing: an 18-digit cap (19-digit integers fell to TEXT)
  and signed-overflow UB on −2^63.

## Development results vs zstd-3 (in-memory boundary, 5 paired rounds; `results/summary.txt`)

| Table | types | `mcsv_planes_huf` bytes / cost | `mcsv_prod3` bytes | production `kolmo_fast` bytes / cost |
|---|---|---|---|---|
| fs_mixed (derived) | I T D3 T I | −62.1% / +11% | −70.9% | −69.2% / +8% |
| syn_mixed (derived) | I T D1 T | −87.9% / −0% | −87.9% | −82.3% / +1% |
| goog_mixed (derived) | T T I D2 | −42.1% / +32% | −39.2% | −26.0% / +176% |
| dem_rows (all INT) | 403 × I | −35.6% / −10% * | −44.6% | −36.8% / +141% |
| goog_table (all INT) | I I | −55.4% / −18% * | −54.5% | −41.3% / +113% |
| real Stocks.csv (local) | all T | −3.3% / +64% | −3.3% | −2.2% / +131% |
| real zip_sums.csv (local) | all T | −11.1% / +35% | −11.1% | −11.1% / +18% |

(* fits zstd-3's cost on every pair.) Bytes: typed columns beat zstd-3 by 36–88% where numeric columns exist and
match or beat production `kolmo_fast` in 5 of 7 cases. Cost: the general mixed parser is slower than P2-E5's
all-integer parser (fs_table +24% vs about −3% there; a shared, noisy 2-vCPU machine); text-only files pay a typing
pass for nothing. **Provisional:** byte-strong, cost not yet inside the speed budget except on all-integer tables;
next is to make the TEXT path cost-neutral (skip typing when the first rows are all text) and to profile the mixed
decoder. The archive comparison against OpenZL's CSV profile on the N1 boundary is the natural next measurement.
