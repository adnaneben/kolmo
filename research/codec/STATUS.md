# Codec research status (isolated branch `fix/planner-record-ownership-isolated-20261112`)

**Provisional proposals to revisit later: [`PROVISIONAL.md`](PROVISIONAL.md)** (both workstreams; update entries in place).

Layout: `E02C/` holds the parallel forced-typed E02C implementation, frozen by the E02D preregistration
(`E02D/PREREGISTRATION.md`); `E02C-harness/` holds the separate production-connected harness and its results (moved
there unchanged so the frozen directory stays byte-identical); `E02D/` is the preregistered validation.

Updated 2026-11-12. Research only: no default, format, threshold or release change is proposed here.

| Item | State | Evidence |
|---|---|---|
| Planner record-ownership repair | Committed at `2f3308a` (`src/kolmo.c` `e06467c8…`); focused validator re-run locally and passed; CI runs 43187874283 / 43187874421 / 43187874238 reported successful (also recorded in `E02C/BASELINE.md`) but their artifacts were **not verified in this session** (no GitHub API access) | [`E02C-harness/BASELINE_EVIDENCE.md`](E02C-harness/BASELINE_EVIDENCE.md) |
| E02F per-plane leaves on multi-byte residuals | **Executed** (Actions 43260123241). Calibration passed; all three families exercised. H1 fast and H2 hc **provisional**: met only by pageviews (−9.57% / −7.48%); EEG (+0.08% / −0.02%) and speech (−0.16% / −0.47%) fail. With E02E: per-plane leaves are not a general KG_INT layout; gains track sparse high planes. `KG_SPLIT` not justified | [`E02F/RESULTS.md`](E02F/RESULTS.md) |
| E02E per-plane leaf validation | **Executed** (Actions 43255531577). Calibration passed. H1 fast **provisional** (GHCN −4.23%, decode 1.02×; MIT-BIH not exercised); H2 hc **not supported** (GHCN −3.72% but decode 1.105× > 1.05×; MIT-BIH residuals are single-byte, 0.00%); guard never triggered. No format change justified | [`E02E/RESULTS.md`](E02E/RESULTS.md) |
| Holdout overlap note | E02D source UCI 235 (household power) is also listed in the release holdout `benchmark/holdout/inputs.json`; E02D's preregistration does not mention it. Not changed here; flagged for the E02D owner | `benchmark/holdout/inputs.json` |
| E02D source-disjoint validation of the parallel implementation's candidates | Preregistered (`e6bf564`) and executed by the parallel workstream; results recorded in `91c4cda` (`E02D_RESULTS.md`), **deliberately not read in this session** so E02E stays blind. Per-plane leaves are not in its frozen matrix | [`E02D/PREREGISTRATION.md`](E02D/PREREGISTRATION.md) |
| E02C follow-up 2: level-matched per-plane leaves (production leaf rule) | **Complete.** −1.94% (fast) / −0.64% (hc) on files with INT blocks, decode within ~1.5%; kept for integration research pending a format-change review, a sparse-plane guard and larger validation. Found: the fast leaf rule's sampling can choose Huffman-only for sparse large leaves (164 KB vs 3 KB on one plane) | [`E02C-harness/RESULTS.md`](E02C-harness/RESULTS.md) §9 |
| E02C follow-up: plane-aware controls | **Complete.** One leaf per residual byte plane with zstd -3 is −0.37% (fast) / −0.58% (typed) with equal encode, +3.6% decode — the only selector-free gain; best entropy-only per plane +1.19% (fast). Existing rANS fails on single-symbol chunks. Per-plane leaves would need a KG format change (not proposed here) | [`E02C-harness/RESULTS.md`](E02C-harness/RESULTS.md) §8 |
| E02C residual representations (global/local bitpacking, global/local patched packing × stored/Huffman/rANS/zstd-3) | **Complete — negative.** No fixed combination beats unmodified Kolmo on real development data; a charged zstd-3 selector saves 0.20% (fast) at ~1.5× encode. GBP, LBP, fixed GPP/LPP rejected; LPP/GPP kept only as selector candidates; sel+zstd3 kept only for a size-oriented profile study | [`E02C-harness/RESULTS.md`](E02C-harness/RESULTS.md) |
| E02C experimental C checks (this harness) | ASan/UBSan/LSan selftest, golden vectors (two builds) and independent Python reference passed locally (clean worktrees of `41bcc96` and of the final tree); **no CI job runs them** — `e02c-research.yml` builds only the parallel implementation | `make -f research/codec/E02C-harness/Makefile check` |
| E02C parallel forced-typed implementation (`c880659`/`706cdab`: `build.py`, `probe.c`, `run.py`, zstd-1 control) | Executed by the parallel workstream (Actions 43232093686, 43233624953; reported there, not re-checked here): unconditional packing rejected; global/local/patched packing retained only as workload-specific candidates | [`E02C_RESULTS.md`](E02C_RESULTS.md), `E02C/README.md` |
| Previous result "packing before zstd enlarges" | Reproduced (A real +13–25%, B fast +2.3–7.4% vs existing layout with zstd-3) | `E02C-harness/RESULTS.md` §1 |

Next substantive task: X-01 in `PROVISIONAL.md` — reconcile per-plane leaves with E03's costed native baseline on
one measurement boundary, before any further selector or packing work.

Not started / not authorised: LZ engine, float or JSON codecs, neural selector, transform backlog, default or
threshold changes, merges, releases.
