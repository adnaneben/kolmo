# Provisional proposals register

Every proposal on this branch that was kept **provisionally**, **deferred**, **kept only as a control or for a narrow
workload**, or flagged for later review. Rejected items are listed at the end only where a later result could
reopen them. Update an entry in place (status, date, evidence) whenever new evidence arrives; never delete an entry —
mark it **retired** with the reason.

Last updated: 2026-11-12, after X-01 and the parallel review R1 (`25b1c80`). Both workstreams are covered: the production-connected harness
(`E02C-harness/`, `E02E/`, `E02F/`) and the parallel forced-typed line (`E02C/`, `E02D/`, `E03/`).

**Evidence rule for revisions.** All data used so far (the E02C, E02C-harness, E02D, E02E and E02F corpora) is now
*observed*. Any revision that changes a rule, threshold or candidate must be tested on fresh, preregistered,
source-disjoint data, and release decisions need an audited holdout (see D-01).

## A. Per-plane leaves and leaf rules (production-connected line)

| ID | Proposal | Status | Evidence | Would confirm | Would retire | Revisit when |
|---|---|---|---|---|---|---|
| P-01 | **Per-plane KG_INT residual leaves** (hybrid/zstd profile): one leaf per residual byte plane, production leaf rule per plane. Separate leaves would need a new graph node (`KG_SPLIT`); R1 shows a single-codec variant fits existing leaf chunk framing (P-17) | **Provisional, one family, twice.** Not supported as a general layout; format change **not justified** | Dev: −1.94% fast / −0.64% hc on INT files (`E02C-harness/RESULTS.md` §9). E02E: fast provisional (GHCN −4.23%), hc not supported. E02F: fast and hc provisional (pageviews −9.57% / −7.48%); EEG and speech ≈ 0 | Restricted to P-02's phenotype, ≥ 2 fresh families meet size and decode bars | A fresh phenotype-selected validation fails in all exercised families | P-02 is designed, or count-like series become a target workload |
| P-02 | **Sparse-high-plane selection**: use plane leaves only for blocks whose high residual planes are sparse or near-constant (precipitation, pageviews, spiky counts) | **Hypothesis, untested** (post hoc from E02E + E02F) | Gains of 7–16% on sparse series and ≈ 0 on dense signals, across both validations | Preregistered selector with a cheap sparsity test, costed as in E03, passing on ≥ 2 fresh families | Selector cost exceeds its gain, or no gain beyond E03's native baseline | Together with E03's selector work (X-01) |
| P-03 | **Sparse-leaf guard for the fast leaf rule**: when the 3 × 32 KiB sampling picks Huffman-only for a leaf ≥ 256 KiB, also try zstd and keep the smaller | **Deferred: production-review candidate.** Changes output bytes, not the format | Dev: one real misfire (164,040 B where zstd needs 2,996 B; `E02C-harness` §9), plus a constructed selftest case. Never triggered on E02E or E02F data | Misfire frequency and saved bytes measured on broad production frames (all transforms, not only KG_INT), at an acceptable encode cost | Misfires are vanishingly rare on representative data | Before any change to `kg_serialize`'s fast path |
| P-04 | **Charged size selector over representations with zstd-3** (`sel+zstd3`) | **Kept only for a size-oriented fast-profile study**; likely superseded | Dev: −0.20% complete-input (fast) at about 1.5× encode, +11% decode (`E02C-harness` §5) | — | Superseded by P-02 / X-01, or the E03 baseline already captures its gain | When the X-01 reconciliation is done |
| P-05 | **Global patched packing** (`gpp`) | **Deferred, workload-specific** | Dev: wins only on a few outlier-heavy 64-bit inputs (fs_inode −4.7%, goog_volume −4.4%) | Covered by P-12 if pursued | — | With P-12 |
| P-06 | **Local patched packing, 128 values** (`lpp128`), as a selector candidate only | **Kept as a selector candidate** | Dev: best entropy-only packed representation; still +3.95% vs production (`E02C-harness` §5) | Covered by X-01 / P-11 / P-12 | — | With X-01 |
| P-07 | **Entropy-only coding per plane** (`planes+ent`: Huffman/rANS per plane, no zstd) — native profile | **Upgraded by X-01: strongest measured lever for a zstd-free profile**, decode cost open | X-01 (`E02C-harness/X01.md`): −1.7% to −18.5% vs the E03-style native baseline in every cohort and every multi-byte family (EEG −10% to −12%, speech −2.6%); decode 1.5–1.7× production | R2: costed aligned-chunk encoder with decode-only timing on these cohorts, then fresh validation | Decode cost unacceptable for every application profile | R2 |

## B. Packing and selection (parallel forced-typed line)

