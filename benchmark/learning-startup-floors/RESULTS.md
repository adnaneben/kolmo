# Startup-aware speed floors for small files: rejected (loaded host; quiet replication pending)

> **Timing status: pending: quiet window.** The measurements below ran under a load average of 80–310 on 10
> cores, so the coordinator rule (load ≤ 12) makes their timings invalid. A load-gated replication is queued:
> development is re-measured, `holdout2` is a new sealed holdout that alone decides, the first holdout is
> replicated, and the B26 reference replay follows. See [protocol amendment 1](protocol.md). The memory gate (RSS)
> and byte results below do not depend on load. Floor verdicts and time ratios do.

**Decision: no policy qualifies, so nothing ships.** The development phase picked the startup-aware host calibration
(C). On the sealed holdout it met 22 of 66 end-to-end floor cases; stock met 16 and its duplicate 17. It caused no
new misses and stored identical bytes where both arms met the floor. It failed the preregistered memory gate:
peak encode RSS reached 1.143× stock on one case (limit 1.10). The test also turned up two problems the gates did
not cover. When C missed a floor, it still stored more bytes, 17.8% more over all 66 cases. A calibration table fitted
on seven development files mispredicted another file. The whole run ran on a heavily oversubscribed host, so the
absolute rates are not reliable floor verdicts for a quiet machine. Keep stock `--target` and the built-in table.

[Protocol (committed before measurement)](protocol.md) · [Harness](run.py) · [Holdout cases](results/holdout-cases.csv) ·
[Development cases](results/dev-cases.csv) · [Direct oracle rates](results/holdout-direct.csv) · [Gates](results/score.json) ·
[Raw rounds](results/raw.jsonl.gz) · [Tables](results/) · [Host load](results/load.log)

## What was tested

Each policy is a `--calib` file read by the unchanged pinned encoder. That encoder is main `a1d39559`, SHA-256
`b67bbddb…`, the same binary as B26. No C code or default changed. The stock arm runs with no table, an empty HOME
and no `KOLMO_*` or `CZN_*` variables.

| Policy | Table |
|---|---|
| stock and stock-dup | Built-in table from an x86 VM, in-process setup only |
| B, launch | Built-in rates. Every setup term adds Pe = 4.477 ms for encode and Pd = 4.504 ms for decode. Each is the median end-to-end time of a 1-byte `-1` round trip over 21 rounds. |
| C, host plus launch | `kolmo calibrate -s 0.1` on the 7 development files, then the same Pe and Pd added |

Floors are the six ordinary B26 floors: `encode>=1/20/60MB/s` and `decode>=1/30/80MB/s`. A `size` spec checks the
objective. Each case is judged by the arm's median **end-to-end** rate on the actual workload. Decode floors use the
stock 0.0.2 reader's fresh-process decode of the arm's own frame. Encode floors use the `--target` process itself,
**so trial compressions, table parsing and classification are charged**. The whole-file oracle forces all nine
settings on every round. Its answer is the smallest frame whose own median end-to-end rate meets the floor.

All runs used T1, fresh file-to-file processes and cells shuffled within each round. Development had 7 producers,
1 warmup and 5 scored rounds. The holdout had 11 producers, 1 warmup and 11 scored rounds. In total, 5,220 encode
and stock-decode round trips restored the input SHA-256, and every (file, spec, arm) cell emitted one frame across
rounds. No producer overlaps B26, the #99 pilots, the small-dictionary set, the impact corpus, or the GnuPG/g10code
and GMP/MPFR/MPC families ([manifests](holdout.json)).

## Development: C picked

| Arm | Floor cases met, out of 42 |
|---|---:|
| stock | 8 |
| B, launch only | 10 |
| C, host plus launch | 19 |

B moved `decode>=30` on the text files to `hc` and met them. It also became conservative elsewhere and broke two
`encode>=20` cases that stock had met. Most of C's extra gain came from `encode>=1MB/s`. Its host table rates `eco`
and `hc` as slow on text, which drops those trial compressions. The `--target` process then finishes faster and emits the
same frame through `ratio-l1`/`ratio`. That gain comes from **selection cost**, not from better prediction of the
chosen route.

## Holdout gates for C

| Gate | Result | Pass |
|---|---|:-:|
| G1 attainment | 22 met vs 16 for stock and 17 for the duplicate. Gain 6, one stock/duplicate disagreement | ✓ |
| G2 no new misses | None | ✓ |
| G3 objective | 16 cases met by both used 600,905 B in both arms. The `size` spec produced the stock frame on all 11 files | ✓ |
| G4 selection cost | Geometric-mean paired encode time 0.423×. Max paired peak encode RSS 1.143× on nettle `decode>=30MB/s`, and 1.142× on pzstd `decode>=1MB/s` | ✗ |
| G5 integrity | 3,960 round trips, all exact, with frames identical across rounds | ✓ |

The two RSS cases chose the same route as stock, `ratio`. They used about 2.5 MiB more because C's trial set and
order differ, and the selector keeps the best whole-chunk trial frame. The duplicate stock arm's worst case was
1.058×, so the increase is outside timing noise.

