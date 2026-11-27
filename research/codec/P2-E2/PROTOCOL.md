# P2-E2 — type-first fast integer pipelines (development)

Status: **development** under P2 (`benchmark/accept/ACTIVE_POLICY.json`). All inputs were observed before; nothing
here is acceptance evidence. Follows P2-E1, which found that production `fast` never fits zstd-3's round-trip cost
on the development corpus because of its encoder analysis, and that reading the declared type first cuts that cost.

## Pipelines (`p2e2.c`, `fastint.inc`)

Typing from the input's own NumPy (little-endian 2/4/8-byte integers, C order) or 16-bit PCM WAV declaration, inside
the timed encode. Undeclared inputs are opaque and go to whole-input zstd at the mode level.

- `fi_choose` / `fi_forward` / `fi_inverse`: specialised re-implementations of production `kg_num_choose`,
  `kg_encode_num`'s INT program and `kg_int_inv`. **Compatibility contract:** the predictor/sign choice and cost,
  the parameter bytes, residual stream, rest stream, and the inverse output must be byte-identical to production on
  every typed input; `p2e2.c` checks all of them before timing (`p2e2_identity` records) and aborts otherwise. No KG
  wire semantics are changed: a production-format INT program can be produced from the same parameters and streams.
- Residual leaf: one E02C configuration per candidate (production leaf rule on the existing layout, Huffman on the
  existing layout, per-plane Huffman / best-of-entropy / production rule with guard / stored), using E02C's
  experimental leaf framing (`0xE0|rep`, `0xD0|rep`); rest leaf stored.
- Guards, all charged: post-encode 64 KiB zstd probe (`_g`); pre-encode probe comparing the predictor choice's own
  bit estimate with a 64 KiB (`_pg`) or 16 KiB (`_pg16`) zstd sample, before residuals are written; cheap leaf
  selection (`tf_sel*`): per-plane Huffman, then a zstd probe of the residual, re-encode with the production leaf
  rule when the probe predicts >= 5% smaller.
- Research container (real bytes, parsed by the decoder): flag, uvarint length, production block checksum of the
  original, then parameters, rest, residual blob (or a whole-input zstd frame for flags 0/2). Checksum verified on
  decode. Not a production frame (`measurement_scope = complete_pipeline_research_container`).

Controls: zstd-3 (designated speed rival), zstd-19 (informative), P2-E1's `typed_fast`/`typed_hc` (production INT
encoder via `kg_encode_num`/`kg_decode`), `*_pinv` (same pipeline with production `kg_int_inv`).

## Measurement contract

Identical to P2-E1 (in-memory boundary, one thread, `policy.CostModel()` defaults, 5 rotated/shuffled paired rounds,
samples >= ~20 ms, byte-exact checks around every timed sample, VmHWM per candidate in a separate process, 1 GiB
limit). Records go unchanged into `competitor-policy-v2/policy.py`. Archive comparisons stay blocked until a pinned
OpenZL rival is measured on the same boundary (the overnight N1 workstream is pinning it).

## Data

E02C development corpus (20 inputs). Guard sizes (64 KiB, 16 KiB) and the 5% selection margin are development
choices; they must be frozen before any validation and validated on fresh, source-disjoint inputs.
