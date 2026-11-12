# Compression research review R1

**12 November 2026 — independent retrospective and revised execution priorities**

Repository: `adnaneben/kolmo`. Authorized continuation: `fix/planner-record-ownership-isolated-20261112`, draft PR #7. Review source snapshot: `a5518d016348acc521a9ce8dd739ffce4d3fe249`; production reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`. Concurrent E02F results were additionally reviewed at `a0c7eb75ef5ddcb2b58c94ef20a687f8bf69ea03` before publication.

## Executive judgment

The work has produced useful, reproducible evidence, but its search has become too concentrated on repacking already-predicted residuals and repeatedly validating that narrow path. Preserve the negative findings, the ownership repair and exactness discipline. Change the priority from **another packing selector** to **better use of the existing coding representation and decoder, followed by a stronger symbol model**.

The biggest missed opportunity identified and tested in this review is that **some plane-aware entropy modeling can use existing leaf chunk framing without introducing a new graph node or decoder format**. A second missed distinction is that rANS rejecting a single-symbol chunk is an encoder implementation choice, not a fundamental ANS limitation or necessarily an on-wire incompatibility.

The immediate goal should be a costed, old-decoder-compatible experiment with plane-aligned entropy chunks and explicit degenerate-stream handling. Do not merge, change production defaults, or call the exploratory size result a speed improvement. The alternative larger project—an independently deployable native codec—still needs a defined input contract, target workload and product-level benchmarks. Improvements to a numeric leaf do not by themselves constitute that codec.

## 1. Decision review

| Earlier choice | Revised decision | Reason and boundary |
|---|---|---|
| Begin with lossless numeric/structured data | Keep | A tractable specialist starting point, not proof of a general-purpose market or workload. Define typed input and arbitrary-file fallback separately. |
| Pin source, preserve original input bytes, count complete framing | Keep | Essential controls. Derived NPY reconstruction is valid only for the derived input; it does not recover source CSV spelling or excluded missing positions. |
| Freeze prediction/GCD while testing representations | Keep for ablation; reopen later | Correct causal isolation. The production predictor minimizes sampled summed residual bit lengths, not the actual selected coder's total size or latency. Freezing it forever would constrain a new native codec unnecessarily. |
| Prioritize global/local/patched packing | Downgrade | Useful cases exist, but this is a narrow part of the design space. Constant/run coding, integer-symbol models and side-stream separation were not comparably evaluated in the production-connected matrix. |
| Reject unconditional packing before HUF/rANS/zstd | Keep, with scope | Supported for the tested unconditional implementations/data. Do not generalize to all packing, terminal packing, optimized kernels, or other stream models. |
| Reject the E03 optional selector | Keep for its registered profile | Its ~1.716x encoding cost fails its declared screen. It remains a measured size/encoding trade-off, not a proof that selective packing is useless for archive-heavy applications. Do not revise the old verdict retroactively. |
| Use stored/HUF/rANS as a native baseline | Keep as a measured reference | E03 charges actual trials. It is neither the fastest possible native baseline nor an optimized final design; duplicated histograms, table construction and allocation remain opportunities. |
| Treat every per-plane alternative as requiring a new graph format | Revise | Separate graph leaves with different codecs may need graph evolution. Aligned HUF chunks, or aligned rANS chunks in one existing leaf, are narrower alternatives. The pinned decoder already reads variable chunk lengths. R1 tests this route. |
| Reject rANS-only planes because a constant chunk aborts coding | Reopen the implementation comparison | The current encoder returns failure, but the old decoder accepts a singleton frequency table. The original measurement remains true; its interpretation is implementation-specific. |
| Keep 128-value local blocks | Keep as a control, not an optimum | Segmentation controls attribution. A later limited, preregistered granularity experiment is justified; do not silently change it in past comparisons. |
| Defer SIMD until there is a signal | Revise narrowly | Do not optimize a losing selector. But compare scalar prototypes against a pinned mature packing kernel before judging the algorithm family's speed potential. Fusion and avoiding materialized uint64 buffers can matter as much as SIMD. |
| Exclude LZ from native experiments | Keep the dependency boundary; revise the inference | Match-free entropy coding and zstd solve different redundancy problems. First measure repetition/run structure and inexpensive native run/small-dictionary options; do not immediately build a new general-purpose LZ engine. |
| Defer float/JSON/neural work | Keep for the immediate task | They broaden correctness and deployment costs. Revisit only after numeric milestones and workload selection, not as a reaction to a negative packing result. |
| Treat E02D as independent validation | Qualify | Independent of the E02C fixture families, not of the repository's entire release evaluation programme: UCI 235 appears in the release holdout manifest. All C/D data is now observed development for new rules. |
| Treat E02E as a final rejection of plane modeling | Reject that interpretation | Its registered fast result is provisional; hc fails its registered cost bar. MIT-BIH has no relevant fast INT blocks and single-byte residuals, so it does not supply a second applicable family. |
| Preserve strict upstream release targets | Keep | Do not edit TARGETS.md. Separate research Pareto decisions and explicit application profiles from the existing release gate. |
| Repeat large correctness CI for every research-document change | Improve workflow discipline, not criteria | Keep evidence tied to exact source hashes. Avoid intentionally launching duplicate jobs; separately assess ordinary CI triggers and concurrency before any workflow edit. |

