# E02C results — KG_INT residual representations (production-connected harness)

Method and framing: [`PROTOCOL.md`](PROTOCOL.md). These results come from `harness/e02c.c` only. The parallel
forced-typed implementation on this branch (`../E02C/README.md`, `probe.c`, `run.py`; zstd level 1 control) is measured by its own
workflow and is not summarised here; its numbers are not interchangeable with these (different zstd level, framing,
corpus and selector policy). Its executed results (`../E02C_RESULTS.md`, Actions runs 43232093686 / 43233624953) reach the
same direction: unconditional packing rejected, packing kept only for narrow workload hypotheses.

**Outcome: negative for every fixed packing representation; marginal for a charged per-block selector that needs zstd.
Follow-up (§8): the existing rANS failure is its single-symbol rejection, and one stream per byte plane coded with
zstd -3 (`planes+zstd3`) is the only fixed configuration smaller than production fast (−0.37%) — a layout finding, not a
packing one. Follow-up 2 (§9), level-matched with production's own leaf rule: per-plane leaves are −1.94% (fast) and
−0.64% (hc) on files with INT blocks, at equal decode speed.**
No representation/backend combination, used unconditionally, stored fewer complete-input bytes than unmodified Kolmo
on the real development inputs in either regime. A size selector over all five representations with the zstd -3
control saved 0.20% (fast planner, real inputs) at roughly 1.5× encode time. The earlier result is reproduced:
packing before zstd generally enlarges.

Scope: development data only (17 real/derived + 3 labelled synthetic inputs, 13.1 MB; [`corpus/manifest.json`](corpus/manifest.json)).
Nothing here is a holdout result, a release measurement or a general claim.

## Tested identities

| | |
|---|---|
| Production code | reference `2f3308a3380a140ad0f6873ae228de7715e5eef5`, `src/kolmo.c` `e06467c8…`, `src/kgraph.inc` `91ad067d…`, unmodified |
| Harness | committed in `cc42a00` (first committed as `41bcc96`, rebased unchanged onto the parallel work and later moved to `E02C-harness/`; the run used the byte-identical harness files, SHA-256 in `results/env.json`) |
| Binary | `build/e02c` SHA-256 `fc05802c…5c51`; `build/libvendor.a` `55709832…9fb8` |
| Toolchain / host | gcc 13.3.0 `-O3 -march=native -fno-plt -ffp-contract=off` (core + harness in one TU), vendor `-O3 -fno-plt`; zstd 1.5.7 (vendored); Intel Xeon @ 2.10 GHz, 2 vCPU shared VM, Linux 6.18 |
| Run | 1 thread; 7 repetitions; job order and configuration order shuffled per repetition (seed 20261112); all samples kept |
| Raw records | `results/raw.jsonl.xz` (uncompressed SHA-256 `3258da3d…7e2f`); tables `results/tables.md`; aggregates `results/summary.json` |

Every complete input was reconstructed byte for byte for every configuration (regime A: through the experimental
leaf + production `kg_int_inv`; regime B: every block of the frame, INT blocks through the experimental leaf and the
production block checksum), and every timed decode was compared.

## 1. Complete-input stored bytes

Change against unmodified Kolmo (regime A: production typed payload; regime B: production frame). "Same backend" =
against the existing layout coded by the same backend, which isolates the representation.

| config | A typed, real (n=17) | A: vs exist, same backend | B fast, real (n=17, 12 with INT) | B fast: vs exist, same backend | B hc, real (n=17, 8 with INT) |
|---|---|---|---|---|---|
| exist+huf | +19.94% | — | +6.43% | — | +7.02% |
| exist+rans | +151.90% | — | +8.97% | — | +7.95% |
| exist+zstd3 (hybrid) | +0.02% | — | +0.01% | — | +1.05% |
| gbp+huf | +36.94% | +14.18% | +10.37% | +3.70% | +9.63% |
| gbp+rans | +34.40% | −46.64% | +9.16% | +0.18% | +8.27% |
| gbp+zstd3 | +25.22% | +25.20% | +7.44% | +7.42% | +7.34% |
| lbp128+huf | +21.58% | +1.37% | +5.34% | −1.02% | +4.39% |
| lbp128+rans | +21.82% | −51.64% | +5.18% | −3.48% | +4.27% |
| lbp128+zstd3 | +16.63% | +16.61% | +4.29% | +4.28% | +3.66% |
| gpp+huf | +20.62% | +0.57% | +4.17% | −2.12% | +3.34% |
| gpp+rans | +21.32% | −51.84% | +4.20% | −4.37% | +3.37% |
| gpp+zstd3 | +13.34% | +13.32% | +2.34% | +2.32% | +1.78% |
| lpp128+huf | +17.06% | −2.40% | +3.95% | −2.32% | +3.28% |
| lpp128+rans | +17.64% | −53.30% | +4.04% | −4.53% | +3.36% |
| lpp128+zstd3 | +13.02% | +12.99% | +2.91% | +2.90% | +2.39% |
| sel+huf (charged) | +11.88% | −6.72% | +2.95% | −3.26% | +2.78% |
| sel+rans (charged) | +15.85% | −54.01% | +3.17% | −5.32% | +2.82% |
| sel+zstd3 (charged, hybrid) | **−2.13%** | −2.15% | **−0.20%** | — | +0.88% |

