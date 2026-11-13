# P2-E8 results — archive frontier on N1's complete-CLI boundary (development)

Run: Actions **43313535706**, 21:52–22:11 UTC (~19 runner-minutes), workflow at `46af42d`. Evidence recovered from the
job log; 13/13 files match the run's `MANIFEST.json`. Sanitizer gate for `mcsv_prod19` passed. N1 `confirm`:
**IDENTITIES_MATCH** (130 configurations, 114 complete; 16 = OpenZL `-p numpy`, not provided by v0.3.0). Both runs
give the same smallest-feasible pipeline on every input.

## CSV views vs the designated archive rival (OpenZL `csv` level 9)

Smallest complete pipeline inside OpenZL csv-9's modeled archive cost on every pair (P2 archive model):

| Table | OpenZL csv-9 bytes | Smallest feasible | Bytes vs csv-9 | Smaller but over cost |
|---|---|---|---|---|
| Covertype | 834,271 | production `kolmo -m hc` | **−41.1%** | `max` −43.5%, `ultra -l 1` −46.8% |
| CollegeMsg | 323,766 | production `kolmo -m max` | **−30.6%** | `ultra -l 1` −42.2% |

Typed `mcsv_prod19` is feasible on both (−40.4%, −29.1%) but marginally larger than the production winners.

## NPY inputs (informative only: generic `openzl_serial_9`; the designated typed rival is BLOCKED)

Smallest feasible vs serial-9: production `hc`/`eco`/`max` on 5/8 (−32% to −68%), `kolmo_fast` on collegemsg
target_id (−3.4%), zstd-3 on cover_control (−13.3%); `max`/`ultra` are smaller on all 8 but over cost on most.

## Correction to the registered premise

`PROTOCOL.md` said storage dominates the archive cost at these sizes. Measured (first run, Covertype): storage is
71% of OpenZL csv-9's cost, but heavier Kolmo modes buy only 2–4% more bytes for 2.4× (max) or ~40× (ultra) more
encode CPU (`ultra`: 98% of its cost is CPU). The archive model is storage-heavy yet still CPU-sensitive at
sub-megabyte sizes; it would favour heavier modes for larger files or longer retention, which this run did not test.

## Interpretation (development; no acceptance)

Under P2's archive rule, production Kolmo (`hc`, `max`) is the smallest feasible pipeline on both CSV views, 31–41%
below OpenZL's CSV profile; type-first planning matches it within 1 point at similar cost. No archive result for
arrays until a typed OpenZL comparator exists. Observed, derived, correlated tables only.
