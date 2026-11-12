# E02C protocol — production-connected residual representation trial (harness `harness/e02c.c`)

This directory holds two independent E02C implementations. This file describes the production-connected harness
(`harness/`, `Makefile`, `run_matrix.py`, `analyze.py`, results in `RESULTS.md`), which also covers the complete
production planner. `README.md` / `BASELINE.md` / `build.py` / `probe.c` / `run.py` describe the parallel
forced-typed-path implementation (commit `c880659`, zstd level 1 control, measured by `.github/workflows/e02c-research.yml`);
its results are produced by that workflow and are not reported here.

Question: does global/local bitpacking or patched packing of Kolmo's KG_INT residuals give a useful
total-size / encode-time / decode-time / memory trade-off against the existing residual byte layout, when coded with
Kolmo's existing Huffman-only and rANS leaf coders (stored and zstd as controls)?

This is an experiment. It does not change the production encoder, decoder, CLI, format, defaults or release
thresholds, and it is not a release or default-promotion proposal. Results: [`RESULTS.md`](RESULTS.md).

## Frozen reference

| | |
|---|---|
| Reference commit (production code under test) | `2f3308a3380a140ad0f6873ae228de7715e5eef5` |
| `src/kolmo.c` blob | `e06467c8872d494b97270811bedf6fd0bac2aa31` (repaired; repair not re-applied) |
| `src/kgraph.inc` blob | `91ad067dc583b9d9e6f060f214a0f7c402bb4a31` |
| Original leak-reproduction baseline (NOT used for compression) | `3c6890a91314b4d71db1911043ec19b87c16a86d` |

This harness adds files under `research/codec/` only; `src/` stays byte-identical to the reference
(`git diff 2f3308a -- src vendor Makefile tests benchmark` is empty). Planner-repair evidence: [`BASELINE_EVIDENCE.md`](BASELINE_EVIDENCE.md).

## Design

`harness/e02c.c` is one research translation unit that `#include`s the real `src/kolmo.c` (the pattern of
`tests/test_planner_ownership.c`). It calls the shipped routines directly — no predictor, detector or cost stubs:

