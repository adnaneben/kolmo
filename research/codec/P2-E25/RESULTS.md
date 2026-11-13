# P2-E25: CSV archive levers on development data (all inputs now observed; development only)

**1. Per-leaf codec policy** (`gen_rt.py` → `rt.inc`, `rtcli.c`). These are generated copies of production
`kg_serialize`/`kg_encode_table` with a per-leaf codec hook. They use the existing KG syntax and leaf codecs, and the
production `kolmo -d` decodes them.

Local CLI timing on 70 development CSVs (FiveThirtyEight + P2-E22/E23 TidyTuesday) against locally built OpenZL
`csv`-9; indicative only (`local_cost2.json.xz`). Fits OpenZL's cost and smaller:

| Pipeline | Fits and smaller, of 70 | Bytes under the rule |
|---|---|---|
| `eco` | 45 | −7.1% |
| `tt_zstd12` | 44 | — |
| `tt_bsc0` | 37 | — |
| `rt_bsc_nr` (BSC leaves, no rANS trial) | 40 | **−12.2%** |
| `rt_mix32` | 43 | −10.6% |
| `rt_mix32_z19` | 38 | — |

Dropping the rANS trial makes BSC cheaper. No single fixed pipeline dominates.

**2. The pipelines complement each other.** Either `eco` or `tt_zstd12` fits and is smaller on 59/70; that is 61/70
with any of four pipelines. `eco` alone gets 45/70. Trial-encoding several pipelines exceeds the cost (P2-E13
lesson), so the choice must be near-free.

**3. A near-free picker** (`selector_csv.py`, runner records of P2-E21–E24, 105 CSVs, leave-one-experiment-out).

| Rule | Fits and smaller, of 105 |
|---|---|
| line regularity of the first 64 KiB > 0.9916 → zstd-12 table frame, else `eco` | **82** |
| `eco` alone | 75 |
| per-file oracle | 96 |

Interpretation: the table program helps when lines are regular, and quoted or ragged CSVs belong to `eco`.
`pickcli.c` implements the rule in C. Its outputs are byte-identical to the component encoders on 107/108 files; the
exception is the CR-only file, where it uses `eco` by design. ASan/UBSan/LSan: 8/8 locally.

**Next step: P2-E26**, a preregistered fresh test of the picker.
