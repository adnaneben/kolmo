# P2-E13: leaf selection on one shared type-first pass (negative, local, development)

Question (a stacking item): P2-E12 showed that `tp_prod3` (production zstd-3 leaves) wins on smooth/sorted columns
(Covertype, CollegeMsg timestamps) and `tp_phuf` (per-plane Huffman) wins on id-like columns. Can a pipeline get both
wins inside zstd-3's cost if typing and the forward pass are shared and only the leaf coding varies?

Setup:
- Encoder: `p2e13.c` includes P2-E11 unmodified, writes genuine `CZN1` frames, and decodes with unmodified
  `kolmo_decompress`. Every selection step is charged inside the timed encode.
- Measurement: 20 observed development inputs (`e02c-corpus`), in memory, 5 paired rounds, seed 20261113, one thread.
  This is a local container, not a runner, so treat it as noisy.
- Correctness: ASan/UBSan/LSan pass 60/60 on `tb_best`, `tb_best_fb` and `tb_lazy`, and 20/20 on `tb_pick`.
  Every frame decodes byte-exactly.

| Candidate | What it adds | Fits zstd-3 and smaller / fits (of 20) |
|---|---|---|
| `tp_phuf` (reference) | — | 6 / 7 |
| `tp_prod3` (reference) | — | 5 / 5 |
| `tp_phuf_pg16` (reference) | 16 KiB pre-guard | 4 / 4 |
| `tb_lazy` | builds the prod leaf only if Huffman saves < 30% | 5 / 5 |
| `tb_pick` | predicts the leaf from per-plane entropy and a 16 KiB residual zstd probe | **2 / 2** |
| `tb_best` | builds both leaves and keeps the smaller | **1 / 1** |
| `tb_best_fb` | `tb_best`, plus a whole-input zstd frame | **0 / 0** |

Full per-input table: `results/summary.txt`. A first run of the first three gave 3 / 3, 0 / 0 and 6 / 6, with the
same conclusion.

## Findings

1. **Selecting by measuring is cost-negative against zstd-3.** Building both leaves gets the best bytes (for example
   `fs_inode` −80.6%, `goog_date` −72.8%, `mri` −33.8%), but the extra leaf coding puts it 15–135% over zstd-3's
   in-memory round-trip cost on most inputs.
2. **The predictor `tb_pick` is both slower and often wrong.** Its probes cost about as much as coding. It picks
   Huffman where prod wins (`mri`, `syn_sparse`, `dem`) and prod where it does not pay (`goog_volume`).
3. **Leaf choice under a speed rival must come from cheap type/declaration facts or a learned encoder-only ranker
   whose inference is near-free**, not from trial coding. This supports AGENTS.md's ordering: compare deterministic
   bounded planning and supervised ranking before anything heavier, with the model's inference charged.
4. On the complete-CLI boundary, process start-up dilutes these overheads (P2-E12: `tp_prod3` fits on 8/10 there).
   So `tb_best` may fit at that boundary, but this was **not run**: the runner slot is reserved for the N1 exact-NPY
   comparator.

This is development data only, with no acceptance. No new wire semantics: the frames use existing `T_KG`,
`C_GRAPH`, `C_HUF` and `C_ZSTD` leaves.

## Addendum: production-format per-plane rANS (archive family), still local

`gen_rans1.py` generates `rans1.inc` from the pinned core, using the same single-line transform as the R1 review
(P-18): a single-symbol chunk gets frequency `RANS_M`. The unchanged production `rans_decompress_all` reads the
result.

New candidates:
- `tp_prans`: plane-aligned `C_RANS` residual leaf.
- `tp_pent`: the smaller of the rANS and Huffman leaves.
- `tp_arch19`: also tries production zstd-19 leaves and a whole-input fallback.

Correctness: every frame decodes with the production `kolmo -d`. That is 60/60 on the corpus and **360/360 on 120
random NPY arrays** (constant, two-symbol, walk, ramp and wide arrays), with ASan/UBSan/LSan on the encoder.

Bytes (`results/archive_sizes.txt`):
- `tp_pent` matches P2-E10's most frequent archive winner, the research container `tf_planes_ent19`, to within
  0.1% on all 20 inputs, now as a production frame.
- `tp_arch19` is smaller again where zstd-19's match model helps (`fs_mtime` 53,349 vs 147,554 bytes;
  `fs_inode` 33,213 vs 53,448 bytes).
- Production `kolmo -m max` is smaller on most audio, but at a much higher CPU cost.

Archive cost against OpenZL is measured in P2-E14 (amendment 1).
