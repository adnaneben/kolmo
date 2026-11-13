# P2-E30 results: audio levers on observed audio (development)

Run: Actions **43436027556** at `ddfcb1b`, 13:42–13:53 UTC (~11 runner-minutes). Inputs: 80 observed WAVs, 40 ESC-50
and 40 MS-SNSD.

Integrity:
- Recovered from the job log: 16/16 printed files match `MANIFEST.json`.
- `confirm`: IDENTITIES_MATCH, **800/800**.
- Gate **400/400**: ASan encoders, in-process ASan decoder check with leak detection on, and production decode.
- `p2e30 verify` on the runner: the fast forward pass was byte-identical on **80/80**, all of them on the fast path.

Fits zstd-3's speed cost on every pair and smaller (run 1 / run 2):

| Pipeline | ESC-50 (40) | MS-SNSD (40) | Total ratio (80) |
|---|---|---|---|
| `tp_phuf_pg16` (P2-E20/E29 rule), production decoder | 31 / 28 | 19 / 23 | 1.535 |
| `tp_phuf_pg16_fd` (fast decoder only) | 31 / 34 | 25 / 26 | 1.535 |
| `ta_phuf` (fast forward pass only; same bytes) | 28 / 29 | 24 / 23 | 1.535 |
| **`ta_phuf_fd` (fast forward pass + fast decoder)** | **32 / 35** | **28 / 28** | 1.535 |
| `ta_rep_fd` (repetition-aware leaves + fast decoder) | 27 / 26 | 25 / 26 | **1.674** |
| `ta_rep_fb_fd` (repetitive → whole-input fallback) | 31 / 30 | 25 / 22 | 1.612 |
| production `kolmo fast` | 0 / 0 | 0 / 0 | 1.549 |

## Reading

- **Speed levers add up.** The fast forward pass and the fast decoder together give the best speed-mode fit rate:
  60–63 of 80 (75–79%), against 50–51 for the original frame. On MS-SNSD alone it is 28/40, against 19–23 for the
  original frame.
- **The repetition-aware leaf is a bytes lever, not a speed lever.**
  - It raises the total ratio by 9% (1.535 → 1.674) by fixing the loop-recording noise files.
  - Those files were 33–590% *larger* than zstd-3 with Huffman. With zstd-3 leaves on the residual they are now
    13–40% smaller than zstd-3.
  - The cost of those leaves keeps them outside zstd-3's speed budget, so they belong to archive mode.
- **Candidate for the preregistered fresh test (P2-E31):** `ta_phuf_fd`. It is the same bytes as the original rule,
  with a faster identity-checked encoder pass and the faster decoder inverse.