The historical failure classification and evidence are retained. The recent production repair is a correctness result, E02C/D are representation studies, E03 is a costed selection study, and E02E is a production-connected plane study. Their framing, selection, zstd levels, timing boundaries and eligible populations differ; their percentage improvements must not be pooled.

### Concurrent E02F evidence incorporated before publication

Claude Code completed E02F during this review. Its frozen per-plane graph layout was exercised on multi-byte residuals in all three families. The reported registered outcomes are provisional in one family for both modes: pageview data saves 9.57% fast / 7.48% hc; EEG and speech do not meet the size/cost criteria. Pooled savings over eligible files are only 0.15% fast / 0.37% hc. EEG hc decoding is 1.178x. This is not an eligibility failure like the earlier single-byte ECG case. It strengthens the rejection of a universal plane layout and points toward sparse/highly different plane distributions, not merely width > 1. The E02E and E02F rules and verdicts remain as registered; do not combine one passing family from each after the fact and declare either study "supported". R1 read the registered reports, not a new execution or complete independent audit of E02F's raw records.

The next compatible-chunk experiment must include these now-observed negative patterns as controls and must not reuse E02F as a fresh holdout. This does not undo R1's old-decoder feasibility result: existing chunk alignment is a narrower construction than E02F's independently coded graph leaves and may have different metadata costs.

## 2. Verified overlooked route: use existing entropy-chunk framing

### 2.1 Plane-aligned Huffman in one existing leaf

`huf_decompress_all` reads a sequence of chunks. Each contains a mode, raw length, and possibly a coded length. It enforces a maximum chunk length and total reconstructed length; it does not require every chunk to have the encoder's default maximum size.

Consequently, calling the existing HUF encoder separately on adjacent byte planes and concatenating the resulting HUF chunks can reconstruct the identical residual byte sequence through the **unmodified decoder**. This is not the same as introducing independently selectable HUF/rANS/zstd graph leaves. It does not provide arbitrary cross-codec switching within a leaf. It does remove the blanket need for a new graph opcode for this narrower experiment.

Plane boundaries already exist in the KG_INT residual representation when its byte-plane flag is set. No data permutation or bitpacking is needed. The encoder needs the boundaries; the leaf decoder needs only its existing chunk lengths. The outer graph/container still needs correct payload lengths/checksums if this is integrated later.

### 2.2 Singleton-capable rANS with the same decoder

The pinned `rans_norm` rejects `used < 2`. For a nonempty one-symbol chunk, its frequency calculation already yields the full `RANS_M = 65536`. The pinned decoder permits that table, and its four states can return to the required starting states without consuming residual renormalization words for that symbol.

R1 generates a separate research encoder with the sole normalization decision changed to accept exactly one used symbol. The original encoder and decoder remain present as controls; production source is never edited. This prevents a constant chunk from forcing the original rANS encoder to reject the entire stream. It does not establish that rANS is preferable for constant data: the existing HUF repeated-byte mode is usually a stronger small-frame control.

### 2.3 What was actually executed

A standalone compatibility probe stages complete, exact leaf-function bodies from the hash-verified native source distribution. It is **not a full production-CLI integration test**. Its two generated candidate encoders use the same raw leaf formats as the pinned decoder.

GCC release and Clang ASan/UBSan with leak detection both passed:

- 272 single-symbol cases, including every byte symbol and chunk-size boundaries.
- 15 mixed-chunk/nondegenerate byte-identity cases.
- 64 plane/chunk compatibility cases across one, two, four and eight planes and lengths on both sides of the HUF boundary.
- Replays of all 58 archived E03 baseline residual streams, with all emitted candidate payloads decoded by the unchanged leaf functions.
- Exact agreement between the two builds on residual hashes and all five measured payload sizes per stream.
- 11 Python audit-unit tests.

