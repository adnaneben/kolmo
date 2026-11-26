# Levels 8-9: guarded segment merge (review 40, top-10 item 5)

Change: `src/core/compress.c`. The equal-plan merge pass that levels 3-7 already run (neighbouring tar members and
content runs with the same plain or word plan are coded as one unit) now also runs at levels 8 and 9, with a guard:

- two neighbours merge when both are at most 8 MiB (R03's interim rule), or when a repeat probe finds at least 0.4%
  of the later segment's content-defined 32-byte anchors in the last 16 MiB of the earlier one (`seg_repeat_permille`;
  periodic anchors such as zero fill are ignored);
- at level 8 a merged unit stays at most 2 MiB: level 8 chooses block sorting or KLZMA for a larger block from a
  512 KiB slice of its middle, and that slice misjudged samba's two merged runs (KLZMA 2,719,103 B against
  2,537,128 B for the runs apart with block sorting, -8 +4.56% before the limit).

No format change: every frame is container version 12. Base: `origin/main` a1d39559. Every output of this branch in
the tables was decoded by the 0.0.2 binary (`/opt/homebrew/bin/kolmo`, 0.0.2) and checked by SHA-256 against its
input. Sizes only; the timing runs (`timing.sh`) wait for the machine's quiet window
(`evidence/QUIET_WINDOW.md`).

## Choosing the guard (-9, `-T 10`, sizes in bytes)

| rule | samba | mozilla | silesia.tar |
|---|---:|---:|---:|
| main (no merge at 8-9) | 3,945,893 | 13,915,668 | 40,507,982 |
| every equal plan merges (as at 3-7) | 3,887,666 (-1.48%) | 13,380,195 (-3.85%) | 41,014,330 (**+1.25%**) |
| both neighbours <= 8 MiB | 3,887,666 | 13,517,715 (-2.86%) | 40,507,907 |
| probe >= 0.4% | 3,887,666 | 13,609,618 (-2.20%) | 40,507,982 |
| probe >= 0.4%, or either neighbour <= 1 MiB | 3,887,666 | 13,392,722 (-3.76%) | 40,651,523 (**+0.35%**) |
| **probe >= 0.4%, or both <= 8 MiB (chosen)** | **3,887,666 (-1.48%)** | **13,380,195 (-3.85%)** | **40,507,907 (-75 B)** |

Probe values: every Silesia member pair 0 per mille, mozilla's large members 5-20, samba's two runs 23.
canterbury.tar, ooffice, osdb, xml and magika.onnx were identical under every rule.

## Coverage

- Corpus files: every file of `corpus/`, `corpus-holdout/`, `corpus-domain/` and `mkt/data/` (59 files, 1.1 GB).
  Levels 8-9: main and this branch at `-T 1`, this branch also at `-T 10`. Levels 1-7 cannot change (the merge
  only differs at mode max); they were checked frame for frame at `-T 10`.
- Real tars, one-shot: silesia.tar and canterbury.tar (above), big.tar (two Silesia copies, 424 MB), paxpipe.tar
  (bsdtar pax format, 364 MB), bigmember.tar (1.16 GB with a 988 MB member) and mix2g.tar (2.25 GB, 27,794 members:
  corpora, source trees and build output). All levels at `-T 10`, main and this branch; level 3 and level 9 also at
  `-T 1` for mix2g.tar.
