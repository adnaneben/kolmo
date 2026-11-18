# rc3 wave 2, twopass lane: reading the whole file first

Branch `claude/rc3w2-twopass-20261117` (from `claude/release-rc3-20261116` at `1536036`). Question and recommendation:
`docs/TWOPASS.md`. Raw outputs: `raw/`. Tools: the `.c`, `.py` and `.sh` files here (`build.sh` builds the C ones
against `make`'s objects).

## Machine and method

- Cloud VM: Intel Xeon, 4 vCPU, 15.7 GiB, Linux 6.18, gcc 13, `make` defaults (`-O3 -march=native`). "All threads" is
  4 here; every row says 1 or 4 threads, never mixed (`benchmark/rc2/PROTOCOL.md`).
- Study runs (sizes) were run up to four at a time; their times are indicative only. End-to-end runs
  (`bench.py`, `queue.sh`) are strictly one process at a time: wall time and peak memory from `wait4`, encode and
  decode separately, the decoded file compared byte for byte.
- Inputs (`*_100k`, `*_1m`, `*_10m`, `*_100m`: the first N bytes, cut at a line end for text, CSV and logs):
  - text: enwik8 (Wikipedia XML);
  - log: Loghub HDFS (`HDFS.log`, the first 120 MB);
  - table: UCI household electric power consumption (CSV, `;`, 9 columns);
  - bin: Silesia mozilla, samba, ooffice, sao, osdb, mr concatenated (the 100k–10m files are all mozilla);
  - num: the seven numeric columns of the power data as little-endian float64 rows (missing values NaN);
  - image: Silesia x-ray (12-bit greyscale in 16 bits), `image_8m` is the whole file (there is no 100 MB image);
  - mixed: a quarter each of text, bin, table and num, in that order (`segments.py` rebuilds and checks the parts).
- Training data for the shared priors (not in any test file): 1 MB slices of Silesia dickens, webster, xml, nci,
  reymont and Loghub Hadoop logs (model 3); of `/usr/bin/pandoc`, `/usr/bin/dockerd`, libLLVM, libicudata and
  libgallium from the VM's Ubuntu 24.04 (model 2).

## Q1. Codec and transform choice: full-file trials against sampling

`planoracle FILE MODE` (`raw/oracle.txt`): the mode's own frame, then every distinct plan from the analyses of all
five modes (and their quick variants), the same analyses with every trial slice set to the whole file
(`KOLMO_PLAN_HOOK`, research builds only), and no transform, each coded by the mode's real coders on the whole file.

- **101 file × mode cases: the full-file analysis picked the same plan as the sampled one in every case.** Planning
  time per 100 MB: sampled ≤ 0.09 s, full 0.6–7.7 s.
- Where another plan was smaller, it was another mode's plan (a level trade-off, not a sampling error):

| file | mode | best other plan | smaller by | cost |
|---|---|---|---|---|
| num 100k / 1m / 10m | speed | balanced's (graph) | 53% / 51% / 50% | decode ≈ 4× slower (still ≥ 250 MB/s) |
| image 100k / 1m / 8m | speed | balanced's (16-bit image) | 38% / 32% / 32% | same |
| text 1m / 10m / 100m | speed | hc's (word transform) | 12% / 12% / 16% | encode 3–4× slower |
| text 1m / 10m / 100m | balanced | hc's (word transform) | 1.2% / 1.0% / 4.8% | encode 3.5× slower (0.58 → 2.0 s per 100 MB, 1 thread) |
| mixed 10m | speed / balanced | hc's | 3.7% / 0.4% | (the segment result below is larger) |
| **num 10m** | **x** | **no transform (plain CM)** | **32.9%** | the graph was chosen by a sampled BWT proxy: fixed (below) |

**Heterogeneous files** (`segments.py`, `raw/segments.txt`): the mixed file coded whole against its four parts coded
separately (each part analysed on its own):

| file | speed | balanced | hc | ratio | `-x` |
|---|---:|---:|---:|---:|---:|
| mixed 1m | +0.02% | −12.9% | −8.5% | −7.7% | +0.19% |
| mixed 10m | +0.04% | −10.3% | −6.1% | −4.7% | — |

Implemented as content segments (`src/plan/segment.c`) for balanced, eco, hc and ratio: windows classed as text,
binary or a delimited table (its delimiter and field count). Mixed 10m: balanced −10.0%, hc −5.4%, ratio −4.2%; mixed
100m −13.3% / −7.1% / −12.7% (end to end, below; the cuts fall on 64 KiB windows, and binary and doubles are one class,
so the parts are not exactly the quarters). Every homogeneous 10 MB and 100 MB file at balanced, hc and ratio:
byte-identical frames. Cost: 32 probes of 8 KiB for one-kind files;
balanced on 100 MB of text 0.237–0.247 s before, 0.249–0.251 s after (4 threads; a first version that scanned every
byte cost 0.16 s and was replaced).

## Q2. Per-file tuning of the context-mixing networks

Codec 7 alone on the raw bytes (model 3 for text, logs and tables, model 2 otherwise).

**Full search** (`cmtune search`, coordinate descent over 8 fields × 4 values, 2 passes, 49 codings of the whole
file; `raw/fullsearch_small.txt` without the prior, `raw/fullsearch_prior.txt` starting from the prior):

| file | default | full search | gain | full search + prior | gain |
|---|---:|---:|---:|---:|---:|
| text 100k | 26,732 | 26,631 | 0.38% | 26,280 | 1.69% |
| log 100k | 4,699 | 4,651 | 1.02% | 4,533 | 3.53% |
| table 100k | 6,552 | 6,466 | 1.31% | 6,340 | 3.24% |
| bin 100k | 14,224 | 14,188 | 0.25% | 13,666 | 3.92% |
| num 100k | 11,065 | 10,926 | 1.26% | 10,447 | 5.59% |
| image 100k | 47,164 | 47,078 | 0.18% | 46,574 | 1.25% |
| text 1m | 219,908 | — | — | 218,575 | 0.61% |
| log 1m | 30,081 | 29,824 | 0.85% | 29,607 | 1.58% |
| bin 1m | 622,610 | 622,372 | 0.04% | — | — |
| num 1m | 71,861 | 70,998 | 1.20% | — | — |
| image 1m | 423,078 | 422,630 | 0.11% | — | — |

(— : not run; a 1 MB full search takes 160–430 s.) Table size: one or two bits more or fewer than one bucket per byte
changed sizes by under 0.05% (`cmbits`).

**Race** (`cmrace race`, `raw/race.txt` with 9 presets, `raw/race4.txt` with the 4 shipped presets): the presets
code a prefix, the winner codes the whole file.

| file (1 MB) | default | prior only | race 4 on 256 KiB | best of the 4 on the whole file | full search + prior |
|---|---:|---:|---:|---:|---:|
| text | 219,908 | 219,170 | 218,573 (0.61%) | 218,573 | 218,575 |
| log | 30,081 | 29,955 | 29,666 (1.38%) | 29,652 | 29,607 |
| table | 51,588 | 51,507 | 50,796 (1.54%) | 50,796 | — |
| bin | 622,610 | 621,777 | 621,777 (0.13%) | 621,777 | — |
| num | 71,861 | 71,096 | 70,317 (2.15%) | 70,142 | — |
| image | 423,078 | 422,313 | 422,022 (0.25%) | 422,022 | — |

A 128 KiB prefix picked a worse preset on text (0.46%) and bin (0.03%); 256 KiB was kept. With 9 presets the
256 KiB race on bin chose one that lost 0.02% on the whole file; the shipped set keeps the prior-only preset, which
never loses.

## Q3. Weight initialisation

`cmwinit FILE MODEL PRIOR` (`raw/winit.txt`): two-pass means (8-bit with 315 header bytes, 16-bit with 630), and a
prior trained on 2 MB of the same kind of data (the per-kind trial that preceded the general prior):

| file | two-pass 8-bit | two-pass 16-bit | per-kind prior | general prior (shipped) |
|---|---:|---:|---:|---:|
| text 100k | +0.06% | +1.25% | −1.25% | −1.13% |
| log 100k | +4.90% | +11.6% | 0.00% | −2.64% |
| table 100k | +4.06% | +8.84% | +1.27% | −1.40% |
| bin 100k | −0.26% | +1.90% | −3.60% | −3.54% |
| num 100k | −0.45% | +2.39% | −3.96% | −3.91% |
| image 100k | −0.78% | −0.12% | −1.37% | −1.12% |
| text 1m | −0.24% | −0.09% | −0.37% | −0.34% |
| log 1m | +0.95% | +2.00% | +0.11% | −0.42% |
| table 1m | +0.66% | +1.27% | +0.40% | −0.16% |
| bin 1m | −0.09% | −0.04% | −0.15% | −0.13% |
| num 1m | −0.68% | −0.26% | −1.07% | −1.06% |
| image 1m | −0.19% | −0.12% | −0.22% | −0.18% |

The per-kind log prior (trained on later HDFS lines) was a worse start for logs than the text prior (−2.8% with the
dickens prior); one prior per model, trained on varied data, is what ships.

## Q4. Semi-static models

`semistatic FILE` (`raw/semistatic.txt`): whole-file counts per context, tables stored (zstd -19 of the frequency
arrays, counted), static rANS, against zstd -3 and -19 on the same bytes, 1 thread:

| file | zstd -3 | zstd -19 | order 0 | order 1 | order 2 (hashed, 4096 contexts) | o1 enc / dec MB/s |
|---|---:|---:|---:|---:|---:|---|
| text 10m | 3,565,691 | 2,794,530 | 6,398,635 | 4,862,517 | 4,114,536 | 78 / 84 |
| log 10m | 891,080 | 588,959 | 6,516,967 | 3,340,988 | 1,825,450 | 103 / 83 |
| table 10m | 1,767,413 | 1,116,667 | 4,042,360 | 3,097,064 | 2,189,652 | 120 / 122 |
| bin 10m | 5,148,045 | 4,767,186 | 8,575,383 | 7,177,872 | 7,189,136 | 63 / 72 |
| num 10m | 1,559,733 | 1,175,023 | 5,610,327 | 1,754,414 | 1,521,018 | 73 / 89 |
| mixed 10m | 3,672,167 | 3,231,116 | 7,933,628 | 5,261,446 | 4,885,303 | 72 / 78 |

zstd -3 decodes these at 700–1,100 MB/s. Only order 2 on num comes near zstd -3 (−2.5%), at a twelfth of its decode
speed; Kolmo's balanced codes num 10m to 1,191,001 bytes anyway. Not a tier.

## End to end

`queue.sh` with `bench.py` (`e2e.csv`; all rows as a table in `e2e.md`, produced by `summarize.py`). `base` = rc3 base
`1536036`, `new` = this branch. One process at a time; MB/s are of the input; memory is peak resident (MiB).

### `-x`

| file | T | base bytes | new bytes | change | encode MB/s base → new | decode MB/s base → new |
|---|---|---:|---:|---:|---|---|
| text 100k | 1 / 4 | 26,784 | 26,360 | −1.58% | 0.21 → 0.06 / 0.21 → 0.18 | 0.26 → 0.25 |
| log 100k | 1 / 4 | 4,751 | 4,574 | −3.73% | 0.13 → 0.05 / 0.15 → 0.12 | 0.30 → 0.29 |
| table 100k | 1 / 4 | 6,604 | 6,393 | −3.20% | 0.10 → 0.02 / 0.17 → 0.06 | 0.28 → 0.31 |
| bin 100k | 1 / 4 | 14,276 | 13,756 | −3.64% | 0.37 → 0.10 / 0.42 → 0.29 | 0.45 → 0.38 |
| num 100k | 1 / 4 | 11,117 | 10,519 | −5.38% | 0.11 → 0.05 / 0.20 → 0.14 | 0.37 → 0.36 |
| image 100k | 1 / 4 | 45,822 | 45,822 | 0 | (codec 12 graph block; no CM stream) | — |
| text 1m | 1 / 4 | 219,960 | 218,625 | −0.61% | 0.12 → 0.07 / 0.13 → 0.12 | 0.27 → 0.27 |
| log 1m | 1 / 4 | 30,133 | 29,718 | −1.38% | 0.17 → 0.11 / 0.18 → 0.16 | 0.29 → 0.29 |
| table 1m | 1 / 4 | 51,395 | 50,367 | −2.00% | 0.16 → 0.08 / 0.35 → 0.17 | 0.53 → 0.46 / 1.49 → 1.24 |
| bin 1m | 1 / 4 | 622,662 | 621,829 | −0.13% | 0.26 → 0.14 / 0.32 → 0.27 | 0.27 → 0.29 |
| num 1m | 1 / 4 | 71,913 | 70,369 | −2.15% | 0.42 → 0.17 / 0.41 → 0.25 | 0.44 → 0.43 |
| image 1m | 1 / 4 | 420,190 | 419,874 | −0.08% | 0.34 → 0.16 / 0.41 → 0.32 | 0.38 → 0.38 |
| mixed 1m | 1 / 4 | 195,881 | 194,737 | −0.58% | 0.35 → 0.16 / 0.40 → 0.27 | 0.36 → 0.35 |
| text 10m | 1 / 4 | 2,012,690 | 2,006,748 | −0.30% | 0.18 → 0.16 / 0.18 → 0.18 | 0.22 → 0.23 |
| log 10m | 4 | 250,740 | 248,837 | −0.76% | 0.27 → 0.26 | 0.32 → 0.33 |
| table 10m | 4 | 449,066 | 445,942 | −0.70% | 0.94 → 0.47 | 1.72 → 1.61 |
| bin 10m | 4 | 4,333,900 | 4,328,826 | −0.12% | 0.40 → 0.36 | 0.43 → 0.41 |
| num 10m | 1 / 4 | 875,955 | 581,849 | **−33.58%** | 7.26 → 0.34 / 9.42 → 0.38 | **169 → 0.42 / 179 → 0.46** |
| image 8m | 4 | 3,560,794 | 3,560,232 | −0.02% | 0.46 → 0.44 | 0.45 → 0.47 |
| mixed 10m | 4 | 2,696,386 | 2,695,964 | −0.02% | 0.35 → 0.32 | 0.35 → 0.35 |
| text 100m | 4 | 18,681,140 | 18,677,629 | −0.02% | 0.19 → 0.20 | 0.22 → 0.21 |
| num 100m | 4 | 5,588,523 | 5,587,799 | −0.01% | 0.41 → 0.40 | 0.44 → 0.43 |

- Every frame with a tuned stream is version 11 (was 3).
- Encode: the race costs up to 4 codings of 256 KiB (100 KB files: 4 codings of the whole file), so at 1 thread `-x`
  encodes 100 KB–1 MB files 2–4× slower; with 4 threads 1.1–1.4×. At 10 MB it is 1–12% (table 10m: 2×, because
  every column of codec 25 races on one thread). Decode speed is unchanged within noise, except num 10m (below).
- Memory: with 4 threads the race holds up to four models of the sample at once: 100 KB files 280 → 1,100 MiB
  encode peak, 1 MB files +0–300 MiB; decode memory unchanged.
- num 10m: the block is now plain context mixing instead of a numeric graph, 33.6% smaller and decoded at `-x`'s
  CM speed (0.4 MB/s instead of 170 MB/s; decode memory 4.5 GiB). That is the trade `-x` makes for every other kind
  of data; the graph block was larger than `ratio`'s own output (743,054 bytes).
- Above 16 MiB a stream is not raced (the prior with the default rates): the first run raced num 100m and the 256 KiB
  winner (`520401`) was 1.17% *larger* than base over the 100 MB stream (5,654,143 bytes); text 100m −0.09% with the
  race, −0.02% without. 1–10 MB streams gained from the race in every case measured.

### balanced, hc, ratio

Every homogeneous 100 MB file (text, log, table, bin, num) at balanced, hc and ratio with 4 threads: byte-identical
frames to base (bin 100m starts with a tar archive, so tar members already form its segments). Encode and decode times
on those rows are equal within noise (`e2e.md`; base and new alternate per file, after a warm-up read of the file).

| file | mode | T | base bytes | new bytes | change | encode MB/s base → new | decode MB/s base → new |
|---|---|---|---:|---:|---:|---|---|
| mixed 1m | all | 1 / 4 | | | 0 (under 2 MiB: one segment) | | |
| mixed 10m | balanced | 1 / 4 | 3,681,900 | 3,313,538 | −10.00% | 294 → 135 / 500 → 200 | 833 → 588 / 1429 → 1250 |
| mixed 10m | hc | 1 / 4 | 3,231,168 | 3,058,088 | −5.36% | 3.2 → 7.2 / 3.2 → 8.8 | 625 → 323 / 500 → 1000 |
| mixed 10m | ratio | 1 / 4 | 3,019,636 | 2,892,520 | −4.21% | 1.6 → 3.7 / 1.7 → 7.6 | 17 → 42 / 31 → 68 |
| mixed 100m | balanced | 1 / 4 | 26,577,493 | 23,037,706 | −13.32% | 69 → 172 / 855 → 498 | 505 → 641 / 2500 → 2000 |
| mixed 100m | hc | 1 / 4 | 20,418,469 | 18,977,873 | −7.06% | 1.8 → 3.6 / 1.8 → 5.3 | 571 → 435 / 441 → 952 |
| mixed 100m | ratio | 1 / 4 | 18,712,010 | 16,339,746 | −12.68% | 1.1 → 2.8 / 1.1 → 5.5 | 128 → 46 / 115 → 84 |

On mixed files hc and ratio encode 2–5× *faster* (each part gets the cheaper plan that suits it); balanced encodes
slower at 4 threads (the CSV and float parts now take their transforms) and ratio decodes slower (its parts now use
block sorting and graph leaves where they are smaller). A first segmenter with a numeric-array class split the
homogeneous num 100m into 7 segments (+1.86% at ratio, +0.25% at hc); that class was removed before these rows.

## Tests

On the branch head: `make check`, `make check-rc`, the determinism check (122 frames, 1 and 4 threads; the 17 changed
`-x` frames are regenerated in `87a4258`). `make check-sanitizers`, `make kfam-check` and
`make experimental-check GPU_CHECK=` passed on `a45247c` (the head differs from it only in the calibration output
format).

## What failed or was not done

- A model-2 tuned stream under 64 KiB was written with legacy logistic tables but a header that says exact: caught by
  `make check` (`ultra-l2-int-series`), fixed before any measurement here.
- `make check-rc` (`check-profile`) failed after the race: `kolmo calibrate` wrote `-x`'s rate on its 16 KiB samples
  with one decimal, now under 0.05 MB/s, as `0.0`, which the table parser refuses. Rates are now written with four
  significant digits (`a305807`).
- The race on long streams (above) and the numeric segment class (above) were both measured, found to lose, and
  restricted or removed.
- Not done: tuning GR6+ (codec 26) and the image model (codec 17), whose networks have their own settings; per-segment
  plans at `-x` (measured to lose 0.2%); 100 MB `-x` rows for log, table, bin and image (each 25–30 minutes per binary);
  Apple M5 runs (integration lane).

## Commands

```
make && benchmark/rc3w2/twopass/build.sh cmtune cmrace cmwinit cmbits planoracle
gcc -O3 -march=native -Ivendor/zstd benchmark/rc3w2/twopass/semistatic.c build/libvendor.a -o build/semistatic -lpthread
./build/planoracle FILE MODE 1                 # Q1, also run_oracle.sh
python3 -I benchmark/rc3w2/twopass/segments.py build/kolmo MIXED_FILE speed balanced hc ratio x
./build/cmtune search FILE MODEL 0 2 [1]       # Q2 full search (1: from the prior)
./build/cmrace train 3 prior3.inc FILES...     # the shipped priors (src/codecs/cm_prior.inc)
./build/cmrace race FILE MODEL 262144 - 000001 1ca501 520401 dcb401   # also run_race.sh, run_race4.sh
./build/cmwinit FILE MODEL PRIOR_FILE          # Q3, also run_winit.sh
./build/semistatic FILE                        # Q4
benchmark/rc3w2/twopass/queue.sh; python3 -I benchmark/rc3w2/twopass/summarize.py benchmark/rc3w2/twopass/e2e.csv
```