The original E03 archive SHA-256 and all 611 evidence-manifest files were checked. Every archived native baseline frame size was reproduced. Vendor objects are not sanitizer-instrumented in this test. The tested leaf and harness translation unit is instrumented. These are finite feasibility checks, not a security proof or old-version compatibility certification.

### 2.4 Exploratory size accounting

Actual encoded payload sizes plus the original, unchanged E2C1 metadata overhead give:

| Observed development group | Original native best-of-stored/HUF/rANS | Offline best of original and compatible aligned/singleton candidates | Diagnostic reduction |
|---|---:|---:|---:|
| E02C real-derived, 7 files | 51,436 | 50,786 | 1.264% |
| E02D full streams, 11 files | 337,195 | 319,539 | 5.236% |
| E02C synthetic, 18 files | 628,234 | 591,902 | 5.783% |

The E02D figure is close to E03's 319,859-byte optional-packing output, but **no speed superiority or deployable selection claim follows**. R1 times neither the new encoder nor an actual selector. It computes an offline diagnostic over tested candidates; multiple trial costs are absent. No new integrated frame writer is delivered. The result suggests where to spend the next experiment, not a production win.

Standalone aligned HUF is not a universal solution: on E02D it gives 411,824 bytes versus 426,202 for original-layout HUF, while the stronger native baseline is already 337,195. Aligned singleton-enabled rANS gives 319,748 bytes. One must compare against the adaptive baseline, not advertise improvement over the weaker fixed HUF or previously failing fixed rANS path.

## 3. Algorithm opportunities insufficiently tested so far

### Priority A: representation and probability-model boundaries

**Constant, zero-run and repeat coding.** Distinguish an entirely constant stream from long constant runs in a nonconstant stream. HUF already handles an entirely repeated chunk; it still has whole-bit limitations for highly skewed nonconstant chunks. Examine zero/nonzero maps, run lengths and nonzero-value streams. Include the map, run counts and every side-stream header. Compare bitmap versus gap-coded exception positions under fixed rules. Do not claim that small marginal entropy captures long-range repetition.

**Separate streams with different distributions.** The existing patched implementation interleaves local widths, counts, packed payload, exception positions and varint high parts. These are not one statistical population. An experiment should keep the residuals fixed and independently model control metadata, low bits and exceptions. Low bits that are near uniform should be packed directly rather than passed through an expensive entropy coder.

**Integer symbols plus exact offset bits.** A useful next native design decomposes a residual into a small magnitude/range token and exact remaining bits. Zero gets an explicit token; a bit-length token can imply its leading bit, leaving lower bits as an exact offset. More general learned bins are a later option. Pcodec's bin-plus-offset design is a relevant primary reference. This is a much more substantive alternative than treating an arbitrary packed byte stream as the sole entropy alphabet.

A diagnostic for plane modeling is

`N * H(byte) - sum_over_planes(N_plane * H(byte | plane))`.

It estimates empirical order-0 model separation headroom, not realizable compressed savings: model tables, chunk framing and coding precision still cost bytes. Add run/repetition diagnostics because this expression does not model sequential dependence. This diagnostic is in R1's audit, not in a deployed selector.

### Priority B: cheap numeric alternatives and prediction

**Local frame of reference and minimum-delta packing.** The initial lab included frame of reference, but the later production-connected packing matrix did not establish its general value. Parquet's delta/minimum-delta layout is a useful control. Use a defined modular signed/unsigned inverse and charge local bases. Do not conflate a new predictor with a packing-only win.

**Rice/Golomb with bounded escape.** Rice appeared in the initial lab, then dropped out of the controlled matrices. It is worth a narrowly targeted test on geometric-like residuals, with a bounded unary path and escape for outliers. FLAC's partitioned residual design is an established precedent. Rice is not an automatic choice for arbitrary values, and one scalar slow result does not determine every implementation's trade-off.

**Dictionary or patchless bounded-range coding.** Low-cardinality integer values can prefer a small dictionary plus IDs, or a range partition, over maximum-width packing. Include dictionary and lookup costs. Compare a controlled family, not an unrestricted search.

**Prediction tied to actual coding cost.** Keep the original predictor for the next compatibility ablation. Subsequently compare its current bit-length objective with approximate total costs for the intended native token/plane model, including table and seed overhead. Only then consider bounded higher-order, seasonal/lagged or cross-column prediction. Avoid searching arbitrary expressions without paying for discovery and parameters.