Absolute: A real production typed payload 3,054,649 B (zstd -3 whole input 3,560,281 B); B fast production frames
3,150,852 B; B hc 2,562,687 B. Stored backends and all synthetic-input tables: `results/tables.md`.
Synthetic inputs (reported separately, never in the real aggregates): patched packing is −1.1 to −2.3% in A and the
charged `sel+rans` −4.0% (A) / −2.7% (B fast) — the outlier stress files were built for exactly this case.

Notes on the table:
- **hc column**: the control is zstd at a fixed level 3 while production hc codes the residual leaf at zstd 19, so
  the `*+zstd3` rows are not level-matched there; the Huffman/rANS rows are.
- **exist+rans is pathological, not a packing virtue**: the existing rANS uses one static order-0 table per 1 MiB
  chunk, which spans the residual byte planes (near-constant high planes mixed with a noisy low plane); Huffman's
  128 KiB chunks roughly follow the planes. Packing removes the empty high planes, which is most of the "rANS gain"
  of the packed rows. Production is unaffected: it takes rANS only at hc and only when ≥ 3% smaller.
- Per input, `sel+zstd3`'s 6,410 B saving in B fast comes from five inputs (audio_space3 −4.36%, audio_roll −1.19%,
  fs_inode −4.74%, goog_volume −3.24%, goog_date −5.83%); the other seven are larger only by the experimental framing (≤ +0.06%).

### Forced numeric path vs complete planner

`fs_mtime_i64`: typed `gpp+zstd3` is −45.9% against the production typed payload (122,746 → 66,455 B), but the complete
planner does not choose the INT graph for this file and its frame is 51,727 B — the forced result would still be
28% larger than what Kolmo already does. Likewise four audio files that are INT graphs in fast mode are not in hc.
Typed gains are not whole-compressor gains.

### Earlier negative result preserved

Unconditional packing before zstd enlarged the streams again here: against `exist+zstd3`, `gbp/lbp128/gpp/lpp128 +zstd3`
are +25.2/+16.6/+13.3/+13.0% (A real) and +7.4/+4.3/+2.3/+2.9% (B fast real); 14–16 of the 17 typed real inputs end up larger than production.
This was not re-tuned.

## 2. Speed (native kernel: residual stream → blob → residual stream)

Regime A, real inputs, 10.75 MB of residual bytes; sum of per-input medians; MB/s of residual bytes; 1 thread.

| config | enc MB/s | dec MB/s | | config | enc MB/s | dec MB/s |
|---|---|---|---|---|---|---|
| exist+huf | 874 | 1075 | | gpp+huf | 341 | 579 |
| exist+rans | 261 | 814 | | gpp+rans | 229 | 366 |
| exist+zstd3 | 303 | 1151 | | lpp128+huf | 320 | 606 |
| gbp+huf | 449 | 531 | | lpp128+rans | 230 | 392 |
| lbp128+huf | 546 | 619 | | lpp128+zstd3 | 258 | 647 |
| lbp128+rans | 313 | 371 | | sel+huf | 91 | 867 |
| lbp128+zstd3 | 307 | 635 | | sel+zstd3 | 50 | 1042 |

With the same backend, packed representations decode 1.7–2.0× slower than the existing layout with Huffman (2.1–2.7× with
rANS) and 1.8–2.2× slower with zstd; they encode 1.6–2.7× slower with Huffman. The charged selectors encode 5–10× slower than a single trial.
CPU time tracks wall time within ~3% (single thread). IQRs and regime B kernels: `results/tables.md`.

## 3. End-to-end (complete planner, fast; 12 real inputs with INT blocks, 3.04 MB)

