# P2-E31 results: fresh audio (AudioMNIST + audiomentations) speed test — primary **PASS**, with an important caveat

Run: Actions **43438206296** at `43ef4cc`, 13:57–14:06 UTC (~9 runner-minutes).

Integrity:
- The frozen files' hashes on the runner equal the preregistration commit. The analysis reproduces from the committed
  records.
- Recovered from the job log: 16/16 printed files match `MANIFEST.json`.
- Gate 200/200, with leak detection on.
- `confirm`: IDENTITIES_MATCH, 400/400.
- Fast forward pass identical on 37/37 typed inputs.

Inputs: 30 AudioMNIST files (60 speaker keys) and 10 audiomentations fixtures (17 keys).

| Pipeline (fits zstd-3 speed cost on every pair and smaller, of 40) | Run 1 | Run 2 | AudioMNIST / fixtures (run 1) |
|---|---|---|---|
| **`ta_phuf_fd` (primary)** | **35** (LB 0.719) | **34** (LB 0.689) | 29/30, 6/10 |
| `tp_phuf_pg16` (original rule, production decoder) | 33 | 34 | 29/30, 4/10 |
| `tp_phuf_pg16_fd` | 34 | 35 | 28/30, 6/10 |
| `ta_rep_fd` | 30 | 35 | 27/30, 3/10 |
| production `kolmo fast` | 0 | 0 | 0/30, 0/10 |

Total ratio: zstd-3 is the reference. `ta_phuf` gives 2.389, `ta_rep` 2.408, and production `kolmo fast` 2.347.

**Decision: PASS.** The one-sided lower bound at alpha = 0.05/3 exceeds 0.5 in both runs.

## Caveat: this source is easy

The **original** rule with the production decoder also scores 33–34/40 here. The pass shows that the type-first audio
frame beats zstd-3 within its speed cost on this fresh source. It does **not** show that the new speed levers were
needed for that.

Across the three fresh audio speed tests the picture is source-dependent:

| Test | Source | Result |
|---|---|---|
| P2-E20 | ESC-50 | 25/40, fail |
| P2-E29 | MS-SNSD | 22–26/40, fail |
| P2-E31 | AudioMNIST + fixtures | 34–35/40, pass |

The development data (P2-E30) shows the levers' contribution on the harder sources: 60–63/80 against 50–51/80.

Not validated: the levers' gain on a *fresh* hard source. That would need a fourth fresh source with long or noisy
recordings.
