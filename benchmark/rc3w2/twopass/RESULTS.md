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

Implemented as content segments (`src/plan/segment.c`) for balanced, eco, hc and ratio. Mixed 10m with the
segmenter, 4 threads: balanced 3,681,900 → 3,313,538 (−10.0%), hc 3,231,168 → 3,058,088 (−5.4%), ratio 3,019,636 →
2,892,520 (−4.2%) (the cut points fall on 64 KiB windows, so the parts are not exactly the quarters). Every
homogeneous 10 MB file at balanced, hc and ratio: byte-identical frames. Cost: 32 probes of 8 KiB for one-kind files;
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
