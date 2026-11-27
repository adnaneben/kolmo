# P2-E7 results — type-first portfolio on N1's complete-CLI boundary (development)

Run: Actions **43310294371**, ~13 runner-minutes; workflow at `3257d66` (P2-E6 typing at that commit, i.e. before
the SWAR revision). Evidence recovered from the job log; all 13 files match the run's `MANIFEST.json`. Toolchain as
P2-E3 (OpenZL CLI 0.3.0, zstd CLI 1.5.7, Kolmo control). Sanitizer gate: 50/50 research round trips passed before
timing. N1 `confirm`: **IDENTITIES_MATCH** (130 configurations, 114 complete; the 16 failures are OpenZL `-p numpy`,
which v0.3.0 does not provide).

## CSV views (2 derived integer tables) — both runs agree on every fit verdict except where noted

| Table | Policy | Bytes vs zstd-3 | Speed cost vs zstd-3 (fits every pair) | Bytes vs OpenZL csv-9 | Archive cost vs OpenZL csv-9 (fits) |
|---|---|---|---|---|---|
| Covertype (2,079,900 B with zstd-3; 834,271 B OpenZL csv-9) | `mcsv_planes_huf` | −65.7% | −15% / −16% (fits, fits) | −14.5% | −32% (fits) |
| | `mcsv_prod3` | **−73.0%** | −4% / −3% (fits, fits) | −32.8% | −44% (fits) |
| | `mcsv_prod19` | −76.1% | +700% | −40.4% | −17% (fits) |
| | production `kolmo_fast` | −72.6% | −8% / −8% (fits, fits) | −31.7% | −44% (fits) |
| | production `kolmo_hc` | −76.4% | +606% | **−41.1%** | −22% (fits) |
| | OpenZL csv-6 | −58.6% | +175% | +3.3% | −8% (fits) |
| CollegeMsg (378,073 B zstd-3; 323,766 B OpenZL csv-9) | `mcsv_planes_huf` | **−36.1%** | −15% / −11% (fits, fits) | −25.4% | −34% (fits) |
| | `mcsv_prod3` | −34.5% | −1% / −0% (fits, not every pair) | −23.5% | −32% (fits) |
| | `mcsv_prod19` | −39.3% | +304% | −29.1% | −25% (fits) |
| | production `kolmo_fast` | −38.2% | +2% / +4% (does not fit) | −27.8% | −36% (fits) |
| | production `kolmo_hc` | −40.0% | +258% | **−29.9%** | −28% (fits) |

Smallest pipeline inside the rival's cost on every pair:
- **Speed (zstd-3):** Covertype → `mcsv_prod3` −73.0% (production `kolmo_fast` −72.6% also fits); CollegeMsg →
  `mcsv_planes_huf` −36.1% (production fast does not fit).
- **Archive (OpenZL csv-9):** Covertype → production `kolmo_hc` −41.1% (typed `mcsv_prod19` −40.4%); CollegeMsg →
  production `kolmo_hc` −29.9% (typed −29.1%). Every typed and Kolmo policy beats OpenZL csv-9 by 14–41% at lower
  archive cost; OpenZL's own csv-6 is larger than its csv-9.

## NPY inputs (re-measured frozen speed policies)

Fits zstd-3 on every pair and smaller, of 8: `tf_planes_huf` 6 (first run; one pair over on one input) and 7
(confirmation); `tf_planes_huf_pg16` 7 and 7, fitting on 8/8 (parity on cover_control). Consistent with P2-E3.

## Interpretation (development; no P2 acceptance)

1. Type-first CSV columns are the speed-mode winner on both tables (on Covertype by 0.4 points over production
   fast, which also fits; on CollegeMsg production fast does not fit). The type-first idea works for tables as well as
   arrays.
2. In archive mode, Kolmo production `hc` and the typed `mcsv_prod19` both beat OpenZL's CSV profile by ~30–41% at
   lower modeled archive cost; production `hc` is marginally smaller. Typed planning does not add archive bytes
   here; its advantage is cost.
3. Limits: two derived, correlated tables from observed sources; derived CSV is not an original published file;
   research container (decision P2-D1). Next: fresh whole-file CSVs, registered before reading.
