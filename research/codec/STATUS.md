# Codec research status (isolated branch `fix/planner-record-ownership-isolated-20261112`)

**Provisional proposals to revisit later: [`PROVISIONAL.md`](PROVISIONAL.md)** (both workstreams; update entries in place).

Layout: `E02C/` holds the parallel forced-typed E02C implementation, frozen by the E02D preregistration
(`E02D/PREREGISTRATION.md`); `E02C-harness/` holds the separate production-connected harness and its results (moved
there unchanged so the frozen directory stays byte-identical); `E02D/` is the preregistered validation.

**Competitor-bounded (P2) overnight work, 12/13 November** (development; see `PROVISIONAL.md` §A4 and each
directory's RESULTS.md): P2-E1 complete-pipeline baseline (production `fast` never inside zstd-3's cost on 20 dev
inputs); P2-E2 type-first fast integer path, identity-checked against production; **P2-E3 on N1's complete-CLI
boundary (Actions 43302008497): type-first per-plane Huffman smaller than the zstd-3 CLI and inside its round-trip
cost on 7/8 typed NPY inputs in both runs, production 0/8**; P2-E4 zero-run leaf; P2-E5/E6 typed CSV; P2-E7/E8
typed CSV vs OpenZL csv-9 (Kolmo smallest within cost on both tables); **P2-E10 on 61 inputs: type-first
per-plane Huffman fits zstd-3 and is smaller on 43/41, production 0/61**; P2-E11 writes it as production frames; **P2-E14 (production frames, production CLI decode, 61 inputs): guarded
per-plane Huffman fits zstd-3 and is smaller on 49/47, production 0/61; P2-E17 archive: a Kolmo pipeline is
smallest within OpenZL typed-9 cost on 47/61** (OpenZL keeps GHCN; P2-E18 explains why). **Fresh preregistered validation P2-E20: archive on fresh
audio PASS (35/40 vs OpenZL); speed rule 60–62.5% FAIL vs its CP95 bar; fresh CSV archive FAIL; P2-D1 closed (not useful).** Archive NPY uses OpenZL `le-*` profiles on whole files (exact NPY adapter pending). No P2
acceptance: development data and research containers (open decision P2-D1).

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

X-01 done (`E02C-harness/X01.md`): against the E03-style native baseline, per-plane entropy coding gains in every
multi-byte family (−1.7% to −18.5%); its narrow result applies only against zstd.

R2 is owned by the parallel workstream (`R2-stack/PROTOCOL.md`). Stacking map: [`STACKING.md`](STACKING.md) — ST-1
(per-plane leaves + sparse-leaf guard) shows a development amplifier.

E02G preregistered (`E02G/PREREGISTRATION.md`): validation of the stacked pipeline (per-plane leaves + sparse-leaf guard,
KG_INT-parameter phenotype) on Backblaze drive stats and OWID COVID counts, with dense EEG and single-byte controls.
It consumes those sources; the native-compatible validation proposed by R2-stack must use others.

E02G executed (Actions 43272770322): both modes **provisional**; the stacked candidate is smaller in all four target ×
mode cells (−0.67% to −2.00%), each mode misses one large-file decode-ratio bar (1.075×, 1.18×), controls passed, and
the guard amplifier reproduced on fresh data. See `E02G/RESULTS.md`. The parallel R3-guard validation uses UCI
Covertype and SNAP CollegeMsg (disjoint from E02G).

E02G-verify executed (Actions 43280075566): decode-only, noise-controlled timing of the identical E02G frames.
Both decode misses are **reproduced** in warm and cache-displaced regimes (Backblaze fast 1.087× / 1.090×, OWID hc
1.198× / 1.206×; identical-frame noise bands within ±6%). The cost is concentrated in a few files and not proportional
to size gain (hc EEG 1.33× for ≤ 0.10%). E02G's verdict is unchanged. See `E02G-verify/RESULTS.md`.

Next substantive task: on development data only, attribute decode time per leaf (coder and leaf count per plane),
then specify a gain-per-decode-cost gate for per-plane leaves (ST-2 as a decode gate). Validate it only on fresh,
preregistered sources.

Not started / not authorised: LZ engine, float or JSON codecs, neural selector, transform backlog, default or
threshold changes, merges, releases.