| pipeline | stored B | encode ms | decode ms (sequential, every block) |
|---|---|---|---|
| unmodified Kolmo fast | 898,742 | 120.5 | 8.32 (API, 1 thread: 8.52) |
| zstd -3 whole input (direct control) | 1,237,282 | 15.9 | 4.58 |
| sel+zstd3 | 892,332 (−0.71%) | 181.1 (+50%) | 9.26 (+11%) |
| gpp+zstd3 | 972,452 (+8.2%) | 126.9 | 11.65 |
| lpp128+huf | 1,023,295 (+13.9%) | 121.9 | 11.21 |
| sel+huf | 991,805 (+10.4%) | 149.9 | 10.47 |

Experimental encode = production compress − re-encode of the production residual leaf with its chosen codec + the
experimental residual encode (derived; production's own leaf trials are not subtracted, so it overstates the
experimental encode). Decode is the actual implemented reconstruction. The hc end-to-end table (in `results/tables.md`)
is not level-matched for zstd and is not used for decisions.

## 4. Peak memory (fast; RSS high-water above the RSS before the operation, KiB; real inputs with INT blocks)

| operation | median | max |
|---|---|---|
| production compress / decompress (1 thread) | 2,230 / 432 | 3,328 / 1,652 |
| experimental full decode: exist+huf / lpp128+huf / gpp+huf / sel+zstd3 | 470 / 996 / 1,010 / 722 | 1,796 / 2,084 / 2,120 / 1,908 |
| experimental residual encode: exist+huf / lpp128+huf / sel+zstd3 | 328 / 904 / 2,102 | 888 / 1,896 / 3,780 |

The packed decoders roughly double decode working memory because this harness materialises a 64-bit value array
(8 B per value) before re-laying the residual planes; a streaming per-segment decoder would avoid most of it.
Measurement boundary: whole-process RSS (heap, stack, touched pages) via `/proc/self/clear_refs` after `malloc_trim(0)`;
page-granular; buffers allocated before the reset are excluded.

## 5. Decisions

| combination | decision | basis |
|---|---|---|
| exist + Huffman-only, fixed | **Reject** as a replacement | +6.4% (B fast), +19.9% (A) vs production; fastest decode, but production already tries Huffman where it wins |
| exist + rANS, fixed | **Reject** | +9.0% / +151.9%; plane-mixing table (see notes) |
| gbp (any backend) | **Reject** | worst packed variant everywhere; one outlier sets the width for the whole block |
| lbp128 (any backend) | **Reject** | dominated by lpp128/gpp on size at similar speed |
| gpp + Huffman/rANS/zstd, fixed | **Defer** (workload-specific only) | wins only on a few outlier-heavy 64-bit inputs (fs_inode −4.7% with zstd; goog_volume −4.4% with Huffman, a 2.6 KB file; topobathy −0.03%); +2.3–4.2% in aggregate, ~2× slower decode |
| lpp128 + Huffman/rANS, fixed | **Reject** as fixed; keep only as a selector candidate | best entropy-only representation (−2.3/−4.5% vs exist with the same backend in B fast) but still +3.95/+4.04% vs production, ~1.8× slower decode, ~2× decode memory |
| lpp128 + zstd3, fixed | **Reject** | packing before zstd enlarges (+2.9% B fast, +13.0% A vs exist+zstd3) |
| sel + Huffman / sel + rANS (entropy-only selector) | **Reject** for general use; defer for outlier workloads | +2.95/+3.17% vs production (B fast); gains appear only on the synthetic outlier files |
| sel + zstd3 (charged, hybrid) | **Keep only for a size-oriented fast profile study** | −0.20% complete-input (B fast real), −0.71% on files with INT blocks, from five files; +50% encode, +11% decode, ~+0.3 MB decode memory; not level-matched at hc. Not a default candidate. |

## 6. Limitations

- Development data only, 20 inputs, small (≤ 3 MB each); five real files carry the only selector gains. Three inputs are
  machine-specific filesystem metadata; three are synthetic stress files reported separately.
- The audio family dominates the real bytes (9 of 17 inputs).
- No network access in this session: no independent external numeric corpora; the CI artifacts for the planner repair
  could not be inspected (see `BASELINE_EVIDENCE.md`).
- 2 vCPU shared VM: absolute speeds are indicative; ratios between configurations under the same shuffled runs are the
  usable signal. Packing kernels are straightforward scalar C (no SIMD), which favours the existing layout.
- Patched-packing width choice: global uses an estimated cost, local an exact cost; segmentation fixed at 128 (not tuned).
- zstd control fixed at level 3; hc comparisons of `*+zstd3` rows are not level-matched.
- The experimental leaf framing adds a tag byte and per-stream lengths; an integrated format could save a few bytes per
  block (≤ ~0.05% here).
