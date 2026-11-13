# P2-E17 results: archive-mode frontier in one run (61 inputs; every candidate decodable by the production CLI)

Run: Actions **43341163635** at `f4fcadc`, 00:57–01:30 UTC (~32 runner-minutes).

Evidence:
- The input hashes are **identical to P2-E14's**.
- Recovered from the job log: 17/17 printed files match `MANIFEST.json`.
- Sanitizer gate (ASan/UBSan/LSan `p2e13` encoder, then a production `kolmo -d` and `cmp`): **183/183**.
- N1 `confirm`: **IDENTITIES_MATCH, 488/488 configurations, 0 failures**.

Rival: OpenZL's typed `le-*` profile at level 9 on the whole original file. The table shows the smallest pipeline
inside that rival's archive cost on every pair (first run / confirmation):

| Winner | All candidates | Production `kolmo` only |
|---|---|---|
| production `kolmo -m max` | 19 / 17 | 20 / 18 |
| research frame `tp_pent` (per-plane rANS/Huffman) | 15 / 15 | — |
| research frame `tb_best` (Huffman or zstd-3 leaves) | 5 / 7 | — |
| production `kolmo fast` | 6 / 6 | 17 / 17 |
| production `kolmo hc` | 2 / 2 | 2 / 2 |
| **OpenZL itself** | **14 / 14** | **22 / 24** |
| byte-weighted frontier vs OpenZL-9 (informative) | −10.6% / −10.6% | −10.2% / −10.2% |

`tp_arch19` (zstd-19 leaves) never wins here: wherever it was smallest, `max` is smaller still within cost.

Per family (first run, all candidates):
- FSDD whole WAVs: `max` 10/10.
- pageviews: `max` 7/8.
- MIT-BIH: `tp_pent` 7/8.
- EEG: research frames 4/4.
- GHCN: OpenZL 7/9.
- speech: `fast` 3/4.
- Backblaze/OWID: mixed.

With production `kolmo` alone, OpenZL keeps MIT-BIH (5/8) and EEG goes to `fast`.

## Interpretation (development; observed sources; no acceptance)

- Against OpenZL's typed profile, Kolmo's existing modes already give the smallest archive pipeline within cost on
  37–39 of 61 inputs. The type-first production frames raise this to **47/61** by taking MIT-BIH, EEG and some
  GHCN/Backblaze inputs from OpenZL.
- The bytes gained are modest, because `max` already holds the large audio inputs.
- OpenZL keeps GHCN daily weather in every configuration so far. That is the clearest family where Kolmo needs a
  different model.
- Caveat: as in P2-E10 and P2-E14, OpenZL's `le-*` profile reads the NPY/WAV header bytes as values. The N1
  exact-NPY run (43334484927, owner) is the header-exact control.
