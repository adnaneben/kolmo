# P2-E29 results: fresh audio (MS-SNSD) speed test with the fast-inverse decoder — **FAIL**

Run: Actions **43410736583** at `0aef2eb`, 10:39–10:52 UTC (~13 runner-minutes).

Integrity:
- The frozen files' hashes on the runner equal the preregistration commit. The analysis reproduces from the
  committed records.
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- Gate: 240/240, with leak detection on.
- `confirm`: IDENTITIES_MATCH, 520/520.

Inputs: 40 WAVs (108 candidate keys; 92 size-checked):

| Group | Files |
|---|---|
| clean speech (train) | 14 |
| "hitapps" quality clips | 10 |
| noise (train) | 8 |
| noise (test) | 7 |
| clean speech (test) | 1 |

| Pipeline (fits zstd-3's speed cost on every pair and smaller, of 40) | Run 1 | Run 2 |
|---|---|---|
| **`tp_phuf_pg16_fd` (primary)** | **22** (LB 0.385) | **26** (LB 0.483) |
| `tp_phuf_pg16` (production decoder) | 22 | 23 |
| `tp_phuf_fd` | 23 | 26 |
| `tb_lazy_fd` | 21 | 21 |
| production `kolmo fast` (either decoder) | 0 | 0 |

**Decision: FAIL.** The bar was lower bound > 0.5 at alpha = 0.025 in both runs.

## Diagnosis of the misses (run 1, `tp_phuf_pg16_fd`)

1. **Large noise recordings, 0.7–4.6 MB: bytes and throughput.**
   - Some (`ShuttingDoor`, `Munching`, `CopyMachine`, `WasherDryer`) are **33–590% larger** than zstd-3. They hold
     long silent or repeated stretches that zstd's matches capture and per-plane Huffman cannot. The 16 KiB
     pre-guard did not catch them.
   - Even where bytes are 13–22% smaller, the encoder runs at about half of zstd-3's throughput on multi-MB files,
     e.g. 32 vs 16 ms on 4.6 MB.
2. **Small speech clips, 100–185 KB: boundary noise.** Five misses had a *median* cost ratio of 0.87–0.94 but lost
   on one pair: fixed process cost dominates at this size, and "every pair" is strict.

Compared with P2-E28 (ESC-50, 35/40), the fast inverse helps far less here (22 → 22/26). These files' costs are
dominated by encoder throughput and guard failures, not by the decoder inverse.

## Consequences

- The fast-inverse decoder is still a clear decode-speed gain, but it is **not validated** as closing the audio speed
  gap in general. The production proposal's evidence section has been updated with this result.
- The next levers are development work on MS-SNSD, now observed:
  - (a) a repeat-aware guard that falls back to zstd when long runs or matches dominate;
  - (b) encoder throughput on multi-MB inputs.

  Any revised rule needs another fresh source.