- E2E encode for experimental rows is derived, not an implemented encoder.

## 7. Next substantive task (as first stated; done in §8)

Replace the `exist+rans` control with a plane-aware variant and re-run regimes A and B unchanged, to decide whether an
entropy-only leaf can match zstd on KG_INT residuals.

## 8. Follow-up: plane-aware and best-of-entropy controls

Run 2 of the same matrix (same corpus hashes, seed, 7 repetitions, harness `8d8fbb8`, production `2f3308a` unchanged)
with seven appended configurations: `planes+{store,huf,rans,zstd3}` (existing residual bytes, one stream per byte
plane) and `{exist,planes,lpp128}+ent` (existing Huffman-only and rANS both run per stream, smaller kept, both charged).
All 1,120 complete-input sizes of run 1 were reproduced exactly (and all 1,300 of an intermediate run). Records:
`results-followup/` (raw uncompressed SHA-256 in its `SHA256SUMS`).

**Mechanism.** The existing rANS (`rans_compress_all`) returns failure for a whole stream if any 1 MiB chunk holds a
single symbol, so the leaf falls back to stored. In regime A one 6.0 MB audio residual leaf was stored outright;
with planes, 17 constant or near-constant planes (1.78 MB) were stored in regime B. Huffman's chunk format codes a
repeated byte in a few bytes, so it never hits this. Plane mixing is a secondary effect: `planes+rans` is still
+14.29% (B fast), worse than `exist+rans`.

| config | A typed, real | B fast, real | B hc, real | B fast: vs exist, same backend |
|---|---|---|---|---|
| exist+huf | +19.94% | +6.43% | +7.02% | — |
| exist+ent | +15.71% | +4.91% | +5.17% | — |
| planes+huf | +17.25% | +5.44% | +6.64% | −0.93% |
| planes+rans | +64.58% | +14.29% | +7.75% | +4.88% |
| planes+ent | +4.58% | +1.19% | +2.96% | −3.55% (vs exist+ent) |
| lpp128+ent | +16.95% | +3.88% | +3.24% | −0.99% (vs exist+ent) |
| exist+zstd3 | +0.02% | +0.01% | +1.05% | — |
| **planes+zstd3** | **−0.58%** (13 of 17 smaller) | **−0.37%** (9 of 12 smaller) | +0.87% (level 3 vs production 19) | −0.39% |

**Can an entropy-only leaf match zstd here?** Not in aggregate. The best entropy-only leaf from existing code
(`planes+ent`) closes about three quarters of the fast-mode gap (+4.91% → +1.19%) and wins on 7 of 12 files
(audio, topobathy, goog_volume: −0.9% to −7.5%), but loses heavily where the residuals keep repeats that zstd's LZ
stage removes (fs_inode +37.6%, mri +25.7%, fs_size +11.4%). It also decodes 55% slower than production end to end
(12.08 vs 7.79 ms, rANS planes) and its charged double trial costs 4× the Huffman-only encode in the kernel.

**planes+zstd3.** Per input (fast): −0.31% to −3.81% on nine files; +0.04% (mri), +0.05% (fs_inode), +1.84% (goog_date,
326 B). End to end (fast, 12 real INT files): 887,026 vs 898,742 B (−1.30%), encode 112.7 vs 112.9 ms (derived), decode
8.07 vs 7.79 ms (+3.6%); kernel 326 / 1,175 MB/s vs `exist+zstd3` 308 / 1,142; decode peak memory median 568 vs 562 KiB.
At hc it is +0.87% against production, but the control is zstd -3 against production's zstd -19 leaf, so hc is untested.
Using it would need a production hook: KG_INT emits one residual stream, and no existing node splits a stream into
planes (CONCAT only joins), so plane leaves need a new KG node or INT flag — a format change, outside this branch's
authority and not covered by the source-identity gate. It is recorded as a hypothesis, not implemented.

**Decisions added**

| combination | decision |
|---|---|
| planes + zstd (one leaf per residual byte plane) | **Keep for integration research** — the only fixed, selector-free gain (−0.37% fast, −0.58% typed), with equal encode and +3.6% decode; needs a level-matched hc test and a format-change proposal reviewed separately |
| planes + entropy-only (`ent`) | **Keep only for a workload-specific study** (audio-like residuals without repeats); reject as a general leaf (+1.19% fast, +55% decode) |
| planes + rANS alone | **Reject** — single-symbol rejection stores constant planes |
| exist/lpp128 + `ent` | **Reject** — dominated by planes+ent / planes+zstd3 |
| existing rANS single-symbol behaviour | Not a production defect (production only takes rANS when it wins); a constraint for any rANS-only design |