- `-T 1` and `-T 10` frames of this branch were identical in every pair compared (137 of 137).
- `run.py` (the harness, phase `files`, plus `oneshot` of the tars phase) and `summarize.py` wrote these tables;
  `results.tsv` holds the one-shot rows. The harness's stream and archive phases measured work that moved to
  `claude/stream-tar-cuts-handoff` (issue #57) and are not part of this change.

## Results

rows: 1402; new outputs decoded by 0.0.2: 767; failures: 0
new -T 1 vs -T 10 frames compared: 137, different: 0

### Corpus files by kind, one-shot (levels 8-9 `-T 1`, levels 1-7 `-T 10`; sum of sizes, change, files changed / files)

| kind | -1 | -2 | -3 | -4 | -5 | -6 | -7 | -8 | -9 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| audio | = (16) | = (16) | = (16) | = (16) | = (16) | = (16) | = (16) | = (16) | = (16) |
| binary/archive | = (3) | = (3) | = (3) | = (3) | = (3) | = (3) | = (3) | -0.28% (1/3) | -2.97% (2/3) |
| fastq | = (4) | = (4) | = (4) | = (4) | = (4) | = (4) | = (4) | = (4) | = (4) |
| image | = (14) | = (14) | = (14) | = (14) | = (14) | = (14) | = (14) | = (14) | = (14) |
| log | = (1) | = (1) | = (1) | = (1) | = (1) | = (1) | = (1) | = (1) | = (1) |
| markup | = (2) | = (2) | = (2) | = (2) | = (2) | = (2) | = (2) | = (2) | = (2) |
| numeric/weights | = (7) | = (7) | = (7) | = (7) | = (7) | = (7) | = (7) | = (7) | = (7) |
| table/db | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) |
| tar | = (2) | = (2) | = (2) | = (2) | = (2) | = (2) | = (2) | -0.00% (1/2) | -0.00% (1/2) |
| text | = (5) | = (5) | = (5) | = (5) | = (5) | = (5) | = (5) | = (5) | = (5) |
| video | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) | = (6) |

### Corpus files, one-shot: per file (only files where some level changed)

-T 1 (change of new against base; `=` identical frame):

| input | mode | -1 | -2 | -3 | -4 | -5 | -6 | -7 | -8 | -9 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| mozilla | oneshot |  |  |  |  |  |  |  | -0.40% | -3.85% |
| samba | oneshot |  |  |  |  |  |  |  | = | -1.48% |
| silesia.tar | oneshot |  |  |  |  |  |  |  | -0.00% | -0.00% |

-T 10 (change of new against base; `=` identical frame):

| input | mode | -1 | -2 | -3 | -4 | -5 | -6 | -7 | -8 | -9 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| mozilla | oneshot | = | = | = | = | = | = | = | -0.40% | -3.85% |
| samba | oneshot | = | = | = | = | = | = | = | = | -1.48% |
| silesia.tar | oneshot | = | = | = | = | = | = | = | -0.00% | -0.00% |


### Real tars, one-shot (sizes change only at levels 8-9)

-T 1 (change of new against base; `=` identical frame):

| input | mode | -1 | -2 | -3 | -4 | -5 | -6 | -7 | -8 | -9 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| mix2g.tar | oneshot |  |  | = |  |  |  |  |  | -1.71% |

-T 10 (change of new against base; `=` identical frame):

| input | mode | -1 | -2 | -3 | -4 | -5 | -6 | -7 | -8 | -9 |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| big.tar | oneshot | = | = | = | = | = | = | = | -0.00% | -0.00% |
| mix2g.tar | oneshot | = | = | = | = | = | = | = | -0.11% | -1.71% |
| paxpipe.tar | oneshot | = | = | = | = | = | = | = | = | -0.00% |


### Absolute sizes (bytes, -T 1 unless noted)

| input | mode | level | base | new | change |
|---|---|---:|---:|---:|---:|
| mozilla | oneshot | -8 | 14,447,505 | 14,390,162 | -0.40% |
| mozilla | oneshot | -9 | 13,915,668 | 13,380,195 | -3.85% |
| samba | oneshot | -9 | 3,945,893 | 3,887,666 | -1.48% |
| silesia.tar | oneshot | -8 | 42,006,681 | 42,006,588 | -0.00% |
| silesia.tar | oneshot | -9 | 40,507,982 | 40,507,907 | -0.00% |
| big.tar | oneshot T10 | -8 | 84,013,558 | 84,013,439 | -0.00% |
| big.tar | oneshot T10 | -9 | 81,016,793 | 81,016,613 | -0.00% |
| mix2g.tar | oneshot T10 | -8 | 656,745,621 | 656,028,838 | -0.11% |
| mix2g.tar | oneshot | -9 | 646,767,942 | 635,691,509 | -1.71% |
| mix2g.tar | oneshot T10 | -9 | 646,767,942 | 635,691,509 | -1.71% |
| paxpipe.tar | oneshot T10 | -9 | 117,740,108 | 117,740,037 | -0.00% |
