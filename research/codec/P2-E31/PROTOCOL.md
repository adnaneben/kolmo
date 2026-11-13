# P2-E31: preregistered fresh audio speed test of the combined speed levers (frozen before any file is read)

**Primary pipeline: `ta_phuf_fd`.** It produces the same bytes as the original guarded per-plane Huffman frame, with
two speed levers:
- the identity-checked fast forward pass (P2-E30, `p2e30 enc ta_phuf`);
- decoding by the research fast-inverse decoder (P2-E28 `gen_fastdec.py`; same wire format; `src/` unchanged).

On observed audio (P2-E30, development) it fit zstd-3's speed cost and was smaller on 60–63 of 80.

**Sources (paths-only selection, `prepare_e31.py`).** Neither is referenced in the repository before this commit.

| Source | Pinned commit | Selection |
|---|---|---|
| `soerenab/AudioMNIST` | `630d7da…` | one file per speaker directory, up to 30 |
| `iver56/audiomentations` | `f5e4504…` | one file per directory + stem, up to 10 |

Keys are ordered by hash; files of 20,000–5,000,000 bytes are kept. Limitation, stated in advance: AudioMNIST is
short spoken digits at 48 kHz, and the second source is test fixtures of mixed formats. Files that are not 16-bit
PCM fall back to the production zstd-3 frame and count as they come out.

**Policies.** Identical to P2-E30 (`run_e31.py`): `tp_phuf_pg16`, `ta_phuf`, `ta_rep` and `ta_rep_fb`, each with the
production decoder and with `_fd`; `kolmo fast`; and the zstd-3 rival. The N1 harness is unchanged; two runs;
`confirm`. The gate is as in P2-E30, plus `p2e30 verify` on every input.

**Decision (`analyze_e31.py`).**
- **Primary.** `ta_phuf_fd` fits zstd-3's complete-CLI speed cost on every pair and is smaller. **Pass** if the
  one-sided Clopper–Pearson lower bound at alpha = 0.05/3 exceeds 0.5 in both runs. This is the third sequential
  audio speed test, after P2-E20 H1 (fail) and P2-E29 (fail). Fewer than 20 inputs means inconclusive.
- Everything else is reported only.

A pass supports this claim: with the faster encoder pass (research) and the faster decoder inverse (a proposed
production change), type-first audio frames usually beat zstd-3 within its speed cost on fresh audio. Shipping it
still needs authorization of the `src/` and `tests/` change.
