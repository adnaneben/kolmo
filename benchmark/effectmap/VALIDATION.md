# Validation on two known cases

The tool was run on two PRs whose evidence is already published: #121 (the #46 warm-up fix) and #37 (KLZH2 lower
level at -6/-7). Its verdicts are compared with that evidence below.

## Setup

- **Builds.** All builds come from one Makefile setup, from detached worktrees in `$V/effectmap/src`. The
  `kolmo-impact` builds come from the same commits.

  | build | commit | sha256 (first 16) |
  |---|---|---|
  | main | a1d39559 | b67bbddb88d78d26 |
  | #121 | d1cf7feb | b443680ddc8afe70 |
  | #37 | ef3ac3ca | 74110accc6d15430 |

- **Corpus.** The impact corpus covers 18 kinds, dev and holdout:
  - 53 slices of 4 MiB (S2);
  - 73 cuts of 16 KiB (S1);
  - 188 prefix cuts of 1 and 3 KiB (S0).
- **Settings.** Threads 1 and 10. Levels 5-7 for #37 and 4-7 for #121, each with the level below, for the inversion
  check.
- **Mode.** Sizes only. The 1-minute load was 40-85 during the runs, so cost is instructions retired, with one
  A/B/A2 round.
- **Timed part.** Queued in `QUIET_WINDOW_QUEUE.txt` (`$V/effectmap/pr{121,37}/timing-job.sh`): dev S2 inputs, 5
  rounds, threads 1/4/10.
- **Rows.** `$V/effectmap/pr{37,121}/rows-sizes.jsonl`. The reports are copied into `validation/`.

