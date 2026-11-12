# E02C results — KG_INT residual representations (production-connected harness)

Method and framing: [`PROTOCOL.md`](PROTOCOL.md). These results come from `harness/e02c.c` only. The parallel
forced-typed implementation on this branch (`README.md`, `probe.c`, `run.py`; zstd level 1 control) is measured by its own
workflow and is not summarised here; its numbers are not interchangeable with these (different zstd level, framing,
corpus and selector policy).

**Outcome: negative for every fixed representation; marginal for a charged per-block selector that needs zstd.**
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
| Harness | committed in `cef71a9` (first committed as `41bcc96`, rebased unchanged onto the parallel work; the run used the byte-identical harness files, SHA-256 in `results/env.json`) |
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

## 7. Next substantive task

Replace the `exist+rans` control with a plane-aware variant (one static table per residual byte plane, using the existing
rANS kernel per plane) and re-run regimes A and B unchanged. It tests the one mechanism behind the large entropy-only
deltas above — empty and mixed byte planes — without changing representations, and decides whether an entropy-only
leaf can match zstd on KG_INT residuals before any selector integration work.