### Priority C: implementation and selection economics

**Mature packing controls and fusion.** Stream VByte separates control from data; mature bitpacking/PFor implementations provide an important speed reference. Compare pinned implementations with matching input types, headers and inverse semantics. Preserve licenses and review dependencies before reuse. A scalar loop that touches each bit or materializes a uint64 array is a correctness reference, not a ceiling on attainable throughput. Potential fusion: decode bits directly into the predictor/inverse output instead of materializing packed bytes, uint64 values and then residual bytes in succession.

**Reuse statistics and scratch.** The current rANS encoder allocates `(1 MiB + 64) * 4` bytes of temporary storage per call. Its decoder builds a 65,536-entry symbol lookup. Profile construction, normalization, histogram scans and allocations separately. Share trustworthy statistics between alternatives and reuse bounded scratch before replacing the coder. Changing the on-wire table precision would be a different format question; do not casually tune `RANS_SB` while retaining the old format identity.

**A cost-aware gate, not merely a width gate.** E03 demonstrated that raw represented-width reduction is a poor predictor of final entropy benefit. Sampling itself was not the main cost; discarded full trials were. Evaluate a bounded model that estimates metadata plus entropy/run benefit and skips unlikely improvements. Compare the gate with no-selector fixed policies and record false-positive/false-negative trial decisions. Do not implement a neural selector before a cheap gate has demonstrated a substantial remaining opportunity.

**A native redundancy ladder.** Native/match-free does not require ignoring repetition. Compare constant, runs, small dictionaries and integer-token modeling before deciding whether a dedicated LZ path is necessary. Keep hybrid zstd as a separate product/control branch. Reusing HUF from zstd is not a dependency-free claim, and a large linked vendor archive is not evidence of a minimal standalone decoder.

## 4. Methodological repairs required before stronger conclusions

1. **One evidence map, distinct populations.** Link E02C, E02C-harness, E02D, E03, E02E and E02F rather than merging incompatible numbers. Preserve the frozen files and raw data. Record tested SHAs, encoder boundaries, actual selected coders, framing, zstd levels, metadata and whether the normal planner ran.
2. **Decode-only timing.** E03's immediate-after-encode timing is confounded by cache/allocation history. Use separate decode-only rounds, order-balanced cases, warm and cold regimes reported separately, batch short inputs to exceed timer noise, and report absolute latency as well as ratios. Byte-identical frames with the same decoder are a useful noise/control pair. Keep verification outside a kernel measurement, but include it in a separately named verified-output end-to-end measure.
3. **No pseudoreplication.** Thousands of timing repetitions and nested prefixes are not independent datasets. Treat source family as the generalization unit; report per-family, byte-weighted and equal-family results, worst regressions, medians/spread and raw paired trials. Bootstrap or intervals must respect that grouping; four families cannot prove universality.
4. **Separate fixture exactness from source exactness.** A derived valid-value NPY input is legitimate, but omitted missing-value positions and CSV spelling are outside its contract. For actual table compression, missing maps and lexical restoration are mandatory stored costs.
5. **Fresh holdout registry.** Source-disjoint means checked against all repository manifests, not just the current agent's corpus. UCI 235 is already observed and appears in release holdout records. Register sources, versions, windows, hashes, exposure and intended use. Old exposed sources remain regression fixtures, not pristine confirmation.
6. **Eligibility before outcomes.** E02E taught that a family may be structurally unable to exercise a transform. Preregister residual-width/transform eligibility and include all selected source outcomes. Do not replace an inconvenient dataset after measuring its size result. A pilot used to select eligible sources is development, not final validation.
7. **Application budgets and Pareto reporting.** Preserve historic thresholds and TARGETS.md. For future research separately define one-time-encode/many-read, ingest-speed and tiny-message profiles. A 5% size saving at 72% extra encode cost can be useful in one application and unacceptable in another. This does not change E03's registered rejection.
8. **Independent parser/decoder coverage.** Current independent Python checks stop at the entropy boundary. Add differential and fuzz checks for model tables and malformed metadata, output/resource limits, fuzz corpus persistence and cross-architecture tests. Do not mistake checksums for authentication or finite sanitizer passes for a proof.

## 5. Revised roadmap and execution boundary

**Now completed in this review:** source/evidence reconciliation; primary-source survey; explicit decision revision; old-decoder-compatible HUF alignment and singleton-rANS feasibility tests; archived-residual replay; an action register and a concrete next task.

