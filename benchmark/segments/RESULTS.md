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
input. Timing: see *Encode time and memory* at the end.

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

## Encode time and memory

`timing.sh` through `tools/timed.sh` in the quiet window (`evidence/QUIET_WINDOW.md`). Compression only, two runs per cell
(the faster wall time and the larger peak RSS are shown), 1-minute load 3.64-9.01 for every row (the limit is 12).
main is a1d39559 (`kolmo-base`); #117 is this branch's merge alone (`kolmo-merge`). Raw rows: `timing.tsv`.

| input | threads | main wall s | #117 wall s | change | main peak RSS MiB | #117 peak RSS MiB |
|---|---:|---:|---:|---:|---:|---:|
| mozilla -9 | 1 | 10.86 | 12.89 | +19% | 517 | 591 |
| mozilla -9 | 10 | 2.05 | 5.10 | +149% | 898 | 663 |
| samba -9 | 1 | 3.43 | 4.22 | +23% | 207 | 331 |
| samba -9 | 10 | 0.84 | 1.47 | +75% | 381 | 483 |
| silesia.tar -9 | 1 | 25.78 | 24.10 | -7% | 1,472 | 1,471 |
| silesia.tar -9 | 10 | 6.58 | 6.78 | +3% | 2,217 | 2,225 |
| mozilla -8 | 1 | 4.82 | 4.68 | -3% | 354 | 345 |
| mozilla -8 | 10 | 1.17 | 1.23 | +5% | 695 | 717 |

- **Where nothing merges, nothing changes.** silesia.tar -9 runs the probe on every equal-plan pair and merges nothing.
  Its times are within run-to-run noise (-7% at `-T 1`, +3% at `-T 10`), so the probe itself is cheap. mozilla -8
  merges only up to 2 MiB and is within noise too.
- **Where the merge fires, encoding is slower, most of all with many threads.** mozilla -9 goes from 33 blocks to one
  51 MB block. That block is coded by one thread, so `-T 10` loses its parallelism: 2.05 to 5.10 s (+149%), and
  +19% at `-T 1`. samba -9 goes from two runs to one block: +23% at `-T 1`, +75% at `-T 10`, and peak RSS +124 MiB
  at `-T 1`.
- Decode time was not measured. Fewer, larger blocks also decode with less parallelism, so expect a similar
  direction on the merged files.
- This is the trade for -3.85% (mozilla) and -1.48% (samba) at level 9. Whether `-T 10` encode time on such files
  matters more than the ratio is the owner's call.

## Mitigating the encode-time cost (sizes now; timing queued)

Variants of the level 9 rule, all thread-independent: a merge is joined only while the merged unit stays within a
cap. The research build (`variants.patch`: `XCAP=<MiB>`, `XCAPMAX=<MiB>`) wrote these frames at `-T 2`; they do not
depend on `-T`. Gain = bytes saved against main at -9.

| rule | mozilla | samba | mix2g.tar | mozilla + samba kept | largest block mozilla / samba |
|---|---:|---:|---:|---:|---:|
| main | 13,915,668 | 3,945,893 | 646,767,942 | 0% | 10.3 / 5.4 MB |
| no cap (#117 as measured above) | 13,380,195 (-3.85%) | 3,887,666 (-1.48%) | 635,691,509 (-1.71%) | 100% | 51.2 / 10.1 MB |
| cap 32 MiB | 13,511,651 (-2.90%) | 3,887,666 | 635,746,644 (-1.71%) | 78% | 29.3 / 10.1 MB |
| cap 16 MiB | 13,629,452 (-2.06%) | 3,887,666 | 639,305,505 (-1.15%) | 58% | 15.1 / 10.1 MB |
| cap 12 MiB | 13,690,333 (-1.62%) | 3,887,666 | 638,107,686 (-1.34%) | 48% | 11.9 / 10.1 MB |
| cap = largest unmerged segment of the input | 13,763,100 (-1.10%) | 3,945,893 (0) | 635,691,509 (-1.71%) | 26% | 10.3 / 5.4 MB |
| repeat share >= 1.0% or 1.5% instead of 0.4% | = no cap | = no cap | | 100% | 51.2 / 10.1 MB |

- A higher probe threshold changes nothing: mozilla's large members join through both rules, and the chain still
  ends in one block.
- `-T 10` time follows the largest block, because one block is coded by one thread. Only "cap = largest unmerged
  segment" keeps that block unchanged. It keeps all of mix2g's gain but 26% of mozilla's and none of samba's.
  samba's gain needs its two runs in one 10.1 MB block (main's largest is 5.4 MB), so every cap that keeps it pays
  for that block at `-T 10` (+75% measured).
- `timing2.sh` (queued for the quiet window) measures encode and decode at `-T 1` and `-T 10` for main and every
  variant above. The choice waits for it.

