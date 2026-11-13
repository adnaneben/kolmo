# P2-E30: audio levers on observed audio (development, descriptive)

These levers come from the P2-E29 diagnosis.

1. **Repetition-aware leaf choice** (`fx_rep_score`, `fastaudio.inc`).
   - How it works: it hashes 16-byte windows every 64 bytes (or more, so the table stays at most half full) of the
     first half. It then probes 32 windows of the second half at every alignment. The score is the share of probes
     found earlier.
   - Locally the score is 1.0 on exactly the MS-SNSD noise files where per-plane Huffman lost to zstd by 33–590%,
     and 0 on the other speech/noise files. On ESC-50 it flags 9 clips, where zstd-3 leaves on the residual are
     13–40% smaller than zstd-3.
   - Rule: if the score is ≥ 0.03, `ta_rep` uses the production zstd-3 leaves on the type-first residual and
     `ta_rep_fb` uses the whole-input fallback; otherwise both use the guarded per-plane Huffman frame.
2. **Fast forward pass** (`fx_pass_2_1_o_1p`).
   - Covers 16-bit signed order-1/2, the common audio case.
   - Proves gcd 1 on a prefix, then writes both byte planes in one branch-free pass.
   - Output is byte-identical to `fi_forward`: 278 files plus 60 random int16 arrays in `p2e30 verify`; the runner
     verifies every input again.
   - Forward pass 2.0–2.7× faster in process (771–900 vs 336–374 MB/s); full encode 336–408 MB/s.
   - `ta_phuf` = `tp_phuf_pg16` bytes, produced faster.

**Measurement.**

| Item | Detail |
|---|---|
| Inputs | ESC-50 (40, P2-E20) + MS-SNSD (40, P2-E29), re-fetched by their frozen scripts; all observed, so this is **development** |
| Harness | N1 unchanged; two runs; `confirm` |
| Pipelines | each frame with the production decoder and with the P2-E28 research fast-inverse decoder (`_fd`) |
| Rival | zstd-3 |
| Gate | ASan encoder, then in-process ASan decoder check (leak detection on) and production CLI decode |

If the results are strong, a preregistered test on a **new** audio source follows (P2-E31).
