# FEATURE_DECISIONS — RC3 review, 2026-11-20 (Apple M5, arm64, strict T1)

The baseline is RC3 at 5071f388; C0 is 2293c857 with the same code. Candidate C1 is 302e31a6 (local branch `review/rc3-integration-J62kQD`, made of fb58d3d6, a17d6657 and 5ce318ad).

Evidence:
- branch screening: `report-screening/SUMMARY.md` and the `screening-4.noindex` / `screening-5.noindex` suites;
- holdout: `report-holdout/holdout-main`, 21 unseen files;
- compatibility: `compat-matrix-c1`, `FORMAT_COMPATIBILITY_MATRIX.md`;
- correctness: `TEST_RESULTS.md`.

The full branch inventory is in `BRANCH_PR_INVENTORY.md`.

Decision values: ADOPT, KEEP_OPT_IN, RESEARCH_ONLY, DEFER. Everything not listed below is retained as history (see `BRANCH_PR_INVENTORY.md`). Nothing was deleted.

| item | sha | format effect | fresh Mac evidence (T1, byte-verified) | decision |
|---|---|---|---|---|
| RC3 presets speed/balanced/eco/hc/ratio | 5071f388 | — | Screening and holdout below; all deterministic, 0 misdecodes | **ADOPT (baseline)** |
| W3 decmap + phrase study (local merge) | a17d6657 | none: decoder only; frames identical (compat matrix) | Default eco/hc ties: 4/4 files screening, 21/21 holdout. The KOLMO_NATIVE rerun (br-decmap-native) ties on all 18. The claimed +14.8% decode is not resolvable on this Mac: decodes take 11–57 ms with ±10% CI, and the AVX2 path is x86-only. No regression. | **ADOPT** (in C1) |
| W4 bwt | 5ce318ad | none: frames byte-identical (compat and determinism) | Decode-peak RSS is 6–20% lower on BWT-routed files in screening and holdout (for example nci 179→151 MiB, SRR2584863 223→188 MiB). Encode: 1.087× gm in screening (3/7 wins); holdout 21/21 ties (gm 1.010×). The claimed +33% inverse BWT was not reproduced; it relies on Linux huge pages. | **ADOPT** (in C1) on the memory gain; the speed claim goes to errata |
| W4 cm (`-x` CM model 4) | ca81739e | **new -x generation**; old readers reject it safely; collides with W3 prior's "model 4" | ultra suite: vs rc3/x it is −0.28% (larger) with encode 1.99×, decode 1.81×, peak 225 vs 3,046 MiB. vs zpaq-m5 it is 7.6% smaller. | **KEEP_OPT_IN after renumbering**. Owner decision (new -x format id); not in C1 |
| W4 entropy (khuf v2) | 5f24d6ae | **reuses codec 27, changes 29**; default balanced output changes | +0.02% balanced, +0.11% native-all, ratio −0.01% | **DEFER**: needs a new codec id; gain too small to justify a format change |
| W4 lz (KLZ four-stream) | 3a027805 | **reuses codec 28** (opt-in) | balanced+native +0.17%, speed+native **−1.31%** (larger), speed+native decode 1.12× | **DEFER**: needs a new codec id; size regression on speed |
| W4 docs (async decode writer) | fba28558 | none | runs 2 threads at -T1 decode. Balanced +0.60% on docs; decode RSS worse 3/4 | **RESEARCH_ONLY** (separate scoreboard) until -T1 is single-threaded |
| W4 multi (machine-wide job pool) | fdb99760 | ABI: `kolmo_params` gains a field | not on the T1 board (multithread/governor research) | **RESEARCH_ONLY** |
| W3 prior (CM priors) | ae00b26f | new CM generation; collides with W4 cm "model 4" | not screened separately; superseded by W4 cm | **DEFER** (fold into the W4 cm renumbering decision) |
| KOLMO_NATIVE=all | (in rc3) | container v11 even when no native codec is selected (finding 6) | br-native: speed+native 8.3% smaller than lz4-1; balanced+native 9.0% smaller than zstd-3 at 0.80× encode | **KEEP_OPT_IN**; fix the needless v11 bump |
| KOLMO_NATIVE=all,strong | (in rc3) | as above | balanced: ratio 5.917 vs 4.724, at about 9.9 MiB/s encode | **KEEP_OPT_IN** |
| ultra `-x`, `-x -l 1`, `-x -l 2` | (in rc3) | — | x is 7.85% smaller than zpaq-m5 with 4.07× decode. **Peak about 3.0 GiB encode and 2.5 GiB decode.** x-l2 is +0.84% at 0.43× encode. | **KEEP_OPT_IN**; document the memory (errata E3) |
| GPU=1 | — | — | errors by design in rc3 (C7) | **RESEARCH_ONLY**; not measured |

## Why only two items are in C1

C1 includes only changes that are format-neutral, byte-identical and have no regression in fresh Mac evidence. Every other candidate either:
- changes or reuses a codec id (W4 entropy, W4 lz, W4 cm, W3 prior),
- breaks strict T1 (W4 docs), or
- changes the ABI (W4 multi).

Those need an owner decision before any merge (see INTEGRATION_PLAN).

---

# Addendum, 2026-11-21 (overnight) — decisions for C2 (25bedda8)

| item | commit | format effect | fresh evidence | decision |
|---|---|---|---|---|
| KOLMO_NATIVE v11 only when needed | 91ae8cee | frames that use no native id return to v3/v7 and are readable by rc2/1.1.0 again | 560 encodes, 0 misdecodes (`native-version-c2-2`) | **ADOPT** (in C2) |
| W4 docs encoder tuning, without the async writer | b902e04a (cherry-pick of 47f04156) | none; existing ids, default output changes | holdout: eco −4.39%, hc −1.04%, balanced −0.14%. Eco decodes 0.72× (text→BWT). | **ADOPT** (in C2); eco text rule is the owner's call (ALTERNATIVES A6) |
| W4 docs async decode writer | 1bc1b59a / fba28558 | none | 2 threads at -T1 | **RESEARCH_ONLY** (not in C2) |
| brotli mmap allocator | 69f391f8 | none; bytes identical (191 frames) | eco encode peak 1,079 → 233 MiB on the holdout; encode speed tie | **ADOPT** (in C2) |
| W4 cm (-x, CM model nibble 9; reserved-nibble rules in SPEC.md) | a555c05d | new -x generation; every older reader refuses it explicitly | compat 11,928 + 14,580 decodes, 0 misdecodes. Screening (W4 cm build): 2× speed, peak 225 vs 3,046 MiB, 0.28% larger. | **ADOPT for -x** (in C2). The −2.7% synthetic-table finding is listed in ALTERNATIVES. |
| KOLMO_NATIVE=all for speed | (opt-in) | v11 frames | holdout: 4.42% smaller than lz4-1 with the same decode; 4.2% smaller than default speed | **KEEP_OPT_IN**. Making it the speed default is an owner decision (ALTERNATIVES A7). |
| KOLMO_NATIVE=all,strong for hc | (opt-in) | v11 | holdout: 20.5% smaller than zstd-19, decode 0.33× | **KEEP_OPT_IN** |
| KOLMO_NATIVE=all for balanced | (opt-in) | v11 | 1.9% larger than default balanced | **do not promote** |
| W4 entropy, W4 lz | — | reuse codec ids 27/28 | unchanged from the review | **DEFER** (not attempted tonight; tiny gains) |
