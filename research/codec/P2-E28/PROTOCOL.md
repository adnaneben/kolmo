# P2-E28: does a faster decoder close the audio speed-mode gap? (development, ESC-50 clips observed in P2-E20)

## Why (P2-E20 follow-up, development analysis of the committed records)

On fresh audio, `tp_phuf_pg16` missed zstd-3's speed cost on 15/40 clips. Of those misses, **12 were cost and only
3 were bytes**. Several of the cost misses had a *median* cost ratio below 1 but lost on one pair: they sit at the
cost boundary.

Local profiling (gprof of 400 production decodes of one frame) put about **75% of decode time in production
`kg_int_inv`**, the KG_INT inverse. P2-E2's identity-checked `fi_inverse` computes the same output 3.0× faster on the
40 clips and 5.2× faster on the 19 numeric development inputs (`invbench.c`, local).

## Research decoder

`gen_fastdec.py` copies the pinned `src/` (core blob verified) into the runner's temporary directory and makes one
change: `kg_decode` calls `fi_inverse` instead of `kg_int_inv`. That is the **same wire format**, as a research
decoder only. Nothing in `src/` changes.

Local checks:
- 540/540 frames decode correctly: 40 clips + 20 corpus files + 120 random NPYs, × 3 pipelines.
- Production `fast` frames decode correctly too.
- In-process ASan/UBSan/LSan `fdcheck`: 21/21, with leak detection on.

## Measurement

| Item | Detail |
|---|---|
| Inputs | the 40 ESC-50 clips of P2-E20 (re-fetched by its frozen script, WAV subset) |
| Harness | N1 unchanged; two runs; `confirm` |
| Rival | zstd-3 |
| Pipelines | `tp_phuf_pg16`, `tp_phuf`, `tb_lazy`, `tb_best`, `tp_prod3`; each decoded by the production CLI **and** by `kolmo-fastinv` (`_fd`) |
| Production control | `kolmo fast` and `kolmo fast` + `_fd` |
| Gate | ASan encoder, then `fdcheck` (research decoder, in-process, leak detection on) and production CLI decode, for every input and pipeline |

This is **descriptive development data.** If the fast decoder turns the misses into fits, the next steps are:
- a preregistered fresh audio test;
- a request to authorize the production decoder change (`src/` and `tests/`).
