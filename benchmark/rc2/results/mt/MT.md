# Multithreaded comparison (rc2, Apple M5, 16 November 2026)

Every tool with every thread it can use, on one machine: Apple M5, 10 cores (4 performance, 6 efficiency), 24 GB,
macOS. Kolmo is rc2 at `4d2c15d` (the SIMD merge after it changes nothing on arm64: the determinism check gives the same
110 frames). The research codecs run from their branches as merged into rc2 (`make experimental`), kaud and krans from
their own branches.

## Tiers

| tier | what runs | driver | rows |
|---|---|---|---|
| M1 | one file at a time, all cores: Kolmo and its research codecs `-T 10`; zstd/xz/lz4 `-T0`; 7-Zip `-mmt=10`; bzip3 `-j 10`; zpaq `-t10`; bsc with its own OpenMP threads; brotli has no threads | `run_cli2.py --budget 30` with `make_tools_mt.py` | `M1-<group>.jsonl` |
| M2 | 40 ESC-50 WAV clips, 10 at once (one per core): Kolmo `-T 1`, FLAC 5/8 through libsndfile in 10 processes, kaud profiles 0/1 | `set_parallel.py --workers 10` (run it with a Python that has numpy and soundfile) | `M2-audio.jsonl` |
| MS | in-process specialists with 10 threads: JPEG XL e7/e9, OpenJPEG, CharLS, ZipNN (`zstd_threads=10`), FFV1 (10 threads, 16 slices), CRAM 3.1 (pysam, 10 threads) | `run_spec2.py --threads 10` | `MS.jsonl` |

Times are wall time, fastest of the rounds the budget allows; every output is decoded and compared with its input.
`mt_pairs.py M1-*.jsonl` prints the pair tables (Kolmo's or a research codec's total size against its rival on the
same files, and the speed ratios; above 1 = Kolmo faster).

## Where Kolmo or a research codec beats the leaders with all cores busy

| data (files) | Kolmo / research | rival (all threads) | size | compress | decompress |
|---|---|---|---:|---:|---:|
| structured: CSV, JSON, logs, pickle (9) | `-m balanced` | zstd -3 | −57.5% | 1.34× | 1.58× |
| structured (9) | `-m ratio` | xz -9 | −50.6% | 2.36× | 1.41× |
| structured (9) | `-x` | zpaq -m5 | −45.3% | 2.47× | 3.40× |
| numeric: x-ray, MR, ECG, int columns (6) | `-m balanced` | zstd -3 | −36.7% | 1.27× | 1.68× |
| numeric (6) | `-x` | zpaq -m5 | −5.7% | 1.25× | 1.40× |
| NPY arrays (11) | kbin | zstd -19 | −21.0% | 35.6× | 1.46× |
| general: Silesia tar and 6 Silesia files (7) | `-m balanced` | zstd -3 | −8.7% | 1.15× | 2.73× |
| general (7) | `-m hc` | zstd -19 | −6.5% | 2.10× | 1.77× |
| general (7) | `-m ratio` | xz -9 | −12.7% | 2.54× | 1.68× |
| model weights: bf16, nvfp4 safetensors (3) | `-m balanced` | zstd -3 | −12.8% | 0.85× | 2.50× |
| model weights (3) | `-m balanced` | ZipNN, 10 threads | −0.8% | 2.1× | 4.8× |
| model weights (3) | `-x` | zpaq -m5 | −6.8% | 1.30× | 4.73× |
| FASTQ (3 runs) | kfam (= `-x`, codec 16) | CRAM 3.1, 10 threads | −34.3% | 3.4× | 0.25× |
| FASTQ (3) | kfam | zpaq -m5 | −24.4% | 23× | 42× |
| FASTQ (3) | kfam | xz -9 | −31.6% | 4.8× | 0.16× |
| raw video, CIF 4:2:0 (3) | kvid `-m balanced` | FFV1, 10 threads, 16 slices | −14.0% | 0.94× | 2.08× |
| raw video (3) | kvid `-m hc` | FFV1 | −18.5% | 0.02× | 1.9× |
| raw video (3) | kvid `-m ratio` | xz -9 | −16.0% | 0.61× | 1.08× |
| 16-bit CT (x-ray) | `-x` | JPEG-LS (best image codec here) | −0.9% | 0.007× | 0.005× |
| audio, 40 clips, 10 at once | kaud profile 0 | FLAC -8, 10 processes | −4.0% | 5.6× | 0.25× |
| audio (40) | kaud profile 1 | FLAC -8 | −8.2% | 2.2× | 0.08× |

FLAC runs through libsndfile from Python worker processes, which adds WAV parsing and process overhead to its compress
time. Its decode at 1.5 GB/s shows the overhead is small there; the compress ratio is likely too kind to kaud. Treat
5.6× as an upper bound.

## Where all cores do not change the verdict

- **KRC, KBC, KBX** (research codecs): larger than zstd -19 on every group except numeric (KBX −10%, KBC −2%) and
  slower than `-m balanced` by 30–100× in both directions. The Metal GPU path (`krc hybrid`) is not faster than its
  CPU path on these inputs (0.32× against 0.35× of balanced's compress speed). Multithreading does not make them
  competitive.
- **krans alone** (entropy coding without modelling) is +56% against zstd -3. It is a leaf coder, measured where it
  belongs in the round 3 report (plane leaves), not a standalone tool.
- **`-x` (context mixing) uses 128 MiB blocks**, so a file under 128 MiB is one block on one thread: `-T 10` does not
  speed it up. It stays at 0.4–1 MB/s. Against zpaq it is smaller everywhere but slower on general data (0.58×
  compress, 0.52× decompress).
- **`-x` on FASTQ** is as small as kfam (codec 16 wins every block) but 21–25× slower to compress, because it also
  runs the context-mixing trial on the block. Running the family coder first and skipping the CM trial when it
  applies would make `-x` on FASTQ as fast as kfam with the same bytes.
- **Images:** JPEG XL e9 stays smallest on MR (Kolmo `-x` +5.9%). `-m balanced` decodes 12–23× faster than every
  image codec but is larger than the best one (+1.8% on x-ray, +27% on MR).
- **Audio in Kolmo's own modes:** `-m ratio` is +21% against FLAC -8; only kaud wins.
- **Genomic text (chr22 FASTA):** kfam's one-stream family is +9.9% against `-x`.
- **kbin on all 11 NPY files:** the same size as `-m balanced` at the same speed; on ECG alone it is −4.4% against
  `-m ratio` (round 3).

Full pair tables per group: `MT-pairs.md`.
