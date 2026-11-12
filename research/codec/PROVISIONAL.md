# Provisional proposals register

Every proposal on this branch that was kept **provisionally**, **deferred**, **kept only as a control or for a narrow
workload**, or flagged for later review. Rejected items are listed at the end only where a later result could
reopen them. Update an entry in place (status, date, evidence) whenever new evidence arrives; never delete an entry —
mark it **retired** with the reason.

Last updated: 2026-11-12, after E02F (`a0c7eb7`). Both workstreams are covered: the production-connected harness
(`E02C-harness/`, `E02E/`, `E02F/`) and the parallel forced-typed line (`E02C/`, `E02D/`, `E03/`).

**Evidence rule for revisions.** All data used so far (the E02C, E02C-harness, E02D, E02E and E02F corpora) is now
*observed*. Any revision that changes a rule, threshold or candidate must be tested on fresh, preregistered,
source-disjoint data, and release decisions need an audited holdout (see D-01).

## A. Per-plane leaves and leaf rules (production-connected line)

| ID | Proposal | Status | Evidence | Would confirm | Would retire | Revisit when |
|---|---|---|---|---|---|---|
| P-01 | **Per-plane KG_INT residual leaves**: one leaf per residual byte plane, production leaf rule per plane; would need a new graph node (`KG_SPLIT`, op 7) | **Provisional, one family, twice.** Not supported as a general layout; format change **not justified** | Dev: −1.94% fast / −0.64% hc on INT files (`E02C-harness/RESULTS.md` §9). E02E: fast provisional (GHCN −4.23%), hc not supported. E02F: fast and hc provisional (pageviews −9.57% / −7.48%); EEG and speech ≈ 0 | Restricted to P-02's phenotype, ≥ 2 fresh families meet size and decode bars | A fresh phenotype-selected validation fails in all exercised families | P-02 is designed, or count-like series become a target workload |
| P-02 | **Sparse-high-plane selection**: use plane leaves only for blocks whose high residual planes are sparse or near-constant (precipitation, pageviews, spiky counts) | **Hypothesis, untested** (post hoc from E02E + E02F) | Gains of 7–16% on sparse series and ≈ 0 on dense signals, across both validations | Preregistered selector with a cheap sparsity test, costed as in E03, passing on ≥ 2 fresh families | Selector cost exceeds its gain, or no gain beyond E03's native baseline | Together with E03's selector work (X-01) |
| P-03 | **Sparse-leaf guard for the fast leaf rule**: when the 3 × 32 KiB sampling picks Huffman-only for a leaf ≥ 256 KiB, also try zstd and keep the smaller | **Deferred: production-review candidate.** Changes output bytes, not the format | Dev: one real misfire (164,040 B where zstd needs 2,996 B; `E02C-harness` §9), plus a constructed selftest case. Never triggered on E02E or E02F data | Misfire frequency and saved bytes measured on broad production frames (all transforms, not only KG_INT), at an acceptable encode cost | Misfires are vanishingly rare on representative data | Before any change to `kg_serialize`'s fast path |
| P-04 | **Charged size selector over representations with zstd-3** (`sel+zstd3`) | **Kept only for a size-oriented fast-profile study**; likely superseded | Dev: −0.20% complete-input (fast) at about 1.5× encode, +11% decode (`E02C-harness` §5) | — | Superseded by P-02 / X-01, or the E03 baseline already captures its gain | When the X-01 reconciliation is done |
| P-05 | **Global patched packing** (`gpp`) | **Deferred, workload-specific** | Dev: wins only on a few outlier-heavy 64-bit inputs (fs_inode −4.7%, goog_volume −4.4%) | Covered by P-12 if pursued | — | With P-12 |
| P-06 | **Local patched packing, 128 values** (`lpp128`), as a selector candidate only | **Kept as a selector candidate** | Dev: best entropy-only packed representation; still +3.95% vs production (`E02C-harness` §5) | Covered by X-01 / P-11 / P-12 | — | With X-01 |
| P-07 | **Entropy-only leaves per plane** (`planes+ent`: Huffman/rANS per plane, no zstd) | **Workload-specific only** (audio-like residuals without repeats) | Dev: +1.19% vs production fast, +55% decode (`E02C-harness` §8) | A workload that must avoid zstd (dependency or decode-model reasons) | — | If a zstd-free profile is ever required |

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

## C. Cross-links and process items to revise

| ID | Item | Status | Action |
|---|---|---|---|
| X-01 | **Reconcile per-plane leaves (P-01/P-02) with E03's costed native baseline (P-14)**, as E03's own next task requests | **Open** | Express both on one boundary (production frames vs the E2C1 envelope, the same leaf rule, decode-only timing) before choosing any selector line; do not pool incompatible tables |
| D-01 | **UCI 235 (household power) is in the release holdout** (`benchmark/holdout/inputs.json`) and was used by E02D and E03 | **Open; owner decision** | E02D/E03 household results are measured evidence, not pristine validation (acknowledged in `E03_RESULTS.md`). Decide whether that holdout entry must be replaced before any release decision |
| D-02 | **Observed corpora** (exclude from future validation): E02C-harness development corpus; parallel E02C (digits, Gapminder, Nile, sunspots); E02D (UCI 235, 492, 275, 374); E02E (GHCN-Daily USW00094728/USW00023174/USW00014739, MIT-BIH 100/101/200/201); E02F (eegmmidb S001–S004 R01, Wikimedia pageviews for 8 articles, FSDD) | **Standing** | Check new registrations against this list and against `benchmark/` manifests |
| D-03 | **E02F's decode rule** (per input byte) was set after seeing E02E | **Disclosed** | Did not decide any E02F outcome; keep the disclosure if the rule is reused |
| D-04 | **Terminology clash**: "local byte planes" in E02C/E02D (per-128-value width-rounded planes, a packing control) ≠ "planes" in E02C-harness/E02E/E02F (one leaf per residual byte plane) | **Standing** | Never pool or cross-cite their numbers as the same thing |
| D-05 | **No CI runs the E02C-harness self-check** (only the E02E/E02F validation workflows build it) | **Open** | Add a research CI job if the harness keeps changing |

## D. Rejected, but reopenable only under stated conditions

| Item | Rejected in | Reopen only if |
|---|---|---|
| Unconditional global/local/patched packing before zstd, Huffman or rANS | E02C (both lines), E02D | The leaf coder changes fundamentally |
| rANS alone per plane | E02C-harness §8 | rANS gains single-symbol chunk handling (today it stores the whole stream) |
| Global bitpacking (`gbp`) as a fixed layout | E02C-harness §5 | — |
