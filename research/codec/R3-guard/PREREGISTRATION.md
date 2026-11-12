# R3: bounded compatible-leaf policy and fresh-source validation

Registered 12 November 2026 before downloading or evaluating the new source data. Write only to `fix/planner-record-ownership-isolated-20261112`; PR #7 remains draft/unmerged. Preserve other branches, concurrent work, production source/defaults/formats/TARGETS.md and all earlier frozen studies. R2 controls remain at `be41cc58185712ae660dbf3b7e7a93959625b8e9`; reviewed result reference `9a2fe5a9fbc71826fd26076b44927a36526dca5a`. This is research, not release acceptance.

## One new policy

The candidate makes exactly ONE native stored/Huffman/rANS competition on either the original or plane-aligned schedule; it does not run both schedules. It preserves the production predictor, GCD, residual order and reconstruction metadata. The residual-to-frame boundary is unchanged from R2.

Gate: choose the original schedule for one plane, fewer than 1024 residual values per plane, or fewer than 16384 residual bytes. Otherwise sample at most 1024 evenly spaced values per plane (include both ends), build byte histograms for each plane and their mixture, and compute the estimated conditional-entropy advantage `D = H(mixture) - mean(H(plane))` in bits/byte. Choose aligned HUF + aligned singleton-capable rANS only when estimated savings `residual_bytes * max(0,D) / 8` are at least BOTH `residual_bytes / 32` and `256 + 1024*(planes-1)` bytes. Ties pass the gate. The sample, histogram/log computations and all model construction are charged. The estimate is a heuristic, NOT a proven compressed-size lower bound. No filenames, family IDs or validation outcomes enter this rule.

When aligned, a single rANS temporary-word buffer is sized to `4*(min(plane_length,RANS_CHUNK)+64)` bytes and reused across plane encoder calls within the encode. Encoder normalization, four-state arithmetic and bytes remain identical to R2's singleton-capable implementation. HUF code and pinned readers/inverse remain unchanged. The original-schedule no-op path uses the original rANS rejection semantics, preserving baseline bytes. No reuse of entropy tables or arbitrary per-plane codec mixing is introduced. Deterministic ties remain stored, then HUF, then rANS.

Guarantees: exact input reconstruction and stored fallback no larger than the same RAW residual research frame; byte-identical baseline on the explicit no-op path. There is NO guarantee of non-expansion against an uncomputed compressed native baseline. Record every such regression. Keep R2's costed baseline-or-combined policy as the measured strict non-expansion control, not a fast candidate.

## Comparators and instrumentation

Policies: original native baseline; fixed aligned HUF/rANS + singleton support (R2 HRS); R2 costed baseline-or-HRS safety control; the new gated/shared-scratch candidate; and a duplicate baseline as an identical-output timing-noise control. The candidate must match baseline or fixed-HRS frame bytes according to its recorded gate, not just approximate their sizes.

Count whole-leaf candidate arms separately from actual HUF/rANS internal chunks, rejected calls and singleton chunks. Report directly tracked requested heap bytes/peak live allocations for the harness and staged leaf/inverse functions, with vendor allocations/allocator overhead/stack explicitly excluded. Scratch is per call, not an uncharged persistent cache. Show source-to-generated diffs and identities for counter insertion and scratch lifetime changes.

Encode: timed from frozen captured residuals through gate, model trials, allocation, selection, frame CRC/formation and cleanup; excludes unchanged source detection/prediction/capture and original checksum. Decode: preconstruct all frames, then independent warm and 32-MiB-displaced rounds; separate entropy+buffer kernel from complete CRC-verified KG_INT reconstruction. Kernel assumes prevalidated frames and is not a public decoder API. Deterministic input-size batching targets 1 MiB with a maximum of 128 calls for warm small messages; displaced measurements use one call. Displacement is not guaranteed LLC-cold or storage-cold. Seven rounds plus five confirmation rounds, balanced rotated/shuffled policy order, seeds 2026111203 and 2026111204. All raw timings and CPU times retained. Do not count repetitions as independent datasets.

## Source-exposure audit BEFORE download