| Production routine used | Role |
|---|---|
| `kg_encode_num` (`src/kgraph.inc`) | regime A: KG_INT predictor order, signedness, bias, GCD, seeds, output width and plane layout, unchanged |
| `kolmo_compress` / `make_plan` / block splitting | regime B: the complete production planner decides which blocks become INT graphs |
| `parse`, `decode_block`, `kg_decode`, `kg_int_inv`, `blk_sum` | production inverse, frame walking, block checksums |
| `huf_compress_all` / `huf_decompress_all` (C_HUF 13) | existing fixed Huffman-only coder (zstd's HUF from `vendor/zstd`, 128 KiB chunks) |
| `rans_compress_all` / `rans_decompress_all` (C_RANS 14) | existing fixed static order-0 rANS (Kolmo's rans64, 1 MiB chunks, 2^16 scale) |
| `codec_compress` C_ZSTD, level 3 | **hybrid control** (zstd 1.5.7 from `vendor/zstd`; LZ + entropy) |

Residual capture: the residual leaf of the production payload is decoded with the production leaf decoder, so the
captured bytes are exactly what `kg_int_inv` consumes. Headers, trailing bytes (`rest` leaf), program, parameters
and seeds stay as produced by production code.

"Entropy-only" is not dependency-free: the Huffman coder is zstd's HUF, so a Huffman-only path still links the
zstd library; rANS is Kolmo's own code. No new production hook was needed or added.

### Representations (all from the same residual values; segmentation frozen at 128 values)

| id | name | layout |
|---|---|---|
| `exist` | existing | production residual byte stream (`ow` bytes/value, byte planes when `ow > 1`) |
| `gbp` | global bitpacking | one width `b` = bit length of the maximum |
| `lbp128` | local bitpacking | width per 128 values (1 byte each) — matched control for `gbp` |
| `gpp` | global patched | one `b` chosen by an estimated cost; exceptions as position gaps (uv) + high parts (uv) — matched control for `lpp128` |
| `lpp128` | local patched (PFOR-style) | per 128 values: exact-cost `b`, exception count, positions (u8) + high parts (uv) |
| `planes` | plane-aware control (follow-up) | the existing residual bytes split into one stream per byte plane (`ow` streams when planar), so every backend builds a table per plane |

Each representation emits 1–5 streams; each stream is coded by the configured backend
(`store`, `huf`, `rans`, `zstd3`) and **falls back to stored when the coded form is not smaller** — every stream's
requested and actual codec is recorded (`kernel_size` records), so a stored fallback is never reported as entropy-coded.
Follow-up backend `ent` (configurations `exist+ent`, `planes+ent`, `lpp128+ent`): per stream both existing entropy coders
(Huffman-only and rANS) are run and the smaller kept, both charged — the best entropy-only leaf available from existing code.
Configuration indices 0–23 are unchanged from the first run; 24–30 are appended. `sel+<backend>` still chooses among
the first five representations only.
`sel+<backend>` is an implemented size selector: it encodes all five representations with the backend and keeps the
smallest; all five trials are inside its measured encode time (an exhaustive oracle, charged, not free).

### Experimental framing

Defined in the header comment of `harness/e02c.c`: tag `0xE0|rep`, representation header, then
`codec u8 | raw uv | coded uv | bytes` per stream. It is never written into a CZN1/CZNS frame and uses no KG op
or leaf codec id. Size accounting replaces only the production residual leaf: complete-input size =
production frame (or typed payload) − production residual leaf (id, codec, lengths, bytes) + replacement leaf
(id, codec slot, raw length, blob length, blob). The `exist+*` rows go through the same framing, so representation
comparisons are like for like (in aggregate, `exist+zstd3` is +0.01% against the production fast frames, which mostly code the residual leaf with zstd -3 — that is the framing overhead; per input it differs more where production chose Huffman or a smaller leaf).
All other bytes — container header, index, block checksums, plan bytes, graph program, KG_INT parameters
(seeds, bias, GCD), rest leaf with original header/tail bytes — are the production bytes and are counted.

### Regimes

- **A, explicitly typed**: `kg_encode_num(C_ZSTD, level 3)` on the whole file with the manifest's width and header
  length. A forced numeric-path result; not a whole-compressor result.
- **B, complete planner**: `kolmo_compress` (fast, and hc) with one thread; only blocks the planner turned into INT
  graphs are affected. Inputs without INT blocks keep the production frame unchanged.

## Correctness (this experiment's own checks)

`make -f research/codec/E02C/Makefile check` (from the repository root):

1. ASan + UBSan (`-fno-sanitize-recover=undefined`) + LSan selftest, golden vectors compared;
2. the optimised build against the same golden vectors (cross-build determinism);
3. `harness/reference.py`: independent pure-Python decoder of the stored-backend blobs for all five
   representations, and an independent Python KG_INT inverse reconstructing typed inputs from the captured
   production parameters, residual and rest streams.

Selftest coverage: output widths 1/2/4/8, plane/non-plane layout, lengths 0, 1, 2, 127–129, 255–257, 1000, 4097;
zero, constant extreme, full-width random, sparse and clustered outliers, alternating extremes, tail outlier; every
truncation prefix and random bit flips (must never crash); hand-built malformed blobs (bad tag/rep/codec, length
mismatch, overlong varints, invalid Huffman/rANS/zstd payloads, widths above the output width, nonzero padding,
exception count/position/zero-high/overflow errors, repeated positions, trailing bytes) and resource-limit rejections
before allocation; 240 typed production cases (widths 2/4/8; constant, signed/unsigned extremes, wrapping deltas,
GCD, outliers, wrapping ramp; heads and tails) through `kg_encode_num` at levels 3 and 19 with complete-input
reconstruction for every configuration; complete-planner frames (including the earlier planner-ownership `.npy`
int32 fixture) reconstructed block by block with production checksums. Measurement runs additionally reconstruct
every complete input for every configuration, and check every timed decode.

## Reproduce

```
make ARCH= build/libvendor.a
make -f research/codec/E02C/Makefile all check
python3 research/codec/E02C/corpus/build_corpus.py <corpus-dir> research/codec/E02C/corpus/manifest.json
python3 -I research/codec/E02C/run_matrix.py <corpus-dir> <out-dir> --reps 7 --seed 20261112
python3 -I research/codec/E02C/analyze.py <out-dir>/raw.jsonl <out-dir>
```

`run_matrix.py` refuses to run if any corpus file's SHA-256 differs from the manifest. The filesystem-metadata inputs are
machine-specific (see the manifest); other machines will produce different bytes for those three files.