| ID | Proposal | Status | Evidence | Would confirm | Would retire | Revisit when |
|---|---|---|---|---|---|---|
| P-10 | **Bounded-width global packing** | **Retained provisionally**; size screen passed on two families, no fast-path support | `E02C_RESULTS.md`; E02D H4 (appliances 24.48%, bicycle 5.30% with Huffman) | Fresh families, against the costed native baseline (P-14) | No gain beyond P-14 | X-01 |
| P-11 | **Local packing for changing-width data** | **Provisional, one family** (household; see D-01) | E02D H2: −36.33% vs matched control with Huffman; local decode cost fails the ≤ 1.10 screen in aggregate | ≥ 2 fresh families, excluding UCI 235 | — | X-01 |
| P-12 | **Patched packing for sparse large outliers** | **Provisional, one family** (household; see D-01) | E02D H1: −38.99% (Huffman) / −18.14% (rANS) vs matched control | ≥ 2 fresh families, excluding UCI 235 | — | X-01 |
| P-13 | **Packed storage for small streams** | **Not supported against the stronger baseline** (beats rANS, not Huffman) | E02D H3 | — | Already effectively retired unless the baseline changes | Only if small-message framing changes |
| P-14 | **Costed native baseline**: per leaf, the smallest of stored/Huffman/rANS, all trials charged | **Retained as the experimental control** | E03: −20.88% vs fixed Huffman, −4.98% vs fixed rANS, 1.26× encode | Use as the comparator for every packing or selector claim | — | Always |
| P-15 | **E03 sampled optional-packing selector** | **Do not promote; retained as a control** | E03: −5.14% beyond P-14 at 1.716× encode; zero families pass the screen | A new, entropy-aware rejection rule (main cost: unproductive full trials, 43% of encode), preregistered on fresh data | — | X-01 |
| P-16 | **E03 decode-speed observation** (0.935× decode ratio) | **Not a finding**: timing noise and allocation-history effects | E03 timing caveat | Order-balanced, decode-only, warm/cold-controlled timing | — | Before any decode-performance claim |

## A2. Proposals from the parallel review R1 (`research/codec/review-20261112/`)

| ID | Proposal | Status | Evidence | Would confirm | Would retire | Revisit when |
|---|---|---|---|---|---|---|
| P-17 | **Plane-aligned Huffman chunks inside one existing leaf**: no new graph node, decodable by the unmodified leaf decoder | **Feasibility shown; costs unmeasured** | R1: compatibility probe (64 plane/chunk cases, 58 archived residual replays) | R2 measures complete framing, encode/selection cost and decode-only timing | Gains vanish once chunk metadata and selection are charged | R2 |
| P-18 | **rANS encoder accepting single-symbol chunks** (frequency = `RANS_M`), same decoder | **Feasibility shown**; reopens "rANS alone per plane" | R1: 272 single-symbol cases decode with the pinned decoder | R2 aligned-rANS control | Huffman's repeated-byte mode dominates in every case | R2 |
| P-19 | **R1 algorithm backlog**: constant/zero-run coding, separate control/exception streams, magnitude token + exact offset bits (pcodec-style), local FOR, Rice with escape, small dictionaries, cost-aware trial gate, reused statistics/scratch | **Backlog, untested here** | R1 §3 | One focused, preregistered experiment each, against P-14 | — | After R2 |
| P-20 | **Application profiles** (one-time-encode/many-read, ingest-speed, tiny-message) for research Pareto decisions; TARGETS.md unchanged | **Proposed** | R1 §4.7 | Profiles stated before the experiments that use them | — | Before the next selector decision |

## A3. Stacks and amplifiers (see `STACKING.md`)

| ID | Combination | Status |
|---|---|---|
| ST-1 | Per-plane leaves + sparse-leaf guard (P-01 × P-03) | **E02G: provisional in both modes; size met in both target families** (fast −1.51% / −0.67%, hc −1.17% / −2.00%); one decode-ratio miss per mode (1.075×, 1.18×). Guard amplifier reproduced on fresh data (EEG +6.9%/+14.4% → 0.00%). **E02G-verify: both decode misses reproduced** (1.09× fast, 1.20× hc, outside identical-frame noise); cost concentrated in few files, not proportional to gain. Revise when a decode-cost gate (ST-2) is validated |
| ST-2 | Per-plane leaves gated by KG_INT parameters (order 0 or width ≥ 4) | Context amplification observed in development; used as E02G eligibility. E02G adds a decode reason for an encoder-side gate: ineligible dense EEG would cost +33% hc decode for ≈ 0 size |
| ST-3 | Entropy-only planes + aligned chunks + singleton rANS | Owned by R2-stack |
| ST-4 | Per-plane layout + per-file coder or packing choice | Offline ceiling −2.6% to −7.2% (fast); test after R2-stack with a cheap gate |
| ST-7 | Leaf improvements fed back into the planner's transform choice | Untested; development test after ST-1 |