A fail-closed preflight scans repository benchmark/bench/tune manifests, source-preparation scripts, documentation and all existing research text/registries (excluding this new R3 directory and its workflow) for the prospective source identifiers/URLs. Retain all scanned hashes and matches. Existing `PROVISIONAL.md`, `STACKING.md`, R1 and R2 records remain mandatory context. If an earlier measured or reserved source overlap is found, report validation blocked/incomplete; do not silently substitute sources. Automated text matching supplements, not guarantees, semantic source-independence. Neither prospective source was used in the visible conversation's earlier studies. No outcome may be used to select replacement data.

## Frozen new source families

Only publisher documentation was examined before registration; no numeric payloads were downloaded or evaluated.

F1: UCI Covertype, dataset 31, Jock Blackard (1998), DOI 10.24432/C50K5N, CC BY 4.0. Metadata: https://archive.ics.uci.edu/dataset/31/covertype . Exact download https://archive.ics.uci.edu/static/public/31/covertype.zip ; member whose basename is `covtype.data.gz`. Select the FIRST 262144 source rows, preserving order. Parse exactly 55 integer columns and retain zero-based fields 0 (elevation), 5 (horizontal road distance), 9 (horizontal fire-point distance), 6 (hillshade 9am, a low-range control), and 54 (cover class, a low-cardinality control). Five separate little-endian uint32 NPY v1 files. Do not sort, scale, delta-preprocess or inject missing values. Fail on invalid or out-of-range values.

F2: SNAP CollegeMsg temporal edge data, from Panzarasa, Opsahl and Carley (2009). Metadata/attribution: https://snap.stanford.edu/data/CollegeMsg.html . Exact download https://snap.stanford.edu/data/CollegeMsg.txt.gz . Require 59835 non-comment triples in original order, each SRC DST UNIXTS. All three fields become separate exact little-endian uint32 NPY v1 files. Do not sort, aggregate or enrich IDs. This is public anonymized edge metadata, not message contents. Retain publisher attribution; this registration does not invent a redistribution license.

Eight full real inputs total. No nested-prefix pooling. The compression boundary is exact DERIVED NPY bytes, not publisher CSV/whitespace/multiple fields. Record download, uncompressed-member and each input SHA-256 before first codec invocation; confirmation reuses these bytes. No downloaded corpus is committed to Git. Missing source, parse failure or mismatched expected row count means incomplete coverage, not a successful two-family result.

Constructed dense/random, constant, mixed constant/nonconstant chunk, single-plane and size-boundary cases are mechanism/correctness controls, NOT a third real family. Previously observed R2/E03 inputs may be replayed only as DEVELOPMENT regression controls and cannot supply missing fresh-family evidence.

## Frozen validation decision

First require exact reconstruction, old-leaf-decoder compatibility, source integrity, sanitizer/malformed-input checks, candidate-to-chosen-control byte identity and first/confirmation frame/capture identity. A family is applicability-exercised only if at least TWO of its full streams have more than one residual byte plane; report all eight inputs and no-op controls regardless.

Research profile: balanced native component, not full Kolmo or TARGETS.md acceptance. A family supports the candidate only if, in BOTH release passes: (a) whole-family stored bytes are at least 1% and 128 bytes below original native baseline; (b) no file >=64 KiB grows more than 1%, and no smaller file grows by more than 64 bytes; (c) sum-of-per-input-median encode duration <=1.15x baseline; (d) warm verified-input decode AND entropy-kernel duration <=1.10x baseline. The same family must meet all dimensions. Two exercised families meeting the screen = supported for further integration research; exactly one = provisional; neither = not supported; insufficient family applicability/infrastructure failure = inconclusive for the two-family claim. Dense and non-applicable controls must not be deleted.

Noise check: in each family/pass, the duplicate-baseline aggregate warm-verified decode and encode duration must be within 0.90..1.10x original baseline. Otherwise that family's performance decision is timing-inconclusive, not an automatic pass or proof of regression. No new thresholds, repetitions, fields or data selections after results. Report cold-displacement observations, absolute latency, family-balanced and pooled sizes separately. Observed ratios are descriptive on a shared runner, not confidence-qualified hardware guarantees.

## Outcomes and exclusions

Publish actual code, tests, provenance, raw records, captured-size compatibility checks and keep/reject decision. Do not claim the gate proves no expansion. Do not turn a research pass into a merge, format change or release. If it fails, retain the mechanism and unfavorable result; next decisions may consider a size-oriented profile only in a NEW experiment, without rewriting this verdict. Later packing alternatives, run/magnitude coding, predictor changes, general LZ, neural selection and production planner feedback are outside this task.
