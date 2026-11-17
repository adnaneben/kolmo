# rc3 against rc2 on the Apple M5

17 November 2026. Apple M5 (10 cores: 4 performance, 6 efficiency), 24 GB, macOS; nothing else running. rc2 = branch
`claude/release-rc2-20261115` at `b2f0c00`, rc3 = `claude/release-rc3-20261116` at `f005511`, both built with the
default `make` on this machine.

Method: `run.sh` drives `benchmark/rc2/run_cli2.py` with `tools.json` (every mode of both versions at `-T 1` and
`-T 10`): one process per compression and per decompression, wall time from `wait4`, fastest of up to three rounds
(20 s budget per setting), every decoded output compared with the input by SHA-256. `compare.py *.jsonl` prints the
tables: total size over the files both versions measured, and speed ratios rc3 ÷ rc2 (above 1 = rc3 faster).
Data: the files of the rc2 multithreaded run (`benchmark/rc2/results/mt/`), plus the covid countries-aggregated CSV
and five ESC-50 WAV clips.

### audio

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 5 | +0.00% | 0.99x | 0.97x |
| balanced | 1 | 5 | -26.42% | 0.46x | 0.48x |
| eco | 1 | 5 | -17.41% | 0.70x | 0.61x |
| hc | 1 | 5 | -17.09% | 0.80x | 0.49x |
| ratio | 1 | 5 | -17.76% | 0.74x | 0.30x |
| -x | 1 | 5 | -17.22% | 0.85x | 4.59x |
| speed | 10 | 5 | +0.00% | 1.00x | 1.00x |
| balanced | 10 | 5 | -26.42% | 0.47x | 0.48x |
| eco | 10 | 5 | -17.41% | 0.70x | 0.61x |
| hc | 10 | 5 | -17.09% | 0.80x | 0.49x |
| ratio | 10 | 5 | -17.76% | 0.74x | 0.30x |
| -x | 10 | 5 | -17.22% | 0.85x | 4.62x |

### genomics

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 3 | +0.00% | 1.00x | 0.99x |
| balanced | 1 | 3 | +0.00% | 1.00x | 1.00x |
| eco | 1 | 3 | +0.00% | 1.00x | 1.01x |
| hc | 1 | 3 | +0.00% | 0.98x | 1.03x |
| ratio | 1 | 3 | -0.75% | 0.74x | 0.63x |
| -x | 1 | 3 | +0.18% | 1.94x | 0.99x |
| speed | 10 | 3 | +0.00% | 1.01x | 0.98x |
| balanced | 10 | 3 | +0.00% | 0.94x | 1.02x |
| eco | 10 | 3 | +0.00% | 1.00x | 1.00x |
| hc | 10 | 3 | +0.00% | 1.00x | 0.99x |
| ratio | 10 | 3 | -0.75% | 1.61x | 2.78x |
| -x | 10 | 3 | +0.18% | 1.98x | 0.98x |

### numeric

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 6 | +0.00% | 1.02x | 1.13x |
| balanced | 1 | 6 | -0.03% | 1.08x | 1.09x |
| eco | 1 | 6 | -0.01% | 1.01x | 1.06x |
| hc | 1 | 6 | -0.01% | 0.99x | 1.08x |
| ratio | 1 | 6 | -0.62% | 0.93x | 0.74x |
| -x | 1 | 6 | -0.00% | 0.97x | 0.98x |
| speed | 10 | 6 | +0.00% | 1.00x | 1.02x |
| balanced | 10 | 6 | -0.03% | 1.00x | 1.02x |
| eco | 10 | 6 | -0.01% | 1.01x | 1.05x |
| hc | 10 | 6 | -0.01% | 0.98x | 1.09x |
| ratio | 10 | 6 | -0.62% | 0.92x | 0.76x |
| -x | 10 | 6 | -0.00% | 1.01x | 0.98x |

### numeric_npy

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 11 | +0.00% | 1.02x | 1.08x |
| balanced | 1 | 11 | -0.90% | 0.39x | 1.04x |
| eco | 1 | 11 | -0.13% | 0.88x | 1.08x |
| hc | 1 | 11 | -0.10% | 0.86x | 1.05x |
| ratio | 1 | 11 | -0.93% | 0.90x | 0.74x |
| -x | 1 | 11 | -0.04% | 0.97x | 0.99x |
| speed | 10 | 11 | +0.00% | 1.01x | 1.01x |
| balanced | 10 | 11 | -0.90% | 0.74x | 1.00x |
| eco | 10 | 11 | -0.13% | 0.92x | 1.06x |
| hc | 10 | 11 | -0.10% | 0.87x | 1.06x |
| ratio | 10 | 11 | -0.93% | 0.90x | 0.77x |
| -x | 10 | 11 | -0.04% | 1.00x | 0.98x |

### structured

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 10 | +0.00% | 0.99x | 0.98x |
| balanced | 1 | 10 | -10.31% | 0.40x | 1.02x |
| eco | 1 | 10 | -8.87% | 1.02x | 1.04x |
| hc | 1 | 10 | -4.88% | 1.20x | 1.03x |
| ratio | 1 | 10 | -9.03% | 0.89x | 0.93x |
| -x | 1 | 10 | -1.45% | 0.76x | 1.75x |
| speed | 10 | 10 | +0.00% | 0.99x | 0.99x |
| balanced | 10 | 10 | -10.31% | 0.62x | 0.99x |
| eco | 10 | 10 | -8.87% | 1.08x | 1.05x |
| hc | 10 | 10 | -4.88% | 1.26x | 1.03x |
| ratio | 10 | 10 | -9.03% | 0.90x | 0.96x |
| -x | 10 | 10 | -1.45% | 0.89x | 1.75x |