**Next substantive task.** A level-matched test of per-plane leaves: add zstd -19 (and production's own leaf choice
rule) as per-plane backends, re-run regimes A and B, and include the larger source-disjoint real integer data the
parallel report calls for, to decide whether a per-plane KG_INT leaf layout earns a format-change proposal.

## 9. Follow-up 2: level-matched per-plane leaves with production's own leaf rule

Run 3 (same corpus, seed and repetitions; harness `6c64ebf`; production `2f3308a` unchanged; records in
`results-followup2/`). New configurations: `exist|planes + zstd19` and `exist|planes + prod3|prod19`. The `prod`
backends pass the streams to the **unchanged `kg_serialize`** as leaves, so every stream gets production's exact leaf
decision at zstd level 3 (fast) or 19 (hc). Calibration: `exist+prod3` reproduces the production fast frames and
`exist+prod19` the hc frames to within the 13-byte experimental framing (+0.02% in aggregate). All sizes from the
earlier runs (1,120 and 1,435 records) were reproduced exactly.

| complete planner, real inputs | files with INT blocks: production | per-plane, production rule, same level | all 17 real inputs |
|---|---|---|---|
| fast (zstd 3) | 898,742 B | 881,295 B (**−1.94%**, 9 of 12 smaller) | −0.55% |
| hc (zstd 19) | 551,671 B | 548,150 B (**−0.64%**, 6 of 8 smaller) | −0.14% |

Per file, fast: −0.31% to −6.66% on nine files; +0.06% on mri and fs_inode; +3.37% (+11 B) on the 326-byte goog_date.
hc: −0.12% to −4.44% on six; +0.06% (mri), +3.82% (+10 B, goog_date).

| cost (complete planner) | production | per-plane, production rule |
|---|---|---|
| fast decode, sequential (12 files) | 7.05 ms | 7.15 ms (+1.4%) |
| hc decode, sequential (8 files) | 4.70 ms | 4.75 ms (+1.1%) |
| residual leaf encode, kernel, fast / hc | 17.2 / 296 ms | 16.2 / 370 ms (−6% / +25%: more zstd -19 frames) |
| full decode peak memory, median (fast) | 438 KiB | 508 KiB |

**Typed regime caveat — a production heuristic misfire, not a layout cost.** In regime A `planes+prod3` is +4.48% in
aggregate although 13 of 17 inputs are smaller. All of it is one input: on `audio_concat` (6.0 MB residual, ow 4) the
per-plane layout is +166 KB. A diagnostic outside the harness shows plain zstd -3 per plane costs only +0.3% there
(1,689,273 vs 1,684,176 B). The loss comes from production's fast leaf rule for leaves ≥ 256 KiB, which picks
Huffman-only from three 32 KiB samples. For the nearly empty third plane those samples favour Huffman, which cannot go
below 1 bit per byte: 164,040 B where zstd needs 2,996 B. The same rule can misjudge any large, sparse leaf in
production today. This is recorded as an observation; no production change is made or proposed here. At zstd -19,
where the rule has no sampling shortcut, regime A per-plane is −0.27% versus the existing layout.

**Decisions added**

| item | decision |
|---|---|
| per-plane KG_INT residual leaves (production leaf rule) | **Keep for integration research.** Smaller in both modes level-matched (−1.94% fast, −0.64% hc on INT files), decode within ~1.5%, no selector. Requires (a) a separately reviewed KG format change (new node or INT flag; no existing node splits a stream), (b) a guard for the fast sampling misfire on sparse planes, (c) validation on larger source-disjoint data; development gains of 0.1–2% are not a general result |
| fast leaf rule's Huffman-only sampling on sparse ≥ 256 KiB leaves | **Observation for a separate production review** (not changed here) |
| fixed zstd -19 per stream in fast mode | Out of scope: −6% in fast mode is a level change (fast is level 3 by design), not a layout effect |

**Next substantive task.** Pre-register and run the per-plane-leaf validation on larger, source-disjoint real integer and
telemetry data (the data the parallel report also calls for): `exist+prod` against `planes+prod` at both levels, plus a
variant with a sparse-plane guard on the fast sampling rule, with complete-planner reconstruction, and no tuning on that
data. It needs network access or attached datasets, which this session did not have.

