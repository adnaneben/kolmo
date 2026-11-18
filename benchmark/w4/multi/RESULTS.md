# Several instances at once: the machine-wide pool (w4, multi lane)

Design: `docs/CONCURRENCY.md`. Harness: `benchmark/w4/multi/run.py` (raw figures: `results.json`).

**Setup.** Linux VM, 4 cores (no SMT, so physical = logical = 4 tokens), 16 GB. `build/kolmo` of this branch. N
instances start at once, instance i compressing its own generated file (text with table rows, a different seed each)
with the default thread count (`-T 0`, 4 threads). *Off*: `KOLMO_JOBS_SHARED=off` (the in-process governor alone, as
before this branch). *On*: the default. Median of 3 runs. *Peak MiB*: the largest sum of the instances' resident
memory, sampled every 5 ms. **Every frame was byte-identical between on and off** (the harness fails otherwise).

## Results

### balanced (24 MiB per instance, 6 blocks of 4 MiB)

| N | wall off (s) | wall on (s) | on/off | MB/s off | MB/s on | peak MiB off | peak MiB on |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.12 | 0.09 | 0.79 | 218 | 277 | 137 | 131 |
| 2 | 0.14 | 0.13 | 0.96 | 362 | 376 | 122 | 134 |
| 4 | 0.25 | 0.24 | 0.98 | 404 | 411 | 226 | 212 |
| 8 | 0.54 | 0.48 | 0.89 | 376 | 422 | 473 | 370 |
| 16 | 1.06 | 1.02 | 0.96 | 379 | 394 | 1005 | **522** |

### ratio (8 MiB per instance, one block: BWT with the suffix sorter's crew)

| N | wall off (s) | wall on (s) | on/off | MB/s off | MB/s on | peak MiB off | peak MiB on |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 8.46 | 8.23 | 0.97 | 1.0 | 1.0 | 135 | 135 |
| 2 | 9.37 | 8.55 | 0.91 | 1.8 | 2.0 | 253 | 254 |
| 4 | 11.40 | 9.32 | **0.82** | 2.9 | 3.6 | 488 | 489 |
| 8 | 23.94 | 19.40 | **0.81** | 2.8 | 3.5 | 967 | 994 |
| 16 | 46.42 | 37.60 | **0.81** | 2.9 | 3.6 | 1978 | 1966 |

### -x (512 KiB per instance, one block: CM with its trials and preset race)

| N | wall off (s) | wall on (s) | on/off | peak MiB off | peak MiB on |
|---:|---:|---:|---:|---:|---:|
| 1 | 4.41 | 4.36 | 0.99 | 655 | 655 |
| 2 | 5.87 | 7.37 | 1.25 | 1310 | 824 |
| 4 | 9.74 | 10.02 | 1.03 | 2615 | 1647 |
| 8 | 19.62 | 20.20 | 1.03 | 4945 | 2488 |
| 16 | 42.18 | 40.09 | 0.95 | 9590 | **4171** |

**Start-up**, 300 sequential runs on a 64 KiB file: off 5.60 ms, on 5.16 ms per run (noise; joining the pool is a
`mkdir`, one file created and locked, one directory scan).

## Reading

- **One instance costs nothing**: within noise in every mode (balanced N=1, 0.12 → 0.09 s, is run-to-run variation: the first run gave 0.115 → 0.117 s; ratio
  0.97, -x 0.99, start-up unchanged).
- **Oversubscription, ratio: 18-19% less wall time from N = 4** (the block-sorting crews of N instances no longer run
  4N threads on 4 cores). Memory is the same: each instance has one block, and every instance always runs its block.
- **Oversubscription, balanced: half the peak memory at N = 16** (1005 → 522 MiB: 4 blocks in flight machine-wide plus
  one per instance, instead of 4 per instance), and 4-11% less wall time from N = 4.
- **-x: half the peak memory from N = 4** (9.6 → 4.2 GiB at N = 16; 4.9 → 2.5 GiB at N = 8). This is the memory
  pressure the pool exists to remove: on a 16 GiB machine, 16 concurrent `-x` without it are at 60% of RAM for 512 KiB
  inputs. Wall time: 5% better at N = 16, 3% worse at N = 4 and 8, **25% worse at N = 2**.

### The -x trade-off at small N, and the first measurement

`-x` starts optional threads (the codec trials, the CM preset race, the codec-25 parts) that decide *now* whether they
run in parallel; refused, they run inline later on the owning thread (same bytes). Without the pool, two instances run
about 2 x 7 threads on 4 cores and the kernel interleaves them well; with it, an instance whose trial found every token
taken (by the other instance, at that instant) codes that trial after its main coder, even if a token frees a moment
later. At N = 2 that costs 25% of wall time and saves 37% of peak memory.

The first run of this benchmark had a worse case: a **single** `-x` instance took 7.80 s instead of 4.89 s, because the
same thing happened with the instance's *own* tokens. Fixed before these figures were taken: a process holding every
token is alone and is limited by its own thread count only (`docs/CONCURRENCY.md`, *Progress*, rule 4). Raw figures of
that first run: `results-first-run.json`; its balanced and ratio figures agree with the tables above within noise.

A follow-up that would remove the small-N cost: let optional work wait (briefly, holding nothing) for a token to free
instead of deciding at once, or queue it so a freed token starts it. Not done here: it changes the order in which a
block's work is started, and the rule "optional work only tries" is what makes the memory side deadlock-free; it needs
its own measurement.

## Not measured here

- The Apple M5 (10 cores, the owner's machine): the macOS paths (`flock`, `hw.physicalcpu`) are the same code but were
  not built in this lane.
- SMT machines: there `-T 0` (logical CPUs) exceeds the tokens (physical cores). A single instance is unaffected (rule
  4); under contention, instances share physical cores.
