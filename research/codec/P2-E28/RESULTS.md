# P2-E28 results: a faster KG_INT inverse in the decoder closes most of the audio speed gap (development)

Run: Actions **43409214117** at `f49047d`, 10:27–10:37 UTC (~10 runner-minutes). Inputs: the 40 ESC-50 clips of
P2-E20 (observed, development).

Integrity:
- Recovered from the job log: 15/15 printed files match `MANIFEST.json`.
- `confirm`: IDENTITIES_MATCH, **520/520**.
- Gate **240/240**: ASan encoder; the research decoder checked in-process with ASan/UBSan **and leak detection on**;
  the production CLI decode as well; `kolmo fast` frames through the research decoder.

Fits zstd-3's complete-CLI speed cost on every pair and is smaller, of 40 (run 1 / run 2). "Decode" is the median
decode wall time over the clips.

| Frames (same bytes) | Production decoder | Research decoder (`fi_inverse`) |
|---|---|---|
| `tp_phuf_pg16` | 24 / 24, decode 3.17 ms | **35 / 34**, decode **2.66 ms** |
| `tp_phuf` | 21 / 20 | 31 / 30 |
| `tb_lazy` | 24 / 23 | 29 / 29 |
| `tb_best` | 5 / 6 | 6 / 6 |
| `tp_prod3` | 3 / 3 | 5 / 6 |
| production `kolmo fast` | 0 / 0 | 0 / 0 |
| zstd-3 itself | decode 2.93 ms | — |

## Reading

The speed-mode misses on audio were decode-cost misses. With the identity-checked fast inverse, the type-first frame
decodes faster than zstd-3 does, and fits and is smaller on **85–88%** of the clips, up from 60%. Nothing about the
format changes: the same frames are read, and only the decoder's inverse loop is faster.

This is development data from observed clips. Next steps:
- a **preregistered fresh audio test** (P2-E29, new source);
- if it holds, a request to authorize replacing `kg_int_inv` in `src/kgraph.inc` with the fast inverse, plus
  conformance tests.