## A4. Competitor-bounded (P2) proposals from the 12/13 November overnight run (development unless stated)

Under P2 (`benchmark/accept/ACTIVE_POLICY.json`) a candidate must be the smallest complete pipeline whose measured
cost fits the mode rival (zstd-3 for rt/fast). Entries here are judged on that rule; old percentage bars do not apply.

| ID | Proposal | Status | Evidence | Validation needed | Revise when |
|---|---|---|---|---|---|
| P2-01 | **Type-first planning for declared arrays**: read the NumPy/WAV declaration, then build the KG_INT graph directly (no planner analysis) | **Provisional, two boundaries** | P2-E1: production `fast` fits zstd-3's cost on 0/20 dev inputs, typed-first cuts cost sharply; P2-E3 (Actions 43302008497, CLI, pinned zstd-3): with P2-03 fits and is smaller on 7/8 typed NPY (production: 0/8) | Fresh source-disjoint whole original files with declared types | A fresh validation shows no feasible-and-smaller majority |
| P2-02 | **Identity-preserving fast KG_INT encoder and inverse** (`P2-E2/fastint.inc`): same choice, parameters, residual and output as production, specialised loops | **Correctness established on development data**; production-review candidate (no format change) | 19/19 typed inputs byte-identical (orders 0–2, widths 2/4/8, output widths 1–8, gcd 1/100); 3.3 M differential fuzz cases agree with `kg_int_inv`; removing it loses 6 feasible inputs (14 → 8) | Review against production sources; broader identity corpus incl. every planner-produced INT block | Any identity mismatch |
| P2-03 | **Per-plane Huffman leaves on type-first residuals** (cost stack: P2-01 + P2-02 + planes + Huffman) | **Provisional** | P2-E2 dev: fits 14/20, feasible-and-smaller 8/20; P2-E3 CLI: 7/8 both runs; faster than zstd-3 end to end on 7 dev inputs even with free transfer | As P2-01 | — |
| P2-04 | **Pre-encode applicability guard** (predictor bit estimate vs a 16 KiB zstd probe) | **Provisional; cost is the open issue** | Bounds the worst case (fs_mtime +340% → 0.0%; cover_control +46.6% → 0.0%) but costs as much as it saves on sub-0.3 MB inputs | A zero-probe signal validated on fresh data | A cheaper signal matches its protection |
| P2-05 | **Zero-run residual leaf** (runs + nonzero planes; fused run-aware inverse) | **Byte-positive, cost-neutral; reopenable** | P2-E4: mri −28.9% vs −15.9%, fs_inode −75.2% vs −48.3% (vs zstd-3); auto variant ties P2-03 on count | Encode-side cost reduction, then fresh sparse families | — |
| P2-06 | **Type-first CSV columns** (canonical INT, fixed-scale DEC as exact mantissas, TEXT stream) | **Provisional (development)**; P2-E7 measuring vs OpenZL csv-9 | P2-E5/E6: −25% to −88% bytes vs zstd-3 where numeric columns exist; fits zstd-3 cost on all-integer dem/goog; 310/310 typing agreement with an oracle | Fresh public CSVs (whole original files) | P2-E7 / fresh data |
| P2-07 | **Wide-table layout**: >= 16 numeric columns and < 4096 rows → one column-major stream | **Development rule, frozen for validation** | P2-E5: dem cost +25% → −19%; mri bytes −6.1% → −25.4%; wrong for narrow tables (goog) | Fresh wide tables | — |
| P2-08 | **Probe-based leaf selection** (Huffman, then a zstd probe to decide) | **Negative under P2 at these sizes** | P2-E2: charged trials exceed the bytes saved on sub-0.3 MB inputs | Only for large inputs (>= several MB) | Large-input evidence |
| P2-09 | **Type-first stack in the existing production format** (P2-E11): one KG_INT graph block in a standard `CZN1` frame; per-plane Huffman as plane-aligned chunks inside one ordinary `C_HUF` leaf; decoded by the unchanged decoder/CLI | **Provisional (development)**; encoder-only change | 60/60 frames decode with production `kolmo -d`; `tp_prod3` payload identical to `kg_encode_num` (19/19); dev in memory: fits 13/20, feasible-and-smaller 9/20 (production fast 0) | N1 CLI boundary with production-CLI decoding, then fresh whole files | — |
| P2-10 | **Fused-order predictor choice** (P2-E9): identical decision and cost, fewer sweeps | **Correctness established**; production-review candidate | 200,000 random arrays + 19/19 inputs identical | Runner timing | Any mismatch |
| P2-11 | **Archive mode frontier** (P2-E8): within OpenZL csv-9's archive cost, production `hc`/`max` are smallest (−41.1% / −30.6%); `max`/`ultra` smaller but over cost at sub-MB sizes | **Measured (development)** | P2-E8 | Larger files / longer retention; typed NPY rival | Typed OpenZL NPY comparator (N1 owner) |
| P2-12 | **Cohort breadth** (P2-E10, 61 inputs/11 families): type-first per-plane Huffman fits zstd-3 and smaller on 43/41; production 0/61; archive: Kolmo pipeline smallest within OpenZL-9 typed cost on 47/61 | **Measured (development)** | P2-E10 | Observed sources only; near-boundary GHCN/FSDD vary between runs; OpenZL `le-*` reads headers as values | Fresh source-disjoint validation; exact NPY adapter |
| P2-13 | **Production-format, production-decoded** (P2-E12): `tp_phuf` fits zstd-3 and smaller on 9/9 of 10 N1 inputs; `tp_prod3` best on 5 typed columns (−37…−64%); production `hc` 0/10 | **Measured (development)** | P2-E12 | Same 10 observed inputs as N1/P2-E3 | Fresh source-disjoint validation |
| P2-14 | **Leaf selection by trial is cost-negative vs zstd-3** (P2-E13, in-memory): both-leaves 1/20, predicted leaf 2/20 vs single-leaf 5–6/20 | **Measured negative (development, local)** | P2-E13 | Local noisy container; CLI boundary not run (start-up may dilute) | CLI-boundary check of `tb_best`; near-free learned ranker |
| P2-D1 | **Open user decision**: may a *research container* (complete, checksummed, not a production frame) count as a `complete_pipeline` for P2 research acceptance? `policy.py` currently rejects it, which blocks every type-first result regardless of data | **Decision still useful, no longer blocking the main result**: P2-09 writes the same stack as production frames | All P2-E1…E7 records | — | User decision |

