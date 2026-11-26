# Startup-aware speed floors for small files: B qualifies on the sealed holdout2 (opt-in only; owner decides)

**Sealed `holdout2`, quiet host: the launch-constant table B passes all five preregistered gates.**
It met 34 of 66 end-to-end floor cases, against 30 for stock and 30 for the duplicate stock arm, which agreed with
stock on every case. B caused no new misses. On the 30 cases both arms met, it stored identical bytes. The `size` spec
produced the stock frame on every file. Charged encode time was 0.669× stock (geometric mean), and peak encode RSS
was at most 1.041× stock. The replication on the first holdout agrees: 42 versus 37 met, every gate passes.

**Caveat the gates did not cover:** on floors no setting can meet, B stores more bytes. It predicts more of them as
infeasible and falls back to `speed`. Over all 66 holdout2 cases B stored 8.1% more bytes than stock, and 24.9% more on
the 32 cases both arms missed. Under the protocol, a qualifying policy ships only as an opt-in calibration file, and
no code or default changes. Before any adoption, the owner should weigh that byte cost and the fact that the launch
constants are specific to this host ([what this does not settle](#what-this-does-not-settle)).

[Protocol, preregistered, with amendment 1](protocol.md) · [Harness](run.py) · [holdout2 cases](results-quiet/holdout2-cases.csv) ·
[Replication cases](results-quiet/holdout-cases.csv) · [Development cases](results-quiet/dev-cases.csv) ·
[Gates](results-quiet/score.json) · [Raw rounds and load](results-quiet/raw.jsonl.gz) · [Tables](results-quiet/) ·
[Opt-in table B](results-quiet/B-launch.tab) · [Reference replay](results-quiet/reference/reference-cases.csv)

## What was tested

Each policy is a `--calib` file read by the unchanged pinned encoder. That encoder is main `a1d39559`, SHA-256
`b67bbddb…`, the same binary as B26. No C code or default changed. The stock arm runs with no table, an empty HOME
and no `KOLMO_*` or `CZN_*` variables.

| Policy | Table |
|---|---|
| stock and stock-dup | Built-in table from an x86 VM, in-process setup only |
| B, launch | Built-in rates. Every setup term adds Pe = 1.765 ms for encode and Pd = 1.700 ms for decode. Each is the median end-to-end time of a 1-byte `-1` round trip over 21 rounds on this Mac. |
| C, host plus launch | `kolmo calibrate -s 0.1` on the 7 development files, then the same Pe and Pd added |

Floors are the six ordinary B26 floors: `encode>=1/20/60MB/s` and `decode>=1/30/80MB/s`. A `size` spec checks the
objective. Each case is judged by the arm's median **end-to-end** rate on the actual workload. Decode floors use the
stock 0.0.2 reader's fresh-process decode of the arm's own frame. Encode floors use the `--target` process itself,
**so trial compressions, table parsing and classification are charged**. The whole-file oracle forces all nine
settings on every round. Its answer is the smallest frame whose own median end-to-end rate meets the floor.

All runs used T1, fresh file-to-file processes and cells shuffled within each round. Development had 7 producers,
1 warmup and 5 scored rounds. Each holdout had 11 producers, 1 warmup and 11 scored rounds. No producer overlaps B26,
the #99 pilots, the small-dictionary set, the impact corpus, or the GnuPG/g10code and GMP/MPFR/MPC families
([dev](dev.json), [holdout](holdout.json), [holdout2](holdout2.json)).

## Run conditions

The run took place on 2026-11-26 from 04:28Z to 04:47Z, inside the coordinator's quiet window and one hold of the
timing lock. The harness was frozen at `c2ca75cb`, which includes the round-coverage fix from the #57 source review.

| Phase | Round trips | 1-min load before each round: median / max | After: median / max | Rounds redone |
|---|---:|---|---|---:|
| Development | 1,260 | 2.4 / 3.1 | 2.3 / 2.6 | 0 |
| holdout2 (decides) | 3,960 | 3.0 / 11.5 | 3.2 / 11.5 | 0 |
| holdout (replication) | 3,960 | 3.0 / 11.5 | 2.9 / 7.8 | 0 |

The launch constants and `kolmo calibrate` ran at load 3.5. All 9,180 round trips restored the input SHA-256. Every
(file, spec, arm) cell emitted one frame across rounds and holds exactly one valid observation for every scored round.

## Development: B picked

| Arm | Floor cases met, out of 42 |
|---|---:|
| stock | 24 |
| B, launch only | 26 |
| C, host plus launch | 13 |

On a quiet host, C's in-process calibration on seven files was much worse than stock. The first run's apparent C
advantage came from the loaded host. B was picked by the preregistered rule.

## Sealed holdout2 gates for B (decides)

| Gate | Result | Pass |
|---|---|:-:|
| G1 attainment | 34 met vs 30 for stock and 30 for the duplicate. Gain 4, no stock/duplicate disagreement | ✓ |
| G2 no new misses | None | ✓ |
| G3 objective | The 30 cases met by both used 884,321 B in both arms. The `size` spec produced the stock frame on all 11 files | ✓ |
| G4 selection cost | Geometric-mean paired encode time 0.669×. Max paired peak encode RSS 1.041×; the duplicate's max was 1.002× | ✓ |
| G5 integrity | 3,960 round trips, all exact, with frames identical across rounds and every round present | ✓ |

| holdout2 breakdown | stock | stock-dup | B |
|---|---:|---:|---:|
| Encode floors met, out of 33 (19 feasible by oracle) | 15 | 15 | 17 |
| Decode floors met, out of 33 (18 feasible by oracle) | 15 | 15 | 17 |
| Met with the oracle's smallest passing frame | 22/30 | 22/30 | 26/34 |
| Stored bytes over all 66 floor cases | 1,454,490 | n/a | 1,572,055 (+8.1%) |
| Stored bytes in the 32 cases both arms missed | 427,165 | n/a | 533,430 (+24.9%) |
| Paired decode time, geometric mean | 1 | n/a | 0.933× |

B's four gains:

| File | Floor | stock route, rate | B route, rate | Bytes stock → B |
|---|---|---|---|---|
| cairo.h | `decode>=30` | `ratio`, 23.5 MB/s | `hc`, 50.7 MB/s | 21,042 → 24,368 (the oracle's choice) |
| x11 Compose | `decode>=80` | `ratio-l1`, 65.3 MB/s | `hc`, 166.1 MB/s | 32,494 → 36,582 (the oracle's choice) |
| libicutu dylib | `encode>=60` | `balanced`, 33.1 MB/s | `balanced-n`, 66.3 MB/s | 78,169 → 82,055 (the oracle's choice) |
| libaec dylib | `encode>=20` | `balanced-n`, 14.1 MB/s | `balanced-n`, 20.3 MB/s | identical frame |

Three gains come from better route prediction once launch cost is counted. The libaec gain is selection cost: B
predicts slower settings as infeasible, skips their trial compressions, and emits the same frame faster. This also
explains most of the 0.669× encode time.

On the 29 floors no setting met on holdout2 (37 of 66 were feasible), the startup-aware model correctly predicts
infeasible. The selector then takes its documented fallback, the fastest predicted decoder `speed`. `speed` still
missed every one of those floors, so the extra bytes bought nothing.

## Replication on the first holdout (does not decide)

The first holdout has been seen before, so it was re-measured only as a check of the loaded run. B: 42 met versus 37
for stock and 37 for the duplicate (no disagreements). No new misses. On jointly met cases B stored 1.0015× stock's
bytes, under the 1.01 limit. Encode time 0.668×, max RSS 1.002×. It passes every gate, with the same byte cost on
misses: +7.2% over all 66 cases and +35.4% on the 24 jointly missed cases. Gains:

- `decode>=30` on libtextstyle and openssl NEWS (`hc`, the oracle's choice);
- `encode>=20` and `encode>=60` on the lcms2 dylib (`balanced-n`);
- `encode>=60` on the nettle dylib (same frame, faster charged encode).

## What this does not settle

1. **Byte cost on infeasible floors.** The preregistered G3 covered only jointly met cases. A user who asks for a floor
   no setting can reach gets a `speed` frame, about 25–35% larger than stock's on those cases, and still misses the
   floor. The fallback is native selector behaviour that calibration cannot control. An opt-in fallback that keeps the
   size or stock choice when nothing is predicted feasible would remove this cost. It is a separate native change
   and needs its own preregistration.
2. **Host specificity.** Pe and Pd are this Mac's launch cost: 1.77 ms on a quiet host, 4.5 ms on the loaded one. The
   built-in rates come from an x86 VM. Table B is a valid opt-in for this host. Another machine should re-measure its
   own launch constants with `run.py measure` (21 one-byte round trips) before using it.
3. **The B26 motivating misses were mostly a host effect.** See the [reference replay](#reference-replay-on-the-recorded-b26-inputs-quiet-host-explanatory-only).
   On this quiet Mac, stock already meets GMP `decode>=30` and `decode>=80`. B neither helps nor hurts on the three
   B26 inputs.
4. **One host, one night.** The evidence is one quiet window on one Mac. The duplicate arm matched stock on all 132
   holdout cases, so timing noise is small next to the gains, but no second machine was measured.

## Reference replay on the recorded B26 inputs (quiet host, explanatory only)

This replay compares stock and B on the GMP manual (gmp.info-1), gpgv and the GnuTLS handshake PNG, with 1 warmup
and 7 scored rounds. It ran on 2026-11-26 from 06:06Z to 06:19Z with the same frozen harness, as a separate job: the
first job's lookup of the replay table failed under macOS bash 3.2, so the job exited with rc=2 after the deciding
phases. The 1-minute load before rounds had median 8.6 and maximum 11.9. Rounds 3 and 7 each ended above 12 (at 14.0 and
14.3) and were redone once. Their 138 rows are kept in raw.jsonl with `invalid_load: true`. No round was aborted, and every
cell has exactly one valid observation per round. All 690 round trips were exact.
[Cases](results-quiet/reference/reference-cases.csv) · [Score](results-quiet/reference/reference-score.json) ·
[Raw](results-quiet/reference/raw.jsonl.gz) · [Log](results-quiet/reference/run.log)

| | stock | B |
|---|---:|---:|
| Floor cases met, out of 18 | 13 | 13 |
| Bytes on met cases | 1,688,258 | 1,688,258 |
| New misses | n/a | none |

- **GMP manual.** Both arms meet `decode>=30`: `ratio` at 31.0 MB/s for stock, 30.6 for B. Both meet `decode>=80`:
  `hc` at 112.9 and 109.2 MB/s. These two were B26 misses, but on this quiet Mac stock already meets them. Both arms miss
  `encode>=60`. Their `balanced-n` route reaches 41–43 MB/s once its trial compressions are charged; forced directly,
  `balanced-n` passes. The miss is selection cost, and launch constants do not remove it.
- **gpgv.** Same routes and frames as stock on every floor. The new `decode>=80` miss C caused in the loaded run is
  absent.
- **Handshake PNG.** No setting meets `encode>=20/60` or `decode>=30/80`. B falls back to `speed` and stores 7,084 B
  against stock's 6,901–7,063 B. This is the same infeasible-fallback byte cost as on holdout2, at 0.3–2.7% on this
  file.

## Opt-in use

```sh
kolmo --target 'decode>=30MB/s' --calib benchmark/learning-startup-floors/results-quiet/B-launch.tab -o out.kolmo -- in
```

The file is the built-in table with +1.765 ms and +1.700 ms added to every encode and decode setup term. Nothing reads
it unless `--calib` names it. Adoption, a default change, or a regenerated table for other hosts is the owner's call.

## First run on a loaded host (2026-11-25, timings invalid)

The first measurement ([results/](results/), [reference/](reference/)) ran while the 1-minute load was 80–310 on 10
cores. Other sessions' jobs ran outside the timing lock. Under the coordinator rule (load ≤ 12) its timings are
invalid. It picked C, which met 22 of 66 holdout floors against 16 for stock, then failed the memory gate (1.143× RSS).
The quiet run shows that C's advantage was a load artifact. Its lasting lessons were the infeasible-fallback byte cost,
which reappears above for B, and the need for a fresh sealed holdout. That run's replay on the B26 inputs is in
[reference/](reference/reference-cases.csv). It is explanatory only and also measured under load.

## Reproduce

```sh
# corpus: copy each manifest "source" to DIR/dev/<path>, DIR/holdout/<path>, DIR/holdout2/<path> (and DIR/reference/<path>); hashes are checked
TIMED TAG python3 benchmark/learning-startup-floors/run.py measure --encoder build/kolmo \
  --reader /opt/homebrew/Cellar/kolmo/0.0.2/bin/kolmo --corpus DIR --holdouts holdout2,holdout --max-load 12 --out FRESH
python3 benchmark/learning-startup-floors/run.py score --out FRESH
TIMED TAG python3 benchmark/learning-startup-floors/run.py reference --encoder build/kolmo \
  --reader /opt/homebrew/Cellar/kolmo/0.0.2/bin/kolmo --corpus DIR --table FRESH/B-launch.tab --max-load 12 --out FRESH-REF
```

`TIMED` is the host timing-lock wrapper. `run.py` verifies both executables' SHA-256 values and every input. It uses
seed 20261125 (the reference replay uses seed + 1). The holdout candidate is picked automatically by the preregistered
development rule. A round that ends above `--max-load` is redone; after 6 attempts the run aborts with no verdict.

Prior evidence used as reference: B26 [target floors](https://github.com/adnaneben/kolmo/blob/codex/ppp-audit-and-results-20261125/benchmark/waves/2026-11-25/overnight/target-floors/README.md)
(24 paired floor cases) and [selection regret](https://github.com/adnaneben/kolmo/blob/codex/ppp-audit-and-results-20261125/benchmark/waves/2026-11-25/overnight/selection-regret/README.md),
and the #99 learner (186 round trips, neither split qualified). This run follows the same rules: sealed producer
splits, paired rounds, charged wrapper or selection cost, exact stock-reader verification, and an explicit negative
where one applies.
