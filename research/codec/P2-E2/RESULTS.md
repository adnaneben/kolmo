# P2-E2 results (development — not P2 evidence)

Local 2-vCPU Intel Xeon @ 2.10 GHz, gcc 13.3, one thread, 20 development inputs, 5 paired rounds, 23 pipelines.
Records, analysis and hashes in `results/`. Shared small machine: per-input feasibility near the boundary flips
between runs; counts below moved by ±1–2 across three repeated development runs.

## Correctness

- `p2e2_identity` on all 19 typed inputs: predictor/sign choice and its cost, parameter bytes, residual stream, rest
  stream and inverse output are **byte-identical to production** (`kg_num_choose`, `kg_encode_num`, `kg_int_inv`).
  Coverage: orders 0, 1, 2; signed and unsigned; element widths 2, 4, 8; output widths 1, 2, 4, 8; gcd 1 and 100.
- Every pipeline reconstructs every input byte for byte (before and after every timed sample); the container
  checksum is verified on every decode. ASan/UBSan/LSan: clean on all 20 inputs × 23 pipelines (`run`, 1 round).
- Malformed-input checks (`fuzz.c`, ASan/UBSan/LSan build, `results/fuzz.jsonl`): differential fuzzing of
  `fi_inverse` against production `kg_int_inv` on 3.3 million random parameter/stream cases (including invalid
  widths, flags, truncated/trailing parameters, wrong residual lengths and small resource limits) agreed on every
  accept/reject decision and every accepted output byte; 140,000 mutated research containers (7 input/leaf
  combinations: flips, truncation, header/length edits, splices, varint continuation bytes) were never accepted
  with a wrong output and raised no sanitizer report.

## Policy evaluator

Unchanged `policy.py`: speed `NO_FEASIBLE_CANDIDATE` 20/20 (research container scope, development data — correct),
archive `BLOCKED_MISSING_COMPETITOR` 20/20.

## Development feasibility vs zstd-3 (1 Gbit/s round-trip model, every one of 5 pairs)

| Pipeline | Fits | Fits and smaller | Worst bytes vs zstd-3 |
|---|---|---|---|
| production `fast` (P2-E1) | 0 | 0 | — |
| `typed_fast` (declared type + production INT encoder, P2-E1) | 2 | 2 | +130% |
| `tf_prod3` (fast identical passes/inverse + production leaf rule) | 5 | 5 | +130% |
| `tf_prod3_pinv` (same, production inverse) | 3 | 3 | +130% |
| `tf_huf` (Huffman, existing layout) | 13 | 7 | +346% |
| **`tf_planes_huf`** (per-plane Huffman) | **14** | **8** | **+340% (fs_mtime)** |
| `tf_planes_huf_pinv` (same, production inverse) | 8 | 7 | +340% |
| `tf_planes_huf_pg16` (+16 KiB pre-guard) | 8 | 6 | +6.4% |
| `tf_huf_pg16` | 8 | 7 | +8.2% |
| `tf_sel` (Huffman, probe-selected production rule) | 5 | 4 | +340% |

## Findings

1. **Stacking that works under P2 is a cost stack, not a byte stack.** Typing first, then identity-preserving fast
   passes/inverse, then cheap per-plane Huffman leaves: each layer removes cost, and together they move the
   development corpus from 0 (production) to 8 inputs that are both inside zstd-3's round-trip budget and smaller.
   Removing the fast inverse alone costs 6 feasible inputs (14 → 8).
2. **Per-plane layout + Huffman beats the zstd-coded leaf under this budget**, opposite to the size-only E02C
   ranking: the zstd leaf is smaller where residuals repeat (dem_jacksboro −41.8% vs −31.4%, syn_sparse −28.2% vs
   +5.7%) but its cost rarely fits.
3. **Guards are necessary and are the current bottleneck.** Unguarded, the forced INT graph loses up to +340% (fs_mtime,
   where the production planner rightly avoids it). Any zstd probe bounds the loss but costs as much as it saves on
   sub-0.3 MB inputs; a 16 KiB probe halves the 64 KiB probe's cost. A cheaper applicability signal is needed.
4. **Probe-based leaf selection does not pay** at these sizes (charged trials exceed the bytes they save).
5. Remaining overruns: 16-bit audio with 4-byte residual planes (format rule: zigzag residuals above 16 bits widen to
   4 bytes), mri, and the 22 KB topobathy (fixed per-call overheads).

## Next

- fs_mtime diagnosis: 87% of adjacent values repeat, so the order-1 residual is mostly zero, but it is 8 bytes wide
  and per-plane Huffman cannot spend under 1 bit per byte (>= 8 bits/value) while zstd needs 2.8. This is a missing
  representation (N3 zero-run/sparse residual layer), not only a guard problem.
- A zero-probe applicability signal (from `fi_choose`'s own statistics) to replace the zstd probe; evaluate on the
  E02E/E02F/E02G downloads (development now) on a runner, sequenced after the N1 workflow.
- Archive side once N1's pinned OpenZL rival exists on a matching boundary.

## Sensitivity to the link parameter (labelled sensitivity, NOT the registered rule)

Same records, `policy.run_cost` with only `link_bytes_per_second` changed; feasible on every pair AND smaller than
zstd-3, of 20 inputs. The registered P2 rule stays 1 Gbit/s.

| Link | `tf_planes_huf` | `tf_huf` | `tf_prod3` | `tf_planes_huf_pg16` | `typed_fast` |
|---|---|---|---|---|---|
| 100 Mbit/s | 11 | 9 | 12 | 10 | 10 |
| **1 Gbit/s (registered)** | **8** | **7** | **5** | **6** | **2** |
| 10 Gbit/s | 7 | 7 | 5 | 5 | 1 |
| CPU only (no transfer) | 7 | 7 | 5 | 4 | 1 |

On 7 inputs the per-plane Huffman pipeline is both faster end to end than zstd-3 and smaller, independent of the
link: the finding does not rest on the transfer term.