ST-1/ST-2 under P2: E02G's decode misses are real (E02G-verify) but not what blocks P2 feasibility; production
`fast` is outside zstd-3's budget because of encoder analysis (P2-E1).

## C. Cross-links and process items to revise

| ID | Item | Status | Action |
|---|---|---|---|
| X-01 | **Reconcile per-plane leaves with E03's costed native baseline** | **Done** (`E02C-harness/X01.md`, analysis of committed records) | Per-plane beats the native baseline in every multi-byte family; the narrow 'sparse planes' result applies only against zstd. Decode-only timing is still owed (R2) |
| D-01 | **UCI 235 (household power) is in the release holdout** (`benchmark/holdout/inputs.json`) and was used by E02D and E03 | **Open; owner decision** | E02D/E03 household results are measured evidence, not pristine validation (acknowledged in `E03_RESULTS.md`). Decide whether that holdout entry must be replaced before any release decision |
| D-02 | **Observed corpora** (exclude from future validation): E02C-harness development corpus; parallel E02C (digits, Gapminder, Nile, sunspots); E02D (UCI 235, 492, 275, 374); E02E (GHCN-Daily USW00094728/USW00023174/USW00014739, MIT-BIH 100/101/200/201); E02F (eegmmidb S001–S004 R01, Wikimedia pageviews for 8 articles, FSDD) | **Standing.** Now observed: E02G's Backblaze Q1 2023, OWID COVID-19, eegmmidb S005–S008; R3-guard's UCI Covertype (31) and SNAP CollegeMsg. Also exposed via `benchmark/frontier/protocol.md`: NYC TLC yellow taxi, GH Archive | Check new registrations against this list and against `benchmark/` manifests |
| D-03 | **E02F's decode rule** (per input byte) was set after seeing E02E | **Disclosed** | Did not decide any E02F outcome; keep the disclosure if the rule is reused |
| D-04 | **Terminology clash**: "local byte planes" in E02C/E02D (per-128-value width-rounded planes, a packing control) ≠ "planes" in E02C-harness/E02E/E02F (one leaf per residual byte plane) | **Standing** | Never pool or cross-cite their numbers as the same thing |
| D-05 | **No CI runs the E02C-harness self-check** (only the E02E/E02F validation workflows build it) | **Open** | Add a research CI job if the harness keeps changing |

## D. Rejected, but reopenable only under stated conditions

| Item | Rejected in | Reopen only if |
|---|---|---|
| Unconditional global/local/patched packing before zstd, Huffman or rANS | E02C (both lines), E02D | The leaf coder changes fundamentally |
| rANS alone per plane | E02C-harness §8 | **Reopened by R1 (P-18)**: a singleton-capable encoder works with the existing decoder |
| Global bitpacking (`gbp`) as a fixed layout | E02C-harness §5 | — |
