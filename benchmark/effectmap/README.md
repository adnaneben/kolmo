# effectmap: the acceptance standard applied to one change

`docs/ACCEPTANCE.md` is the standard. This tool measures one change against it and writes:

- **the effect table**, per condition (type × size class × level × threads): bytes, encode, decode, combined speed
  and peak RSS of B against A. It is paired and interleaved (A, B and A2 in each round, the order rotating), with an
  A/A noise band, and speed inside the band counts as 1.00;
- **the never-allowed checks**: level inversions added or removed, output that depends on the thread count, slower
  decode at 1-7, RSS above 1.10×;
- **the stacking table**: the change alone and combined with each existing coder or transform, forced through
  `kolmo-impact` and labelled amplifies, stacks, absorbed or conflicts;
- **rules**: a depth-2 decision tree over type, size class and level, fitted on dev and scored on holdout. Thread
  count is offered only when every frame is unchanged;
- **the verdict**, per tier and overall: KEEP, PASS, ROUTE, TRADE or FAIL;
- **a suggested routing gate**.

## Two modes

| mode | when | what it measures |
|---|---|---|
| `sizes` (default) | any time, also on a busy machine | exact bytes at threads 1/4/10, a round trip of every frame, and **instructions retired** per phase (`proc_pid_rusage`, which does not depend on load) as the cost. Also RSS, and wall time that is recorded but not used. One round, under `nice -n 15`, at most 2 processes |
| `timing` | quiet window only | wall time, 5 interleaved rounds, one process, each round only at 1-minute load < 12, a round that ends above it is run again. It runs only inside `timed.sh`: `effectmap.py queue` writes the job and appends it to `$V/evidence/QUIET_WINDOW_QUEUE.txt` |

When both row files exist, the report uses wall time for speed and keeps the stacking rows from the sizes run.

## Use

Two builds (A = base, B = change); add their `kolmo-impact` builds for stacking:

```
make build/kolmo kolmo-impact            # in a worktree of each revision
python3 benchmark/effectmap/effectmap.py run --out $V/effectmap/my-change \
    --a main=path/to/kolmo-main --b pr=path/to/kolmo-pr \
    --a-impact path/to/kolmo-impact-main --b-impact path/to/kolmo-impact-pr \
    --levels 5,6,7 --partners klzma,bwt,stack-klzma,stack-bwt,notf,tf-none
python3 benchmark/effectmap/effectmap.py report --out $V/effectmap/my-change
python3 benchmark/effectmap/effectmap.py queue  --out $V/effectmap/my-change --a ... --b ... --levels 5,6,7
```

A research switch in one `kolmo-impact` binary (A = no switch, B = the switch; stacking adds each partner's
variables to both):

```
python3 benchmark/effectmap/effectmap.py run --out $V/effectmap/lex \
    --impact build/kolmo-impact --switch KOLMO_CM_LEX=1 --switch KOLMO_IMPACT_CODEC=7 --levels 9,ultra \
    --partners stack-bwt,notf
```

Options: `--levels` (include the level below the change's lowest level, for the inversion check), `--threads`
(default 1,4,10), `--sets dev,holdout`, `--kinds`, `--inputs`, `--cuts 1024,3072` (S0 prefix cuts of every 4 MiB slice,
cached in `$V/effectmap/cuts`), `--large PATH[:kind[:split]]` (S3 inputs), `--size-classes`, `--stack-sizes S1,S2`, `--reps`
(timing), `--max-load`.

Outputs in `--out`: `rows-sizes.jsonl` / `rows-timing.jsonl` (one row per file, level, thread count and round,
every arm's bytes, SHA-256, cost and RSS: plain JSONL, kept for opt-in learning), `meta-*.json` (builds and their
SHA-256), `cells.csv`, `verdict.json` and `effectmap.md`. Paste the verdict lines and the per-type table into the PR's
**Effect map** section.

## How it decides

- Per file and per cell, `standard.classify` (in `standard.py`, the formulas of ACCEPTANCE.md section 3) gives one
  of these classes:
  - `KEEP-ID`: identical frames, faster;
  - `KEEP-FREE`: smaller, not slower, RSS within 1.10×;
  - `NEUTRAL`: identical frames, no speed change beyond noise;
  - `PASS`: within the tier's exchange rate;
  - `TRADE`: a size-for-speed trade beyond the rate;
  - `FAIL`: everything else.
- A cell's bytes are the sum of B over the sum of A. Its speed ratios are geometric means of the per-file ratios,
  snapped to 1.00 inside the cell's noise band. That band is the widest A/A spread per file (per thread count and
  size class, pooled over levels) divided by √n, and never less than the cell's own A/A deviation or the floor
  (0.3% for instructions, 1% for wall time). Using the widest spread, not the p95, keeps the ~400 cells of a run
  from raising ~20 false alarms. The per-file p95 band is reported for reference.
- Memory is the peak physical footprint (`proc_pid_rusage`; it counts compressed pages). Under memory pressure,
  `ru_maxrss` varied by 9-22% between A and A2; it is still recorded, and used off macOS. An increase of 4 MiB or
  less is not counted. A cell's RSS is its median file. In sizes mode (one round under load), RSS and decode flags
  are listed as "to confirm in the timed run": an RSS flag counts only if it holds at every thread count. The
  report also shows the median footprint per level and size class, so systematic shifts are visible.
- Rules: each condition row (file × level × threads) is a sample, labelled with its cell's class (win = PASS or
  KEEP; NEUTRAL rows are left out). The tree is fitted on dev, and splits whose leaves all predict the same are
  merged. The leaves that predict "on" and also hold on holdout make the suggested gate.
- The overall verdict is checked in this order:
  1. any never-allowed case: **FAIL**;
  2. no 0.5% gain on any type: **FAIL**;
  3. only `KEEP-*` and `NEUTRAL` cells: **KEEP**;
  4. no losing cell: **PASS**;
  5. winning and losing cells: **ROUTE**;
  6. only trades: **TRADE** (new mode or rebase, ACCEPTANCE.md section 6);
  7. otherwise: **FAIL**.
  A change that removes a level inversion is kept at least where it removes it, and the gate says so.

## Limits

- Instructions are a cost proxy, not time. Wall time needs the timed mode.
- The impact corpus has no S3 inputs, and its S0 inputs are prefix cuts. Pass `--large` for S3.
- Only one CPU per run. Re-run on x86 (Codex's hosted runs) before a default changes.
- Stacking forces a partner per block through `kolmo-impact` and keeps it where it is smaller (`POLICY=best`).
  kimg, Kaud, Kvid and the table, log and record transforms are measured where the level routes to them, on the
  image, audio, video, table and log cells of the main table, rather than forced.
- Pipe, stream and compressibility dimensions need extra inputs. Pass them with `--large`.

Code reused: `measure.py` is perfgraph's `cell.run` (PR #114) with the instruction counter added. The partners are
the `benchmark/impact/run.py` features, and the inputs are the impact corpus manifest.

## Validation

`VALIDATION.md` holds the effect maps of PR #121 and PR #37, set against the evidence in those PRs.
