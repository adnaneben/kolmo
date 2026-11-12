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
| ST-1 | Per-plane leaves + sparse-leaf guard (P-01 × P-03) | **E02G: provisional in both modes; size met in both target families** (fast −1.51% / −0.67%, hc −1.17% / −2.00%); one decode-ratio miss per mode (1.075×, 1.18×). Guard amplifier reproduced on fresh data (EEG +6.9%/+14.4% → 0.00%). Next: noise-controlled decode-only timing of the E02G frames |
| ST-2 | Per-plane leaves gated by KG_INT parameters (order 0 or width ≥ 4) | Context amplification observed in development; used as E02G eligibility. E02G adds a decode reason for an encoder-side gate: ineligible dense EEG would cost +33% hc decode for ≈ 0 size |
| ST-3 | Entropy-only planes + aligned chunks + singleton rANS | Owned by R2-stack |
| ST-4 | Per-plane layout + per-file coder or packing choice | Offline ceiling −2.6% to −7.2% (fast); test after R2-stack with a cheap gate |
| ST-7 | Leaf improvements fed back into the planner's transform choice | Untested; development test after ST-1 |

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
