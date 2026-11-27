# P2-E11 — type-first pipelines written as genuine Kolmo frames (development)

The type-first encoder now writes **standard `CZN1` frames** (one block, version 7, plan `T_KG/0/width/header`,
codec `C_GRAPH`, production block checksum and header CRC) that the **unchanged production decoder** reads:
`kolmo_decompress` in the in-memory harness and the shipped `kolmo -d` CLI. No `src/` change, no new identifiers;
only the encoder is research code (`tpframe.inc`, `p2e11.c`, `tpcli.c`).

- `tp_prod3`: fast identity-checked passes + production `kg_serialize` (zstd-3 leaf rule). Its payload is checked
  **byte-identical to production `kg_encode_num`** on all 19 typed inputs (`results/identity.jsonl`).
- `tp_phuf`: per-plane Huffman inside one ordinary `C_HUF` leaf. The leaf format is a sequence of self-describing
  chunks of any length up to 128 KiB read until the output is full, so concatenating `huf_compress_all()` over each
  byte plane is a valid leaf (the R1/P-17 observation). Rest leaf stored.
- `tp_phuf_pg16`: with the 16 KiB pre-guard; fallback = a production frame from `kolmo_compress` (transforms off).

## Correctness

ASan/UBSan/LSan: 20 inputs × 3 variants decode through `kolmo_decompress`; all 60 frames written by `tpcli` decode
byte-exactly with the production `kolmo -d` CLI (`results/cli_roundtrip.txt`); predictor choice, forward program and
inverse identical to production on 19/19 typed inputs.

## Development timing (local 2-vCPU, in memory, 5 paired rounds; decoding by production `kolmo_decompress`)

| Pipeline | Fits zstd-3 cost | Fits and smaller | Notes |
|---|---|---|---|
| production `kolmo_fast` | 0 / 20 | 0 | +81% … +861% cost |
| `tp_prod3` (production format, production leaf rule) | 4 | 4 | bytes as production (identical payload) |
| **`tp_phuf`** (production format, per-plane Huffman) | **13** | **9** | same bytes as P2-E2's research container ± ~52 B frame |
| `tp_phuf_pg16` | 7 | 5 | guard bounds fs_mtime to +0.2% |

## What this changes

Decision P2-D1 (research containers) no longer blocks the main type-first result: the same stack is available in the
existing production format, decodable by today's decoder. What remains research is the encoder (declared-type
routing, the identity-checked fast passes, plane-aligned Huffman chunks). A production integration would be an
encoder-side change only, still subject to review. Next: the same pipelines on N1's CLI boundary, decoded by the
production CLI, and on the broader cohort.

## Addendum: production table graph, forced (`tp_table3`)

For comma-separated inputs the encoder calls production `kg_encode_table(',', '.')` directly (no planner analysis)
and writes the result as a production frame (plan `T_KG/','/'.'`). All 21 test frames decode byte-exactly with the
production `kolmo -d` CLI (`results/cli_roundtrip_table.txt`). Development timing on the 10 CSVs
(`results/summary-csv.txt`): bytes are often better than production `fast` (fs_table −60.6% vs −45.6%, syn_table
−44.0% vs −18.2%, mri −29.0% vs +1.1% against zstd-3) but cost is 10–213% above zstd-3 on every table: production's
table encoder and DECIMAL inverse are slower than P2-E6's typed path. **Archive-mode candidate, not a speed-mode
one.** P2-E6's typed CSV (research container) remains the speed-mode table result; expressing it in the existing
graph ops would need new semantics (DECIMAL's mantissa stream is not the INT node's fixed-width input).

## Addendum: entropy guard (`tp_phuf_eg`) — negative

After the passes, the sum of per-plane order-0 entropies of the residual (what per-plane Huffman approximates) is
compared with a 16 KiB zstd probe of the input. It does not catch the two inputs that pass the cheap guard and still
lose bytes (fs_size +6.4%, syn_sparse +5.7%: their plane entropy really is below the probe's whole-file zstd
estimate, so the small probe underestimates zstd there), and it costs more than the cheap pre-guard because it runs
after the passes (fs_mtime +206% vs +21%). Development run `results/summary-eg.txt`: feasible-and-smaller 4/20 vs
`tp_phuf` 8/20 and `tp_phuf_pg16` 6/20 in the same run. Guarding remains the weak spot: the probe, not the estimate,
is the limiting factor.