| Holdout breakdown | stock | stock-dup | C |
|---|---:|---:|---:|
| Encode floors met, out of 33 (16 feasible by oracle) | 5 | 6 | 9 |
| Decode floors met, out of 33 (13 feasible by oracle) | 11 | 11 | 13 |
| Met with the oracle's smallest passing frame | 16/16 | 17/17 | 21/22 |
| Stored bytes over all 66 floor cases | 2,670,058 | n/a | 3,146,062 (+17.8%) |
| Stored bytes in the 44 cases both arms missed | 1,827,139 | n/a | 2,253,889 (+23.4%) |

C's gains were `encode>=1MB/s` on libunistring, openssl NEWS, pzstd and libtextstyle (the duplicate also met
libtextstyle), plus `decode>=30` and `decode>=80` on libunistring. A startup-aware model predicts most small-file
floors as infeasible, which is correct for 37 of 66 cases by oracle. The selector then takes its documented fallback,
the fastest predicted decoder `speed`. On these files under this load, `speed` usually missed the floor anyway, so
the user paid +23% bytes for no attainment. **A startup-aware estimate needs a better infeasible fallback, such as
the stock or size choice, before it can preserve the speed/ratio objective.** The current fallback is outside
calibration control.

## Reference replay on the recorded B26 inputs (after the decision, explanatory only)

This replay compares stock and C on gmp.info-1, gpgv and the GnuTLS PNG, with 1 warmup and 7 rounds. All 552 round
trips were exact. [Cases](reference/reference-cases.csv)

- GMP `decode>=30`: C chose `hc` at 69.2 MB/s, meeting the floor where B26 found a miss. GMP `encode>=1` was also
  met, at 3.4 MB/s charged versus 0.8 for stock. GMP `decode>=80` and `encode>=60` still missed under this load.
- gpgv `decode>=80`: **new miss**. C's development-fitted rate for `ratio` on the class it assigned to gpgv admitted
  `ratio`, which reached 43 MB/s. Stock's `hc` reached 91 MB/s.
- PNG `encode>=1`: both arms met it, but C stored 7,063 B versus stock's 6,552 B.
- Overall: 9 cases met versus 8 for stock.

## Host conditions (limitation)

The machine-wide timing lock was held from 19:32Z to 20:56Z. Other sessions' workloads ran outside the lock,
including PAQ8PX, review40 runs, ASan test suites and other worktrees' benchmarks. The 1-minute load average was 80
at start and 238 at the end. Logged holdout samples (from round 4 on) had median 128.5, maximum 259, on 10 cores. The reference replay ran
at about 310. Paired ratios and the duplicate control limit the bias on comparisons, but absolute end-to-end rates,
and so floor verdicts, are depressed. The launch constants were probably inflated too. This experiment does not
show what the floors would be on a quiet host.

## What a next attempt would need

1. A quiet host, or the timing lock honored by every heavy job, with a **new** sealed holdout, because this one has
   now been seen.
2. A fallback for floors predicted infeasible that keeps the size objective's frame, or the stock choice, instead
   of `speed`. This needs a native change behind an opt-in, so it is out of scope here.
3. A launch-aware table built from end-to-end fits per setting over many producers, instead of `kolmo calibrate`'s
   two-point in-process fit on a 7-file development set. The fit produced implausible entries such as `hc text` at 1.0 MB/s
   encode, and it mispredicted gpgv.
4. A memory gate that allows for trial-set changes, or a selector that frees losing trial frames earlier. Either
   change needs a new preregistration; this run's gate stands.

## Reproduce

```sh
# corpus: copy each manifest "source" to DIR/dev/<path>, DIR/holdout/<path> (and DIR/reference/<path>); hashes are checked
TIMED TAG python3 benchmark/learning-startup-floors/run.py measure \
  --encoder build/kolmo --reader /opt/homebrew/Cellar/kolmo/0.0.2/bin/kolmo --corpus DIR --out FRESH   # TIMED = the host timing-lock wrapper
python3 benchmark/learning-startup-floors/run.py score --out FRESH
TIMED TAG python3 benchmark/learning-startup-floors/run.py reference --encoder build/kolmo \
  --reader /opt/homebrew/Cellar/kolmo/0.0.2/bin/kolmo --corpus DIR --table FRESH/C-host-launch.tab --out FRESH-REF
```

`run.py` verifies both executables' SHA-256 values and every input. It uses seed 20261125 (the reference replay uses
seed + 1). The holdout candidate is picked automatically by the preregistered development rule. Prior evidence used
as reference: B26 [target floors](https://github.com/adnaneben/kolmo/blob/codex/ppp-audit-and-results-20261125/benchmark/waves/2026-11-25/overnight/target-floors/README.md)
(24 paired floor cases) and [selection regret](https://github.com/adnaneben/kolmo/blob/codex/ppp-audit-and-results-20261125/benchmark/waves/2026-11-25/overnight/selection-regret/README.md),
and the #99 learner (186 round trips, neither split qualified). This run follows the same rules: sealed producer
splits, paired rounds, charged wrapper or selection cost, exact stock-reader verification, and an explicit negative.
