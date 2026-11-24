
## 3. Gaps, ranked by expected gain

"Gap" here is against what a leader's user experiences on the second, tenth and thousandth file of a kind: zstd,
xz, bsc and zpaq run one fixed algorithm per level, with no per-file search of this kind; OpenZL moves the search
into an explicit offline training step whose result the user must carry to every later compression; Kolmo pays the
full search on every file and forgets the answer.
The recorded encode-speed deficits that this search feeds (`benchmark/rc3/leaders/RESULTS.md`, one thread):
`ratio` compresses at 0.42x to 0.76x of its tier leader (bzip3/bsc), `-x` at 0.73x of zpaq -m5 on text, `balanced`
at 0.25x to 0.4x of zstd -3 on tables, arrays and audio.

| # | gap | measured tonight | where the time goes | fix |
|---|---|---|---|---|
| G1 | `ratio`: full codings that lose | 25.7% of encode over Silesia (mr 88%, nci 71%, samba 50%, mozilla 10%) | LZMA2 coded in full then beaten by block sorting; block sorting attempted on members where it always loses | experience store, block trials (P1) |
| G2 | `-x`: expensive coder beaten by a cheap one, alternatives that never win, graph codings on images that end as codec 17 | osdb 97.6%; 8 to 17% on four binaries; 25.6% on 16 images | CM pass thrown away; LZMA2/BWT alternatives; LZMA2/BWT graph leaves | P1 |
| G3 | `-x`: the preset race and the planner's CM sample passes on small inputs | section 2.5 | 4 presets on the first 256 KiB; two 1 MiB CM passes for a discarded word plan | P1 (race memo), P2 (plan memo), P5 (deterministic) |
| G4 | `balanced`: analysis repeated on every file | 27.5% of encode on 14 files (tracker: 20.8% on 79) | record search, typed trials, table/log trials | experience store, plan memo (P2) |
| G5 | `ratio`/`-x` text: dead word-transform trial | 10 to 21% of `ratio` encode on dickens, reymont, xml; 9.7 to 13.7% of `-x` | planner trial whose only effect is one plan byte | P2 for repeat kinds, P5 for all files |
| G6 | parameters outside today's search (LZMA2 lc/lp/pb; CM learning settings beyond 4 presets and above 16 MiB) | section 2.6 | not searched because a per-file search costs too much | P3: the store spreads the search over files |
| G7 | `--target` speed table fitted once per machine | not measured tonight | `KT_DEFAULT` / `kolmo calibrate` | P4: the store's timings refresh it |

## 4. Performance improvement plan

### P1 (execute, claim #32 `xp-store-trial-skip`): the experience store and its first users, the block trials

**Principle.** Everything the store influences is a choice the rc3 encoder already makes between codings that the
rc3 decoder already reads (codec id per block in the index, plan bytes, LZMA2 properties and CM learning settings in
the payload). The store changes *which candidates are coded and in what order*, never what a coding looks like.
So every output decodes with any rc3 decoder that never saw the store. No new codec id, no container change.

**Design.**