`test_standard.py` also replays the numbers published in both PRs (and Codex's L8 KLZMA stacking result) through
the tier formulas. All 21 cases give the expected class.

## PR #121: warm-ups only where they can pay

| | PR evidence | effect map |
|---|---|---|
| frames | identical to main on 1,032 of 1,035 frames (3 outside the impact corpus) | identical on every frame (S0-S2, -4 to -7, T1/T10) |
| -4 | not touched | no effect beyond noise |
| encode, -5/-6/-7 | instructions +23.6/+24.2/+32.9% over the no-warm-up build on main, against +4.2/+3.5/+3.1% here. Wall clock against main: -12/-10/-15% (T1), -17/-17/-20% (T10). numeric -44 to -63%, weights -25 to -29% at -5 only | S2 per type: +5 to +68% faster at -5..-7 (numeric +28/+34/+40%; weights +47% at -5 and none at -6/-7; tsv and video the smallest) |
| decode | unchanged | unchanged (within noise) |
| verdict | a byte-identical speedup | **KEEP (byte-identical speedup)**, middle tier pass, 160 cells KEEP-ID and 208 NEUTRAL |
| gate | none (on for every input) | "none needed": the tree has one leaf, always on (holdout: 402 of 402 rows win) |
| stacking | not measured | notf: no change. stack-klzma: no change (460 rows). **tf-none: conflicts**: with transforms off, population-i64 (151 KB) is **+16.4% at -5 and +17.7% at -6/-7** (59,766 → 70,334 B at -6, reproduced by hand) |

The two outputs agree on what the PR measured. The stacking table adds one finding the PR's evidence does not
cover. population-i64 is 151 KB, under #121's new 256 KiB minimum for warm-ups. With its transform it is
byte-identical (38,411 B). Coded raw by KLZH2 (`KOLMO_IMPACT_TRANSFORMS=0`), it loses the warm-up's 2-bit literal
floor, worth 17.7% on a raw 64-bit integer block. The PR's sample found at most 0.37% on small blocks. So any default
path that sends small raw numeric blocks to KLZH2 without the numeric transform loses up to 18%: a planner miss, an
array inside a tar, or a block cut by the stream API. Under the standard this is a stacking conflict to route: keep
the warm-ups on small blocks whose literal-price floor binds strongly. It is reported to #121's owner; it does not
change the verdict on the default paths measured here.

The 3 frames the PR lists as changed (records.bin, two copies of owid_energy f64) are not in
the impact corpus, so the map cannot see them. `test_standard.py` scores them from the PR's own numbers:
- records.bin -5: +0.37% for -12% encode, a PASS within the reverse rate;
- owid -7: +0.06% for -49% encode, a PASS.

## PR #37: KLZH2 8-9 also encode the lower level, gated by size and type

| | PR evidence | effect map |
|---|---|---|
| bytes, impact corpus | -6: -0.010%, -7: -0.051% | S2: -6 **-0.010%**, -7 **-0.054%** |
| instructions, impact corpus | -6: +11.4%, -7: +8.4% (53 files) | S2: -6 +8.4%, -7 +6.7% (42 slices); S1 (16 KiB): +43% / +33%; S0: +2.4% / +12.5% |
| level inversions, impact corpus | -7 > -6 goes from 2 (canterbury, mr) to 0 | S2: the same two removed (canterbury +1.64%, mr +1.28% on main). On the S0/S1 cuts it removes 10 more, and leaves 1 that is also on main |
| per type | audio +47-50% for no gain; weights bf16 -0.25/-0.37% for +50-56%; genomics and video slightly cheaper | audio S1: +33% (-6) and +21% (-7) cost, bytes unchanged; text S1 +29/+45% for -0.006/-0.046%; img16 -0.43% and tar -0.20% at -7 (the inversion files). weights S2 slices are unchanged: the 4 MiB slices do not reach the shuffled-block path, as the PR says ("no impact-corpus file reaches it") |
| exchange rate | the PR does not claim the 1%-per-10% rate. It is kept to fix the order of levels (owner: "keep the gain, cut the cost") | middle tier mixed: 49 of 147 cells pass. The totals are a TRADE (`test_standard.py`: -0.047% for +4.9% needs 0.49%) |
| gate | the PR's: lower level on blocks under 2 MiB, shuffle and record blocks, and 16-bit images at -7 | **ROUTE.** On at -6 for inputs under 4 KiB, and at -7 on video; holdout accuracy 92%. "In any case keep it where it removes an inversion": tar S2 and img16 S2 at -7 (canterbury, mr), and the S0/S1 cuts |
| stacking | not measured | no amplifier. The gain is **absorbed** when KLZMA, BWT, stacked CM/KLZMA/BWT or the transform ablation recode the block (-0.020% alone, 0.000 to -0.013% combined) |
| memory | estimate unchanged | median peak footprint at -7: S0 5.8 → 7.9 MB, S1 20.6 → 25.3 MB (T1); S2 70 → 70 MB. One round under load, so it is listed "to confirm in the timed run" and not counted as a violation yet |

The verdicts agree in substance:
- **Same facts.** The bytes and the inversions it removes match the PR exactly.
- **Rate.** The change does not meet the middle-tier rate on average. The PR does not claim it either: it justifies
  the change by level order.
- **Gate.** The tool's gate says what the PR's own gate design says. The gain is in the inversion files (canterbury,
  mr) and in the smallest blocks. Large blocks of text, executables and audio pay for nothing.

The map adds two points the PR's evidence does not cover:
1. **4 KiB-1 MiB inputs.** On 16 KiB inputs (S1) the lower-level encode costs +33-45% instructions for 0.00-0.05%.
   Under the standard it pays only at -6 below 4 KiB, or where it removes an inversion (canterbury, mr, and 4 of the
   S1 cuts). The PR's "under 2 MiB" branch could narrow to "under 4 KiB, or inversion-prone types". That is a
   suggestion for the #37 owner; this PR changes nothing in #37.
2. **Memory.** At -7 the second encoder adds about 2-5 MB of peak footprint on small inputs (+23-36%), and up to
   +10 MB on some S2 files. The timed run (5 rounds) confirms or clears it. If it holds, the 1.10× rule applies
   unless the owner adopts the proposed 4 MiB floor (ACCEPTANCE.md, "Never allowed").

## What the validation does not cover

- **Wall time.** It waits for the quiet window (queued). The verdicts above use instructions, which match the PRs'
  own instruction tables.
- **x86.** Not run; Codex's hosted runs cover it.
- **S3 inputs** (over 64 MiB). Not in this run: pass `--large` (enwik8, silesia.tar) in the timed job when there is
  room.
