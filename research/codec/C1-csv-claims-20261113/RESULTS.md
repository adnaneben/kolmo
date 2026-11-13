# C1 — CSV competitive-claim audit
13 November 2026. Executed local, independent audit of the frozen E26/E27 artifacts. This is not a new benchmark, a new source-disjoint validation, production integration, or release approval.

## Decision
Retain the CSV picker as a conditional archive candidate against pinned, untrained OpenZL CSV-9. Its file-level wins reproduce from raw evidence, and the advantage survives conservative grouping of the known related files. Do not claim that the picker never regresses versus `eco`, that a hindsight fallback is implemented, or that this establishes universal OpenZL leadership.

## Actual fixed-encoder output, not hypothetical fallback

| Frozen cohort | Comparable original files | Smaller AND within cost in both passes | Picker actual bytes | OpenZL actual bytes | Actual reduction |
|---|---:|---:|---:|---:|---:|
| E26, unseen TidyTuesday datasets at original test time | 35 of 36 | 33/35 | 5,368,612 | 6,391,791 | 16.0077% |
| E27, cross-aggregator files at original test time | 28 of 28 | 26/28 | 4,193,751 | 4,972,885 | 15.6676% |

Actual totals include the losing comparable files. One E26 OpenZL parser failure on `week7_starwars` is explicitly excluded from the comparative denominator, not counted as a Kolmo win. The picker produced an archived success receipt for that input, but there is no corresponding successful rival measurement.

The older analysis's rule totals replace the candidate with OpenZL on files failing the size/cost rule. These are 5,366,694 bytes (16.0377% reduction) for E26 and 4,173,621 bytes (16.0724%) for E27. The replacement saves a hypothetical 1,918 and 20,130 bytes beyond the actual picker outputs. No such cost-aware fallback was executed inside the measured picker. In particular, the approximately 16.1% E27 headline is a hindsight frontier, not the fixed encoder's actual aggregate output. This audit supplies the correct distinction without changing the original study's registered verdict.

## Dependence and grouping sensitivity
Every archived original input's length and SHA-256 was checked. E27's `olist_orders_45d.csv` and `orders_45d.csv` are byte-identical. There are 27 unique input hashes among its 28 files, and no exact hash overlap between E26 and E27.

Using the outcome-independent relationship groups registered in C1 — adult train/test, bank_marketing train/test, har train/validate, Churn_Modelling variants, Norway car-sales variants, and the orders duplicates — E27 has **21 known groups**. The picker wins on **19/21 groups** under the conservative requirement that EVERY member win in BOTH passes. A lexicographically selected representative per group also gives 19/21. Removing only the exact duplicate gives 27 comparable files and a 15.7509% actual byte reduction.

The adjusted binomial lower bound for the grouped 19/21 sensitivity is approximately 0.6486 at alpha=0.05/6. This is a retrospective dependence sensitivity, not a new preregistered pass or evidence that the groups are independent random population samples. Additional shared provenance remains possible. E26 retains 35 comparable weekly-source groups, with 33 wins; different weeks are not certified independent populations either. Do not pool these into a claimed universal market win rate.

## Correction: the picker is not uniformly better than eco

| Picker relative to existing eco | E26 | E27 |
|---|---:|---:|
| Smaller output files | 8 | 16 |
| Identical size files | 19 | 1 |
| Larger output files | 8 | 11 |
| Added robust rival wins | 4 | 2 |
| Lost robust rival wins | 0 | 2 |
| Aggregate picker output relative to eco | 2.9896% smaller | 1.0234% larger |

E27's picker and eco both have 26/28 qualifying rival wins, but they are NOT the same 26 files. The picker gains LetterRecognition and bank_marketing_test while losing CNAE9 and Wage. Equal win counts do not establish non-regression.

On E27, eco stores **4,151,266 bytes**, 42,485 fewer than the picker's 4,193,751. Four inputs are worse under the picker in both stored size and archive cost across all six observed pairs: CNAE9, Wage, diamonds, and segmentationData. These are observed pairwise results, not immutable performance guarantees. The picker also loses on both metrics versus eco for three E26 inputs: BOTY-votes-2019, brisbane_complaints, english_education.

The problematic E27 cases are not negligible:
- CNAE9: picker 33,573 bytes versus OpenZL 20,211, a 66.11% expansion; worst observed archive-cost ratio 1.388.
- Wage: picker 39,638 versus OpenZL 32,870, a 20.59% expansion; one cost pair exceeds the rival despite a favorable median.

Retain these failures as development regression fixtures. Do not tune on them and then describe their recovery as fresh validation. The prior report's general sentence that the picker is never worse than eco is not supported by its own per-input measurements.

## What was verified
- External archive digests for E26 artifact 14027086734 / run 43403830702 and E27 artifact 14028561769 / run 43407137403.
- 6,182 manifest-member hashes and 64 original input instances (63 unique hashes).
- 768 records across both passes; 766 successful records plus two repeated receipts for the one unsupported OpenZL input.
- All 4,596 raw measured encode/decode operations; 2,298 paired speed/archive cost calculations; 5,362 saved medians.
- Complete policy/input/pair matrices, actual command configurations, return codes, timeouts, CPU-affinity declarations, input identities, repeated compressed-frame hash identities and recorded byte-exact flags.
- 43 local regression tests using explicitly synthetic audit fixtures, including duplicate/missing samples, malformed/nonfinite values, wrong rival profiles, missing pairs, corrupted costs/medians and incorrect fallback accounting.
- Deterministic clean rerun: identical audit JSON SHA-256 17d8d4719b340e26c4c051760abae00d5cca80a2b9524de144e129cdc809b366.

The archived workflows excluded compressed-frame payloads; this audit did NOT re-run decompression or independently hash those missing payload bytes. It checked the recorded successful decodes and repeat frame hashes, not a new codec execution. No new hardware performance data or real-codec sanitizer pass is claimed.

An installed Python site hook emitted an unrelated spreadsheet warmup warning on the initial successful audit. A standard-library-only `python -S` rerun was clean and produced byte-identical results; the warning log remains in the evidence package.

## Architecture and competitor boundaries
The measured cost is complete CLI execution under the frozen 30-day/3.3-read archive model, including the picker's regularity scan and any internal rejected route. The hypothetical OpenZL fallback is NOT included. These historical rate parameters are model assumptions, not current cloud-price quotes.

The pinned picker chooses table-zstd12 versus eco from line regularity before invoking the selected encoder. This does not establish the user's full architecture requirement of exact column typing before joint graph/leaf/cross-column planning. It is a useful measured route-selection heuristic, not the completed P2 planner.

The comparator is a pinned untrained OpenZL CSV-9 profile. Source-trained OpenZL, a preregistered stronger rival frontier, controlled target hardware, multi-thread scaling, matched integrity semantics and production deployment remain separate gates. Existing registered E26/E27 outcomes stay intact. No retrospective performance retuning, RL training, production patch, new workflow, automation extension, merge or release occurred.

## Next substantive task
Freeze one CSV continuation benchmark that retains eco and the fixed picker as separate deployable controls, includes the observed regressions, and compares a type-first research routing implementation with applicable pinned untrained and source-trained OpenZL under the same complete-input cost boundary. Charge schema/feature/route/training overhead. Use the current data only for development; source-group-disjoint final evaluation must be selected and registered before it is read. No guaranteed non-regression claim is permitted without a costed mechanism that establishes it.
