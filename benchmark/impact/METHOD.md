# Impact matrix: method

The owner's rule (2026-11-24): every research codec, transform or model is measured across many file types, at every
level where it could run, at one thread and at many threads. A feature that wins on one kind of file only is kept and
routed to that kind. This directory holds the harness, the raw rows and the results (`RESULTS.md`).

## Harness

`build/kolmo-impact` (`make kolmo-impact`) is the CLI compiled with `-DKOLMO_IMPACT`, which includes
`src/core/impact.c` and the research parts of `src/codecs/cm.c`. Nothing else is built with that flag: the library, the
`kolmo` binary, the bindings and the tests are unchanged, and `kolmo-impact` without its variables writes the same
bytes as `kolmo` (checked with `benchmark/determinism/check.py` on both binaries).

After the level has chosen a block's coding (`compress_worker`, just before the block is written), `impact_block`
can code the block again under research settings:

| variable | effect |
|---|---|
| `KOLMO_IMPACT_CODEC=N` | also code the block with codec or role N (7 CM, 18 BWT, 24 KLZMA, 26 GR6+, 32 KLZH2, ...) |
| `KOLMO_IMPACT_CLEVEL=L` | that codec's own level (default 9; CM 0 = model 4 as ultra codes it, 1 = model 1) |
| `KOLMO_IMPACT_INPUT=stream` | code the level's transformed stream instead of the plain bytes (stacking: plan, tflag and meta kept) |
| `KOLMO_IMPACT_POLICY=force` | keep the trial on every block it codes (default `best`: only where smaller) |
| `KOLMO_IMPACT_GATE=0` | GR6+ on every block, not only where `gr6_applies` admits it |
| `KOLMO_IMPACT_NOTF=1` | ablation: the level's own coder on the plain bytes instead of its transform, kept |
| `KOLMO_IMPACT_TRANSFORMS=n` | override the level table's analysis setting (0 none, 1 full, 2 quick, 3 minimal) |
| `KOLMO_IMPACT_LOG=path` | one JSON line per block: the level's codec, tflag and size, the trial's size, kept or not |
| `KOLMO_CM_LEX=1` | CM model 4 adds the rc3w3 lexer's two token-class contexts on text streams (mask bits 30, 31) |
| `KOLMO_CM_KPRIOR=1` | CM model 4 starts a text stream under 4 MiB from the rc3w3 per-kind prior of its kind (ids 3-10) |

Every frame the harness writes is an ordinary version-12 frame except with `KOLMO_CM_LEX` / `KOLMO_CM_KPRIOR`, whose
CM streams use a mask bit or prior id that only the research decoder reads (the stock decoder refuses them; no format id
is claimed). The runner decodes every frame with the stock `kolmo -d` (the research decoder for those two switches) and
compares the SHA-256 with the input's.

The per-block log gives two sizes from one run: **kept** (the frame as written: the level's choice, replaced only where
the trial is smaller) and **forced** (the trial on every block it coded). Both are reported.

## Lexer and priors on main

The prior lane (`claude/rc3w3-prior-20261118`, `docs/PRIORS.md` and `benchmark/rc3w3/prior/RESULTS.md` there) built
its "model 4" (model 3 + two token-class contexts) and eight per-kind priors (hash-free tables of a model-3 coder
trained on public data of each kind) on rc3's CM, measured at `-x` only, on a 4-vCPU cloud Xeon. Main's CM has since
moved to a different model 4 (a masked subset of model 3's contexts, 16-bit mixer, `benchmark/w4/cm`). The port puts:

- the two token-class contexts as model 4 contexts 30 and 31 (the lane's hashing code unchanged);
- the priors, unpacked by `kprior.py` from the lane's `cm_kprior.inc` (liblzma is gone from main), mapped onto model 4:
  each slot takes the state map of the context it stands for, mixer rows w1-w3 fill the 16-bit rows (16.16 / 16), w4's
  mean row is the template of the order-1 mixer, plus the final mixer, the order-1 table and the match counters. The
  token-class contexts keep their start values.

The kind detector (`src/algo/texttype.c`) is the lane's file, compiled only into the research build.

## Corpus

`mkcorpus.py` cuts 53 real files to at most 4 MiB (text at a line end, video at a frame end), 15 kinds: text, code,
JSON, XML/HTML, CSV, TSV, logs, numeric arrays, weights, 8-bit and 16-bit images, audio, video, executables,
containers (XLS, PDF, JAR), genomics (FASTQ) and tar mixes. Sources are the review workspace's corpora, the bench page's
downloads and a few system files (bash's HTML and PDF manual). Each file is tagged **dev** (the corpora earlier lanes
tuned on: `corpus/`, Canterbury, Silesia members that were tuned on) or **holdout** (`corpus-holdout/`, the bench page's
holdout sets, files no lane tuned on). `manifest.jsonl` lists each with its SHA-256 and source.

Limits: 4 MiB slices mean one to four blocks at levels 1-7 and one block at 8, 9 and ultra; GR6+, CM and BWT gain with
longer blocks, so the slices understate what ultra gets on whole files. Numeric slices cut a `.npy` header's declared
shape (the planner then sees a mismatch, as it would on any truncated file).

## Timing

`run.py` runs one cell at a time under the machine-wide lock (`$V/locks/timing.d`, the `timed.sh` protocol), and yields
it for 16 s every 5 minutes so other sessions' `timed.sh` jobs get in. Per cell: compress with `/usr/bin/time -l` (wall,
user + sys, peak RSS), decompress likewise, SHA-256 of the output. Each row records the 1-minute load average. Other
sessions built and tested on this 10-core M5 throughout (load 48-176 while these rows were taken), so wall times and
MB/s are indicative; ratios within one row pair (feature vs the level's frame, run back to back) are less affected than
absolute speeds. Sizes are exact and do not depend on the thread count (the summary checks that every T1 and T10 frame
pair is identical).