**Next experiment:** costed compatible aligned-chunk control. Hold prediction, input bytes, existing codec implementations, outer framing and input cohort fixed. Compare original pooled stored/HUF/rANS with fixed aligned HUF, fixed aligned rANS with singleton handling, and a bounded native choice. Count every trial, histogram, table, allocation, copy and header. Measure decode independently. First calibrate complete output and unchanged-decoder reconstruction before declaring a whole-file result.

**After that decision:** test separate control/exception streams and magnitude-token/exact-offset coding. Restore local-FOR/Rice as focused comparators. Only optimize kernels that remain on a useful Pareto frontier. Revisit prediction and block size in separate controlled experiments. Use a new source registry and fresh preregistered validation for any new selection rule. Do not reuse E02E as untouched once this review has read it.

**Still deferred:** broad LZ development, neural probability codecs, float/JSON expansion, production format additions, default promotion, merging and release. These are not prohibited forever; they require a focused question and the relevant correctness/performance evidence.

`ACTIONS.json` records owners as recommended workstream roles, not a claim that an external agent was launched. `NEXT_TASK.md` is the executable Claude Code/Codex handoff. No background monitoring, remote test completion or later source change is promised by this document.

## 6. Evidence and reproduction

Run in a disposable Linux work area with the pinned native tree and the exact archived E03 evidence:

```sh
python research/codec/review-20261112/build.py --source /path/to/native-root --output /new/path/review-build
python -m unittest discover -s research/codec/review-20261112 -p 'test_*.py' -v
python research/codec/review-20261112/audit.py --archive /path/to/E03_selection_evidence_43250751948.zip --probe /new/path/review-build/compat-release --output /new/path/review-audit
```

The source root contains `src/` and `vendor/`; it may be a verified source snapshot rather than a Git checkout. `EVIDENCE.json` records the source/archive identities and actual results. The full local raw receipts are supplied separately; their hashes are retained. The first local build-driver attempt had an object-list error, and full-core sanitizer compilation attempts were interrupted by timeouts. They produced no passing verdict. The final bounded leaf-function build and all checks above completed; those scopes are deliberately distinguished.

## 7. Primary sources and repository references

External sources were consulted on 12 November 2026; pin versions before benchmarking or copying implementations. Published benchmark numbers are not predictions for Kolmo.

- OpenZL Bitpack manual: https://openzl.org/api/c/codecs/bitpack/ — packing can be the final codec for uniform/nonrepetitive small integers, not necessarily preprocessing for another entropy codec.
- OpenZL concepts and numeric-array example: https://openzl.org/getting-started/concepts/ and https://openzl.org/getting-started/examples/c/numeric-array/ — typed graph decomposition and per-stream coding controls.
- Pcodec project and format: https://github.com/pcodec/pcodec and https://github.com/pcodec/pcodec/blob/main/docs/format.md — numerical latent variables, entropy-coded bins and exact offsets; pages/model overhead.
- Parquet encodings: https://parquet.apache.org/docs/file-format/data-pages/encodings/ — delta/minimum-delta packing, run/bitpacking and dictionary controls. The cited RLE/bitpacking encoding applies to specified Parquet types, not an assertion that arbitrary Parquet columns use it directly.
- Stream VByte paper: https://arxiv.org/abs/1709.08990 and implementation https://github.com/fast-pack/streamvbyte — separate control/data representation and mature byte-oriented decoding.
- Sprintz paper: https://arxiv.org/abs/1808.02515 — a precedent for jointly designed prediction, small blocks and time-series coding; no transfer of its benchmark claims.
- FLAC residual coding: https://www.rfc-editor.org/rfc/rfc9639.html and https://xiph.org/flac/documentation_format_overview.html — partitioned Rice and escapes for appropriate residual distributions.
- TurboPFor project: https://github.com/powturbo/TurboPFor-Integer-Compression — a specialist implementation to evaluate as a control, subject to provenance/license review.

Repository evidence at the review snapshot:
`research/codec/{E02C_RESULTS.md,E02D_RESULTS.md,E03_RESULTS.md}`;
`research/codec/E02C-harness/RESULTS.md` sections 8–9;
`research/codec/E02E/{PREREGISTRATION.md,RESULTS.md}` and `research/codec/E02F/RESULTS.md`;
`src/kolmo.c` (HUF chunk parsing, rANS normalization and decoding);
`src/kgraph.inc` (predictor objective and graph leaf framing);
`benchmark/holdout/inputs.json` and `benchmark/accept/TARGETS.md`.
