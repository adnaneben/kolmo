# P2-E10 results — frozen type-first policies on a broader development cohort (N1 complete-CLI boundary)

Run: Actions **43318087650**, 22:17–23:04 UTC (~48 runner-minutes), workflow at `9f6b972`. The registered E02E, E02F
and E02G sources were fetched by their own scripts (download hashes in `results/DOWNLOAD_*.sha256`) and prepared by
their unchanged `prepare.py`; `prepare_e10.py` built **61 inputs** (143.5 MB): 50 derived NPY arrays, 10 whole FSDD
WAVs and one whole Backblaze daily CSV. Evidence recovered from the job log (two large blocks needed GitHub's inserted
timestamps and a BOM stripped); **all 17 files match the run's `MANIFEST.json`**. Sanitizer gate 366/366. N1
`confirm`: **IDENTITIES_MATCH, 793/793 configurations complete, 0 failures** — including every OpenZL typed `le-*`
profile applied to a whole original file.

## Speed mode — rival zstd-3 CLI; fits on every pair AND smaller, of 61 (first run / confirmation)

| Pipeline | Fits and smaller | Fits |
|---|---|---|
| research `tf_planes_huf` (type-first per-plane Huffman) | **43 / 41** | 43 / 41 |
| research `tf_planes_huf_pg16` (guarded) | 39 / 44 | 42 / 50 |
| research `tf_planes_ent19` (per-plane best entropy) | 22 / 26 | 22 / 26 |
| research `mcsv_planes_huf` (only CSV is typed; else whole-input zstd) | 7 / 8 | 49 / 50 |
| production `kolmo` fast, fast -q, hc, max | **0 / 0** | 0 / 0 |

Per family (first run, `tf_planes_huf`): GHCN 8/9, MIT-BIH 8/8, EEG 4/4, pageviews 6/8, speech 2/4, dense-EEG
control 4/4, single-byte control 1/1, Backblaze 3/6, OWID 2/6, whole FSDD WAVs 5/10 (guarded 8/10), whole Backblaze
CSV 0/1 (nothing fits there). The confirmation run differs mainly on GHCN (5/9) and FSDD (6/10): near-boundary cost.

## Archive mode — rival OpenZL typed profile level 9 on the whole original file

Smallest pipeline inside OpenZL's modeled archive cost on every pair (first run / confirmation):
research `tf_planes_ent19` 23 / 25, production `kolmo -m max` 12 / 10, production `kolmo fast` 8 / 7, research
`tf_prod19` 2 / 3, production `fast -q` 2 / 2, **OpenZL itself 14 / 14** (GHCN 7/9, OWID 2/6, Backblaze 2/6, dense
EEG 2/4, speech 1/4). Byte-weighted, the per-input archive winners total 10.6% / 10.7% below OpenZL-9's bytes
(informative only; families are not pooled for any claim). Whole Backblaze CSV: production `fast` −16.1% vs OpenZL csv-9.

OpenZL caveat: the typed profile parses the NPY/WAV header bytes as a few extra values (a mild handicap). An exact
header-preserving adapter has been prepared by the N1 workstream (`research/codec/N1-exact-npy/`).

## Interpretation (development; observed sources; no acceptance)

1. On 61 inputs from 11 families, **production Kolmo never fits zstd-3's speed budget** on this boundary; the
   type-first per-plane Huffman pipeline is smaller and inside the budget on about two thirds of them.
2. Against OpenZL's typed numeric profiles in archive mode, some Kolmo pipeline (research or production) is smallest
   within OpenZL's cost on 47/61 inputs; OpenZL keeps GHCN weather and some count/telemetry inputs.
3. The research per-plane entropy pipeline is the most frequent archive winner; P2-E11 shows the Huffman variant can be
   written in today's frame format — the rANS-per-plane variant would need R1's singleton-capable encoder (P-18).
