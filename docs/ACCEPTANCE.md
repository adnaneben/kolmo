# Acceptance standard

Adopted by the owner on 26 November 2026. Every change to Kolmo is judged against it. That covers code, level tables,
routing, codecs, transforms and models, whoever writes them (Claude, Codex, people). `benchmark/effectmap/` puts it
into practice, and every PR reports its effect map and verdict in the PR template's **Effect map** section.

The aim is for Kolmo to beat the world leaders at every level, in every lane, on every type and size. Averages hide
where a change wins and where it loses. So a change is judged **per condition**, against an **exchange rate set per
tier**, and every pattern it shows becomes **routing**.

## 1. Measurement

### Rules

- **Paired on/off runs.** Build A (the base, usually `main` at the PR's merge base) and B (the change). Measure them on
  the same inputs, in the same session, interleaved: each round runs every arm once, and the arm order rotates from round
  to round.
- **A/A control.** Every timed run also runs A a second time, as its own arm (A2). The spread of A2 against A is the
  noise band. A speed effect counts only if it falls outside that band. Otherwise it is reported as "within noise" and
  scored as 1.00×.
- **At least 5 rounds** for timings. Report the median over rounds of the paired ratio B/A within each round.
- **Quiet machine.** Timed runs go through `timed.sh` (the machine-wide timing lock). A round starts only when the
  1-minute load is under 12, and a round that ends at 12 or above is discarded and run again. On a busy machine, run
  only the sizes-only part (exact bytes, plus instructions retired, which do not depend on load). Queue the timed
  part in `QUIET_WINDOW_QUEUE.txt`.
- **One thread for every tool** in comparisons with other compressors (`-T1`, `zstd -T1`, `xz -T1`, ...). Kolmo is also
  measured at 4 and 10 threads, for its own behaviour.
- **Every output is decoded and compared** (SHA-256) with the input, by the decoder of the build that wrote it and, when
  the format is meant to be unchanged, by the stock decoder too.
- **Never average away a loss.** Report the worst cell and the worst file next to every total. A total that improves
  while one type gets worse is reported as both.

### Dimensions

| dimension | values |
|---|---|
| file type | the impact corpus kinds (text, code, JSON, XML/HTML, CSV, TSV, logs, numeric, weights, 8- and 16-bit images, audio, video, executables, containers, genomics, tar) plus any type the change targets |
| size class | **S0** < 4 KiB, **S1** 4 KiB-1 MiB, **S2** 1-64 MiB, **S3** > 64 MiB |
| level | 1-9 and ultra; at least every level the change can reach, **and the level below it**, to check level order |
| threads | 1, 4 and 10 (bytes must be identical across them; speed is measured at each) |
| phase | encode and decode separately: wall time, CPU time, instructions retired, peak RSS |
| CPU | Apple M5, x86-64 AMD and x86-64 Intel (the hosted x86 runs; at least one x86 before a default changes) |
| compressibility | already-compressed and random inputs next to the corpus (worst cases) |
| input path | file, pipe (stdin to stdout) and streaming API, when the change touches I/O or block planning |
| data split | **dev** (files earlier lanes tuned on) and **holdout** (files no lane tuned on, sealed). Rules are found on dev and checked on holdout |

The default corpus is the impact corpus (`benchmark/impact/mkcorpus.py`, `$V/impact/corpus`). It has dev and holdout
4 MiB slices (S2) and 16 KiB cuts (S1). The effect-map tool adds 1 and 3 KiB prefix cuts (S0) and any large files
passed with `--large` (S3: enwik8, silesia.tar, the Mozilla tar).

## 2. Stacking

A change is measured **alone** and **combined with every existing codec and transform**:

- the coders KLZ, KLZH2, KLZMA, BWT, CM and GR6+, forced through `kolmo-impact` (`KOLMO_IMPACT_CODEC`) on the plain
  block and on the level's transformed stream (`KOLMO_IMPACT_INPUT=stream`);
- the transforms, through the analysis settings (`KOLMO_IMPACT_TRANSFORMS`) and the transform ablation
  (`KOLMO_IMPACT_NOTF`);
- the type paths a level picks itself (kimg, Kaud audio, Kvid video, the table, log and record transforms). These are
  measured on the type cells where the level routes to them; the per-block log (`KOLMO_IMPACT_LOG`) shows which codec
  and transform each block used.

For each partner *s* and type, the stacking table gives the gain alone (B/A), the gain with *s* (B+s / A+s) and the
label:

- **amplifies**: the gain with *s* is at least 0.5 points larger than alone. The combination is **kept and routed** to
  that type, even if the change alone fails.
- **stacks**: about the same gain with and without *s*.
- **absorbed**: *s* already captures the gain (the gain with *s* is under half the gain alone). Do not route both.
- **conflicts**: the combination is larger than *s* alone.

## 3. Per-tier exchange rates

Notation, per condition cell (B against A):

- `bytes` = B bytes / A bytes, so 0.99 means 1% smaller;
- `enc`, `dec` = speed ratios A time / B time, so 0.90 means 10% slower and 1.00 means within noise;
- `comb` = (A encode + A decode) / (B encode + B decode), the combined speed;
- `lat` = 1 / `comb`, the combined latency;
- `rss` = B peak RSS / A peak RSS, the larger of the two phases.

**Gain threshold.** On top of its tier's rate, a change must gain at least **0.5%** (smaller, or faster beyond noise) on
at least one type.

| tier | levels | a cell passes when |
|---|---|---|
| **Fast** | 1-3 | `comb` ≥ 0.95 with `bytes` ≤ 1.00, **or** `bytes` ≤ 0.99 with `comb` ≥ 0.90 |
| **Middle** | 4-7 | `dec` ≥ 0.95, `enc` ≥ 0.80, and at least 1% smaller per 10% of encode time lost: `1 - bytes` ≥ (1/`enc` - 1) / 10 |
| **High** | 8-9 | Codex's gate: `bytes` ≤ 0.98, `enc` ≥ 0.67, `dec` ≥ 0.95, `rss` ≤ 1.10, `lat` ≤ 1.5. A gain under 2% (0.98 < `bytes` < 1) needs `enc` ≥ 0.90 |
| **Ultra** | ultra | `bytes` ≤ 0.99, `enc` ≥ 0.5, `dec` ≥ 0.8 |

Codex's weight-specific gate for 8-9 (`enc` ≥ 0.55, `dec` ≥ 0.97, `lat` ≤ 1.65, on model weights only) stays available
as a routed exception for the weights type.

**Size losses.** A cell that is larger (`bytes` > 1) passes only if it stays within its tier's rate in the other
direction (for example, at the middle tier, up to 1% larger per 10% faster encode) and is under 0.5% larger. Such cells
are listed in the report. A bigger size-for-speed trade is not a pass: it goes to section 6.

### Always kept

- Correctness and safety fixes (data loss, crashes, decoder hardening, undefined behaviour), whatever they cost. Their
  cost is still measured, reported and then reduced in a follow-up.
- Byte-identical speedups: every frame unchanged and a speed gain beyond noise.
- Free size gains: smaller on some type, larger on none, no phase slower beyond noise, RSS within 1.10×.
- Changes that remove a "never allowed" case below.

### Never allowed

- A level whose output is larger than the level below it on any file of the corpus (a level inversion), unless the
  file is listed in `benchmark/ledger/level_order_exceptions.txt` with a reason. A change may not add an inversion.
- Slower decode at levels 1-7 beyond the noise band.
- Peak RSS above 1.10× in either phase, at any level, except as an explicit new mode (section 6).
- Output bytes that depend on the thread count. Frames at 1, 4 and 10 threads must be identical.

## 4. Patterns become routing

A change that wins somewhere is not averaged away; it is routed there.

1. **Find the pattern.** From the per-condition effect table, fit a small, interpretable model of where the change
   passes. The effect-map tool fits a depth-2 decision tree over cheap features (type, size class, level), read as a
   rule list.
2. **Check it on held-out data.** The rules are fitted on the dev split only and scored on the holdout split. A rule
   that does not hold on holdout is not a gate.
3. **Explain why it holds.** The PR says what in the data makes the change win there (for example: the 2-bit literal
   floor only binds on blocks over 256 KiB; m3 matches cost 16-bit images). A gate with no explanation is a hypothesis.
4. **Make it a gate on cheap features**: type (as the planner detects it), block or input size, level and transform
   flag. A gate must cost less than what it saves.
5. **Gates that change output bytes may never depend on thread count.** Thread count may only steer gates whose
   output is byte-identical (scheduling, buffer sizes, warm-up sharing).
6. **Record it and guard it.** Each gate goes in `docs/ROUTING-MAP.md` with its evidence. Its files go in the CI size
   ledger (`benchmark/ledger/sizes.tsv`), so a later change that breaks the gate shows up. The per-condition rows are
   kept for opt-in learning (`benchmark/effectmap` rows are plain JSONL for that purpose).

Dropped codecs and transforms are measured again under this standard before they are removed for good.

## 5. Report

Every PR that can change bytes, speed or memory includes, in its **Effect map** section:

- the command that produced the map and the two builds (commit and binary SHA-256);
- the effect table per condition with the A/A noise band, the worst cell and the worst file;
- the stacking table;
- the rules found on dev and their holdout score;
- the per-tier verdict, and for a trade, the decision in section 6;
- the suggested routing gate, and whether the PR implements it.

A correctness fix can mark the section "not applicable: no change to output or speed", if the determinism check and
the ledger show identical bytes, and the cost is within noise.

## 6. Ratio-vs-speed trades

A change that buys ratio with speed beyond its tier's rate, or speed with ratio, is neither simply accepted nor
simply rejected. It is decided as one of:

**A new mode.** Possible modes:

- **ultra+**: above ultra. Ratio first. Encode and decode may each be several times slower than ultra; memory is stated.
- **cold-storage / archive**: slow compress and slow decompress for the highest ratio on data that is written once and
  read rarely (backups, logs at rest). Judged on bytes, with decode bounded only by a stated floor (MB/s) and memory
  by a stated cap.
- **video-archive**: very slow compress, but decode must reach at least the video's playback rate. Measure decode in
  **frames per second** and compare with the source's frame rate (from the y4m header or container): a cell passes
  only if decode fps ≥ source fps at one thread on the reference machine (M5) and on x86.

**A rebase of an existing level.** The level's place on the scale moves (for example, `-5` becomes faster and slightly
larger). This is done only when the new point is better placed against the world leaders, and when level order and the
rule "no slower decode at 1-7" still hold.

Either way, the trade is **placed on the perf-graph chart** (`benchmark/perfgraph`: ratio against combined speed, one
thread, every tool) next to the world leaders at that point: zstd, xz, brotli, bzip3, 7-Zip, zpaq, and paq8px/cmix for
ultra and above. The PR says which leader the new point beats, matches or trails, on which types. A trade that loses to an
existing leader point on every type is rejected.

## 7. Tooling

| tool | use |
|---|---|
| `benchmark/effectmap/` | this standard: effect table, stacking, rules, verdict, gate (see its README) |
| `benchmark/impact/` | forcing codecs and transforms on blocks (`kolmo-impact`), the corpus, the per-type matrix |
| `benchmark/perfgraph/` | ratio vs combined speed charts against the world leaders, A/A and bisect (PR #114) |
| `benchmark/ledger/` | CI size ledger and level-order exceptions |
| `benchmark/determinism/` | frame hashes, to show bytes unchanged |