1. *Type key.* A 64-bit XXH3 of coarse features the planner already computes: container kind from `plan/meta.c`
   (tar/zip member, ELF/PE/Mach-O section kind, NPY dtype and rank, PNM/BMP/TIFF channels, WAV/AIFF layout,
   safetensors/GGUF tensor kind, none), the six-way class of `kt_class` (text, table, log, json, numeric, binary),
   the first-line shape for text (`kp_key`'s normalisation, 32 bytes), the size band (`kp_band`), mode, level and
   transform bits; block decisions add the plan type and flag bits, the tflag, a decision id and the block's size
   band. The CLI may add the lower-cased file extension as a hint (API: one setter); never the file name.
   Hashes and small counters only: no content, no names, nothing that identifies a file.
2. *Records.* Fixed 32-byte records in an append-only file (`KXP1` header, version, record size): key, decision id,
   winning arm, the cheap probe statistic the encoder had (slice ratio in 1/1024, or the race winner on the prefix),
   the full outcome (size ratio of the arms in 1/1024, 0 = not coded), block size band, a sequence number, optional
   coder times (for reports and P4 only, never read by a rule that changes bytes), CRC32C. At most 16 newest
   records per (key, decision) and 8,192 in all (256 KiB); compaction rewrites to a staged file and renames it
   (the pattern `cli.c` already uses), under `flock`. One `write()` with `O_APPEND` per call.
3. *Snapshot semantics (determinism).* A call reads the store once, before planning, into an immutable snapshot;
   observations are buffered and appended once when the call ends. The frame is a function of (input, parameters,
   snapshot) only: it never depends on `-T`, on timing, or on what other blocks of the same call observed.
   `--learn=use:FILE` reads without writing, so a pipeline can pin a store and stay bit-reproducible; `-v` prints
   the snapshot's fingerprint and how many hints were applied.
4. *Decision rule.* For a decision with arms A (reference) and B and history H of the key:
   - with a probe (the slice codings rc3 already makes): learn the *reading* of the probe, not the answer. From H,
     the offset between the probe ratio and the full ratio (osdb: slice 0.84, block 0.78; mr: 0.976 to 0.916) and its
     spread; the full coding of an arm is skipped only when probe + offset +/- spread is on one side of the keep
     threshold with a margin of twice that threshold; otherwise both are coded, exactly as rc3 does;
   - without a probe: an arm is skipped only when it lost in each of the last k = 3 observations by at least twice
     the keep threshold (BWT/LZMA2 >= 1.06 where rc3 keeps BWT at <= 0.97);
   - order: the candidate that history says wins is coded first, and a cheap candidate before an expensive one;
   - self-healing: every 8th time a skip would apply (a counter in the snapshot, so still deterministic) all arms
     are coded anyway and recorded; one contradicting observation clears the key's skip; records age out.
   A store can therefore only choose among codings the normal encoder could have produced. A corrupt, stale or
   hostile store costs size or time, never validity: every field is range-checked, records with a bad CRC are
   dropped, nothing read from the store sizes an allocation or indexes a table without a bound.
5. *Decision sites in `compress_worker`* (all "code both, keep the smaller" today): (a) `ratio` LZMA2 against block
   sorting, both slice gates, and the plain-bytes retry after a transform; (b) `-x` CM against LZMA2 / block sorting /
   plain alternatives (`xtrial_*` keeps working: a skipped trial is simply not started); (c) `-x` 8-bit images: the
   graph codings before codec 17; (d) the 4-preset race in `cm_compress_raced`: with history the remembered preset
   is coded directly, and on a stream of at most 256 KiB that means one coding instead of four.
6. *Surface.* `--learn[=FILE]` (default `~/.config/kolmo/experience`, beside the existing calibration table),
   `--learn=use[:FILE]`, env `KOLMO_XP`, `kolmo xp show|forget [FILE]`; library: `kolmo_set_experience(path, mode)`
   process-wide, like `kolmo_set_gpu`. Default **off**.

**Files.** New `src/plan/xp.c`, `src/plan/xp.h` (in the `kolmo.c` unit before `core/blocks.c`); hooks in
`src/core/blocks.c`, `src/core/compress.c` (snapshot, flush), `src/core/trials.c`, `src/codecs/cm.c` (race);
`src/kolmo.h`, `src/api/api.c`, `src/cli/cli.c`; `tests/test_xp.c` and a script test; `fuzz/` target for the store
reader; `SPEC.md` (a note that the store is not part of the format); new `docs/EXPERIENCE.md`.

**Expected gain and basis.** The measured discarded shares are the ceiling for a perfect predictor: `ratio` 25.7%
of Silesia encode time, `-x` 17.6% on eight Silesia files and 25.6% on 16 images, plus the race (2.5). What a store
can actually take is the part where history is decisive: mr-like 16-bit images (88% of that file's encode), nci-like
token streams (71%), archive members of one kind (mozilla: 4.3 s of 43 s), osdb-like tables at `-x` (97.6%), images
at `-x` (25.6%), small `-x` inputs (race). Where outcomes sit at the threshold (samba's binary segments) nothing is
skipped. Size: expected within +/-0.1% of rc3 on held-out files, by construction of the margins; to be measured, and
the worst file reported.

**Risks.** Format: none. Reproducibility: output depends on the snapshot, hence opt-in and pinnable. Size: a wrong
skip writes a larger (valid) block; bounded by the margins, the verification cadence and the reset rule. Speed: one
file read and one append per call (to be measured on 4 KiB to 1 MiB inputs at `-m speed`). Memory: snapshot at most
256 KiB plus an index. Binary size: measured (section 5). Security: the store is untrusted input (bounded, checked,
fuzzed, created 0600). Concurrency: single-`write` appends, `flock` for compaction, readers tolerate a torn tail.

**Success is measured by** (stage 2, all with round trips checked by `cmp`):
1. store off: frames byte-identical to the rc3 binary on Silesia, the image set and the R10 chunks, all six modes;
2. store on and empty: frames byte-identical to rc3 as well (recording must not change anything);
3. train / held-out by kind (mr <-> x-ray; mozilla members <-> ooffice and samba; first half <-> second half of
   osdb, nci, enwik8; 8 images <-> 9 images; 1 MiB chunks of one file <-> its other chunks): encode time through
   `tools/measure.py --runs 3`, size delta per file and in total, worst file;
4. repeat scenario (the nightly-backup case: same kind again): time against the oracle shares above;
5. poisoned store (flipped winners, random records, truncated file): output still decodes, sizes and times reported;
6. `-T 1` against `-T 4` with the same snapshot: identical frames.

### P2 (execute, claim `xp-plan-memo`): plan memo at the planner stage

**Design.** In `plan_worker`, before `plan_for`: look the member's type key up; when the last k = 2 or more
observations for the key agree on one plan and none contradicts, take that plan after a cheap fit check and skip
`make_plan` (and at `-x` `ultra_keep_transform`). Fit checks are the ones `kp_plan_pinned` already has (field count
of lines 1 to 32 for table plans, header bounds for image and graph plans) plus the container header re-read where
the plan depends on it (NPY dtype/shape, PNM geometry). Remembered: plan type and a, b, c, wide, verify, kgw and the
16-bit image fields. Not remembered, so re-derived or left to the normal analysis: record plans (per-file field
tables), formula sets of typed tables (`fset_discover` runs on the remembered column plan), codec-25 column samples
(`cmpz`). Every 8th use the full analysis runs anyway and is compared; a difference clears the key.
The trained-profile code is reused, not duplicated; unlike a profile, nothing marks the frame and nothing is needed
to decode.

**Files.** `src/plan/xp.c`, `src/plan/parallel.c`, `src/plan/profile.c` (shared fit checks made callable),
`src/plan/planner.c` (none or a hook only), tests.

**Expected gain and basis.** `balanced`: the planning share, 27.5% of encode on the 14 files of 2.3 (osdb 49%,
x-ray 55%, f32 NPY 67%), on kinds seen before. `-x` text: 9.7 to 13.7% on 5 to 10 MB and the figures of 2.5 on
1 MiB files. `ratio` text: 10 to 21% on files of 5 to 10 MB (2.2). Nothing on a kind seen for the first time.

**Risks.** A remembered "no transform" hides a transform that a new file of the same key would have had (size
regression, valid output): limited by the key (shape, class, band, mode), the fit checks, the agreement rule and the
re-verification cadence, and it is opt-in with P1's switch. Header-less binary data keys poorly (two raw images of
different geometry share a key): the fit check must reject, and a planned content sketch (P6) improves the key.

**Success is measured by** plan-identity rate on held-out files of the same kind (how often the memo's plan equals
the full analysis's), size delta, encode time (`measure.py`), on: enwik8 in 1 MiB and 10 MB pieces, the OWID CSV
cut at line ends into parts, the NPY set by dtype, the 17 images, Silesia text; and the same poisoned-store and
determinism checks as P1.

### P3 (planned, claim `xp-param-learn`): parameters outside today's search, learnt across files

LZMA2 literal/position context bits (lc, lp, pb: the payload already carries them and `lzma.c` already has an
encoder hook, level >= 100) and CM learning settings beyond the four presets and for streams above 16 MiB (two header
bytes, 65,536 settings; `docs/TWOPASS.md` 2a: a 49-coding search gains 0.6 to 5.6% with the prior but is too slow
per file). The store turns the per-file search into one extra slice coding per call ("explore one neighbour of the
current best on 256 KiB"), keeps a per-kind champion, and uses it when it has beaten the default on k files.
Measured basis for lc/lp/pb in 2.6. No format change; opt-in with the store.

### P4 (planned, claim `xp-target-calibration`): the store's timings feed `--target`

Tracker B12 wants a cost table per option and data type; `kolmo calibrate` fits one per machine on synthetic data.
The store already records coder time per arm and kind on the user's real data: export it as a calibration table
(`kolmo xp calibrate`), so `--target` predictions follow the machine and the data. Timings never influence bytes
outside `--target`, which is already clock-free at decision time (it reads a table).

### P5 (planned, claim `plan-dead-trials`): deterministic removal of planner work whose result is discarded

Independent of the store, for every file including the first: (a) `ratio`/`-x`: skip the word-transform trial when
the block coder will read the plain bytes; (b) `-x`: skip `ultra_keep_transform`'s two CM sample passes for a word
plan; (c) `compress_worker` at `ratio`: code each slice probe once (mr: the same 512 KiB slice is block-sorted twice
and LZMA2-coded twice). (c) is byte-identical. (a) and (b) leave the payload and the size unchanged but the
plan byte of such a block becomes 0 where rc3 writes 8 (word plan with tflag 0), so they are a change of bytes
written: behind a switch, default unchanged, and **flagged for the owner** as a candidate default (same size, same
decode, 10 to 21% faster `ratio` on 5 to 10 MB text, 9.7 to 13.7% faster `-x`, more on small files: 2.5).
What claim #9 (R07) left out: #9 removes the dead `word_encode` inside `compress_worker` (byte-identical); it does
not touch the planner's trial, `ultra_keep_transform`, or the duplicated slice probes.

### P6 (planned, claim `xp-content-sketch-key`): a learnt content sketch as the type key for header-less data

For binary data without a container header the key above is coarse. A 64-bucket sketch (byte-class histogram,
stride-autocorrelation peaks at 1, 2, 3, 4, 8, row candidates, order-0 entropy of two byte planes) quantised against
centroids that the store itself maintains (online k-means with a fixed cap, deterministic update at flush time)
gives "this looks like the 16-bit rasters I have seen" without any trained model shipped. Self-learning of formats
in the modest, checkable sense: the centroid id is just part of the key, so all of P1's safety properties hold.

## 5. Native or optional: how it will be decided

Baseline (`size /home/user/kolmo-review40-base/build/kolmo`): text 3,485,893, data 6,612, bss 86,140 bytes; the
file is 3,591,720 bytes; `kolmo.o` text 1,092,788. Rule I will apply to the built feature in stage 2:

- compiled in by default ("native") if all three hold: (1) `build/kolmo` text+data grows by at most 0.5%
  (17.5 KB); (2) with the store off, encode time over the determinism inputs at `-m speed` and `balanced` is within
  run-to-run noise of the rc3 binary (the added work is one predictable branch per member and per block);
  (3) with the store on, the fixed cost per call (snapshot read + one append) is under 1% of a 1 MiB `-m speed`
  encode and under 100 microseconds on a 4 KiB input;
- otherwise a build option (`make XP=1`), and in both cases `-DKOLMO_NO_XP` compiles it out.

"Native" means present in the binary; the behaviour stays **opt-in** either way (rule 3): turning it on by default
would make output depend on machine history, which `docs/MEMORY-MODEL.md` rules out for defaults ("different bytes
on different machines or at different moments ... would break the determinism check").

## 6. Published work consulted (ideas only; all code will be original)

- "Finite-time Analysis of the Multiarmed Bandit Problem" (Auer, Cesa-Bianchi, Fischer): confidence bounds for
  when an arm may stop being tried; the skip rule above is a conservative, deterministic relative of it.
- "Algorithm Selection as a Bandit Problem with Unbounded Losses" (Gagliolo, Schmidhuber): selection among
  algorithms whose cost is run time.
- "AdaEdge" (compressor selection as a multi-armed bandit on edge devices) and "LEA: A Learned Encoding Advisor for
  Column Stores": learned selection of encodings from data features.
- "OpenZL: A Graph-Based Model for Compression": explicit offline training per format, the trained compressor is
  then supplied to each compression; the contrast case (here the training step disappears and nothing is carried).
