# All levels on the reviewed follow-up build

All 80 frame hashes matched historical main, pinned main and #104. The final measured executable was rebuilt independently from draft #105 at c8fc5ac54aa302baf6abc66f210617901f9e6e2e and had the same SHA-256. Across 80 presets, inputs and thread counts the median within-round change in combined speed was +0.43% against pinned main and +0.87% against its identical control. Each timed run decoded the complete file. The matrix had 960 timed round trips and 80 additional cross-reader checks. The separate confirmation had 144 timed round trips.

Four source/size combinations cover 16 KiB text, 64 KiB Python, 256 KiB Gaia and one complete 441,044-byte ESC-50 recording. The ordinary level path used L1–L9 and Ultra at T1/T4. The optional PCM environment switch was unset.

| Follow-up setting | Change vs main | Change vs repeat main | Same-binary control |
|---|---:|---:|---:|
| gaia256k Lultra T4 | -0.20% | -0.29% | +0.13% |
| python64k L1 T1 | -2.07% | -0.20% | -1.87% |
| python64k Lultra T1 | -2.35% | -1.54% | -0.80% |
| text16k L6 T4 | +7.16% | +0.40% | +1.84% |

The three initial adverse signals shrank to 2.35% or less on both controls over eleven paired repeats. The initially positive text setting also depended on the choice of main sample. Keep conclusions scoped to this shared host and panel. [All phase and byte scores](score.csv) · [Source, input and binary hashes](manifest.json) · [Complete observations](rounds.csv) · [Confirmation](confirmation-rounds.csv) · [Shared runner](../all-level-candidate/replay.py)