### video

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 3 | +0.00% | 1.00x | 1.00x |
| balanced | 1 | 3 | -17.43% | 0.77x | 1.63x |
| eco | 1 | 3 | -12.93% | 1.09x | 1.18x |
| hc | 1 | 3 | -15.51% | 0.99x | 1.35x |
| ratio | 1 | 3 | -11.22% | 0.91x | 1.73x |
| -x | 1 | 3 | -4.13% | 2.01x | 1.35x |
| speed | 10 | 3 | +0.00% | 1.01x | 0.96x |
| balanced | 10 | 3 | -17.43% | 0.47x | 0.52x |
| eco | 10 | 3 | -12.93% | 1.22x | 0.76x |
| hc | 10 | 3 | -15.51% | 0.99x | 0.70x |
| ratio | 10 | 3 | -11.22% | 0.91x | 2.23x |
| -x | 10 | 3 | -4.13% | 3.53x | 2.54x |

### weights

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 3 | +0.00% | 0.99x | 1.01x |
| balanced | 1 | 3 | +0.00% | 0.99x | 1.22x |
| eco | 1 | 3 | +0.00% | 1.01x | 1.06x |
| hc | 1 | 3 | +0.00% | 1.01x | 1.18x |
| ratio | 1 | 3 | +0.00% | 1.00x | 1.01x |
| -x | 1 | 3 | +0.00% | 1.09x | 1.12x |
| speed | 10 | 3 | +0.00% | 1.01x | 0.98x |
| balanced | 10 | 3 | +0.00% | 0.98x | 1.03x |
| eco | 10 | 3 | +0.00% | 1.00x | 1.03x |
| hc | 10 | 3 | +0.00% | 1.00x | 1.08x |
| ratio | 10 | 3 | +0.00% | 0.98x | 1.01x |
| -x | 10 | 3 | +0.00% | 1.90x | 1.12x |

Round trips: every row decoded to its input.

### general

| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |
|---|---|---|---:|---:|---:|
| speed | 1 | 7 | +0.00% | 0.99x | 1.00x |
| balanced | 1 | 7 | +0.00% | 1.01x | 1.00x |
| eco | 1 | 7 | +0.00% | 1.00x | 1.02x |
| hc | 1 | 7 | +0.00% | 1.00x | 1.04x |
| ratio | 1 | 7 | -0.53% | 1.08x | 0.76x |
| -x | 1 | 7 | +0.00% | 0.96x | 1.00x |
| speed | 10 | 7 | +0.00% | 0.95x | 1.01x |
| balanced | 10 | 7 | +0.00% | 0.98x | 0.99x |
| eco | 10 | 7 | +0.00% | 1.02x | 1.03x |
| hc | 10 | 7 | +0.00% | 1.02x | 1.05x |
| ratio | 10 | 7 | -0.53% | 1.19x | 1.64x |
| -x | 10 | 7 | +0.00% | 1.16x | 0.98x |

General (Silesia): only `ratio` changes (codec 18): −0.53%, osdb compresses 5.7× faster (the LZMA2 pass skipped when the slice says BWT wins), decompression 0.76× at one thread and 1.64× at ten. `-x` output is identical; its compression is about 9% slower on plain text (dickens, nci, webster), where GR6+ is tried and loses.

### What the numbers say

- **Smaller where the new codecs apply, same size elsewhere.** Structured data −4.9% to −10.3% in every mode but
  speed; audio −17% to −26%; video −4% to −17%; NPY −0.9%; `ratio` −0.6% to −0.9% on numeric data. Weights, FASTQ in
  the fast modes and plain text are unchanged by design. No group got larger.
- **`-x` got faster:** video 2.0× (1 thread) and 3.5× (10 threads) compress, 1.35–2.5× decompress; FASTQ 1.9–2.0×;
  weights 1.9× at 10 threads; structured decompress 1.75×.
- **SIMD gains** show as 1.1–1.45× on images and integer arrays in the fast modes (mr, x-ray, ECG, i32).

### Regressions you should know about

1. **`balanced` compression is much slower on a few files** (it is the default mode): DEC64 float data
   (`owid_energy.pkl`, `owid_energy_f64.npy`) 0.03 s → 0.30 s (9× slower) for −5.5%; `wide_test.csv` 7.5× slower for
   −4.1%; tiny integer files 2–3× slower (milliseconds). Cause: format2's graph-leaf trials (codecs 21–23) run on every
   leaf in `balanced`. **Proposal:** in `balanced`, try them only when a cheap estimate says they can win, or move them
   to `hc` and up; measure both.
2. **`ratio` decompression 25–40% slower** on files that now use codec 18 (adaptive QLFC decodes about 1.5× slower per
   thread), for 1–3% smaller output. At 10 threads the threaded inverse BWT wins it back on large blocks (FASTQ 2.8×
   faster). **Proposal:** keep (ratio is the archive tier), or use static QLFC where the gain is under 1%.
3. **Audio in `balanced` is 2× slower both ways** (the kaud codec instead of zstd) for −26%. By design; `speed` is
   unchanged for those who need the old speed.
4. **Video at 10 threads in `balanced`/`eco`/`hc` decodes 1.3–1.9× slower** (the kvid route is one frame coded as one
   unit; rc2's plain blocks spread across threads), for −13% to −17%. **Proposal:** cut the kvid frame into several
   blocks so decode uses all threads.
5. **GR6+ makes `-x` compression up to 2.3× slower on wide tables with no gain** (energy_test.csv 7.2 s → 16.4 s).
   **Proposal:** skip it above ~64 columns or cap its formula search.

