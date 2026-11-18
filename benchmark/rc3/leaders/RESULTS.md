# rc3 against the world leaders (baseline before wave 2)

18 November 2026. Apple M5 (10 cores: 4 performance, 6 efficiency), 24 GB, macOS. Kolmo = rc3 at `1536036` (all rc3
lanes merged, before the wave-2 lanes). This is the baseline the wave-2 build is measured against.

## Method

- **Command-line tools** (`make_tools.py`, driven by `benchmark/rc2/run_cli2.py`): every tool on **1 thread**, then
  every tool on **10 threads** (the Homebrew bsc build and brotli have no threads). Whole processes, wall time from
  `wait4`, fastest of up to three rounds within 20 s, every decoded output compared with the input by SHA-256.
  Rivals: lz4 -1, zstd -3 / -19 / -19 --long=27, brotli -11, xz -9, 7-Zip LZMA2 -mx9, bsc -m0 -e2, bzip3 -b 16, zpaq -m5.
- **Specialists, in process** (`run_spec3.py` = rc2's `run_spec2.py` with the zipnn 0.5 constructor; `run_img8.py`):
  JPEG XL lossless e7/e9, WebP lossless, PNG -9, JPEG-LS, JPEG 2000 reversible (imagecodecs); FLAC 5/8 (libsndfile);
  FFV1 (FFmpeg via PyAV); CRAM 3.1 (htslib via pysam); ZipNN 0.5.4. Kolmo through its Python binding.
- **Data:** enwik8 and Silesia (dickens, webster, reymont, xml; silesia.tar, mozilla, ooffice, samba, osdb, nci);
  structured (OWID CSVs, wide/energy tables, JSON, Loghub logs, a pickle, the covid long table); numeric (x-ray, mr,
  int columns, ECG, sao); NumPy arrays; safetensors weights and an ONNX model; 40 ESC-50 WAV clips; 18 scikit-image
  sample images as PPM/PGM (the Kodak site refused automated downloads); three CIF Y4M clips; three ENA FASTQ runs and
  chr22.fa.
- **Leaders not measured:** OpenZL, cmix/paq8px (no build allowed from source in this session).
- ZipNN 0.5.4 rewrites its input buffer in place; the runner gives it a fresh copy per call (the copy is inside its
  timed call: about 2 ms per 10 MB). A first run without the copy made Kolmo compress the rewritten bytes; those rows
  were discarded.

## Key findings

- **Kolmo is the smallest of every tool in 9 of 10 groups**, with `-x` (video: `kvid -m x`, the same bytes as `-x`).
  The exception is model weights, where xz -9 is smallest and Kolmo `ratio` is +0.5%.
- **Against the leader of each tier**, at one thread: tables/logs/JSON −46% to −59%, numeric −21% to −44%, audio −28%
  to −46%, video −7% to −29%, NumPy −6% to −38%, images −9% to −18% (−0.4% to +0.4% at `ratio`), Silesia −6% to −13%,
  plain text +0.5% (`balanced` vs zstd -3) to −5%.
- **Speed:** decompression is often faster than the tier's leader; at 10 threads Kolmo is 1.2× to 5× faster on most
  groups.
- **Specialists:**
  - video: `balanced` is 9% smaller than FFV1 and 3.5× (compress) and 7.6× (decompress) faster at one thread;
  - FASTQ: `-x` is 34% smaller than CRAM 3.1 and compresses 3× faster; `ratio` is 9% smaller than CRAM;
  - audio: `ratio` is 8.9% smaller than FLAC -8, `balanced` 4.6%, but FLAC decodes 2.3–15× faster;
  - 16-bit images: `-x` is within 0.3% of JPEG XL e7;
  - weights: ZipNN 0.5.4 compresses 3× and decompresses 1.6× faster than `balanced`, which is 5% smaller on bf16 and
    0.6% larger on nvfp4.
- **Weak spots:**
  - 8-bit images: `-x` is 13% larger than JPEG XL e9, `balanced` 79% larger (but 40× faster to decode);
  - `eco` against brotli -11 on text (+6% to +12%);
  - `balanced` compression speed on tables, NumPy arrays and audio (0.25–0.4× zstd -3; the wave-2 regress lane
    addresses tables and arrays);
  - `hc` and `eco` on FASTQ (+10% and +27%).

## Command-line tools: each Kolmo level against the leader of its tier

Size: total bytes, Kolmo vs the leader on the same files (negative = Kolmo smaller). Speeds: Kolmo MB/s ÷ leader MB/s.

### audio, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 40 | -2.3% | 0.93x | 1.07x |
| balanced | zstd -3 | 40 | -45.6% | 0.25x | 0.54x |
| eco | brotli -11 | 40 | -34.0% | 29.42x | 0.88x |
| hc | zstd -19 --long=27 | 40 | -42.4% | 0.55x | 0.58x |
| ratio | bzip3 -b 16 | 40 | -32.4% | 0.42x | 1.13x |
| -x | zpaq -m5 | 40 | -28.5% | 1.48x | 24.60x |

Smallest of all on the 40 common files: **kolmo -x** (6,537,582 bytes); next: kolmo ratio +0.0%, kolmo balanced +4.6%, kolmo eco +4.6%, kolmo hc +4.6%

### audio, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 40 | -2.3% | 0.93x | 1.06x |
| balanced | zstd -3 | 40 | -45.6% | 0.25x | 0.54x |
| eco | brotli -11 | 40 | -34.0% | 29.56x | 0.88x |
| hc | zstd -19 --long=27 | 40 | -42.4% | 0.54x | 0.57x |
| ratio | bzip3 -b 16 | 40 | -32.4% | 0.71x | 2.41x |
| -x | zpaq -m5 | 40 | -28.5% | 1.64x | 24.50x |

Smallest of all on the 40 common files: **kolmo -x** (6,537,582 bytes); next: kolmo ratio +0.0%, kolmo balanced +4.6%, kolmo eco +4.6%, kolmo hc +4.6%

### general, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 6 | -0.1% | 0.93x | 1.32x |
| balanced | zstd -3 | 6 | -9.1% | 0.87x | 1.04x |
| eco | brotli -11 | 6 | +5.8% | 40.74x | 1.08x |
| hc | zstd -19 --long=27 | 6 | -6.1% | 1.83x | 0.97x |
| ratio | 7-Zip LZMA2 -mx9 | 6 | -12.7% | 1.63x | 0.45x |
| -x | zpaq -m5 | 6 | -7.4% | 1.09x | 1.18x |

Smallest of all on the 6 common files: **kolmo -x** (55,942,741 bytes); next: zpaq -m5 +7.9%, kolmo ratio +13.6%, 7-Zip LZMA2 -mx9 +30.1%, xz -9 +30.3%

### general, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 6 | -0.1% | 1.51x | 2.00x |
| balanced | zstd -3 | 6 | -9.1% | 1.10x | 2.78x |
| eco | brotli -11 | 6 | +5.8% | 86.14x | 2.82x |
| hc | zstd -19 --long=27 | 6 | -6.1% | 3.09x | 2.02x |
| ratio | 7-Zip LZMA2 -mx9 | 6 | -12.7% | 1.89x | 1.24x |
| -x | zpaq -m5 | 6 | -7.4% | 1.10x | 1.19x |

Smallest of all on the 6 common files: **kolmo -x** (55,942,741 bytes); next: zpaq -m5 +7.9%, kolmo ratio +13.6%, 7-Zip LZMA2 -mx9 +30.2%, xz -9 +30.4%

### genomics, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 4 | +0.2% | 0.93x | 1.21x |
| balanced | zstd -3 | 4 | -0.0% | 0.92x | 1.23x |
| eco | brotli -11 | 4 | +27.4% | 21.14x | 1.33x |
| hc | zstd -19 --long=27 | 4 | +10.1% | 1.26x | 1.15x |
| ratio | bsc -m0 -e2 | 4 | -4.8% | 0.92x | 0.99x |
| -x | zpaq -m5 | 4 | -17.9% | 3.02x | 3.13x |

Smallest of all on the 4 common files: **kolmo -x** (24,780,807 bytes); next: kfam +9.8%, zpaq -m5 +21.8%, kolmo ratio +29.7%, bsc -m0 -e2 +36.2%

### genomics, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 4 | +0.2% | 1.25x | 2.26x |
| balanced | zstd -3 | 4 | -0.0% | 1.14x | 3.17x |
| eco | brotli -11 | 4 | +27.4% | 45.32x | 2.62x |
| hc | zstd -19 --long=27 | 4 | +10.1% | 2.28x | 1.78x |
| ratio | bsc -m0 -e2 | 4 | -4.8% | 1.96x | 4.36x |
| -x | zpaq -m5 | 4 | -17.9% | 3.20x | 3.32x |

Smallest of all on the 4 common files: **kolmo -x** (24,780,807 bytes); next: kfam +9.8%, zpaq -m5 +21.8%, kolmo ratio +29.7%, bsc -m0 -e2 +36.2%

### img8, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 18 | -0.0% | 0.98x | 1.09x |
| balanced | zstd -3 | 18 | -16.6% | 0.42x | 1.08x |
| eco | brotli -11 | 18 | -4.9% | 18.68x | 1.26x |
| hc | zstd -19 --long=27 | 18 | -18.3% | 0.61x | 1.04x |
| ratio | bzip3 -b 16 | 18 | +0.4% | 0.30x | 1.46x |
| -x | zpaq -m5 | 18 | -8.7% | 0.81x | 1.19x |

Smallest of all on the 18 common files: **kolmo -x** (4,755,779 bytes); next: zpaq -m5 +9.5%, bzip3 -b 16 +17.0%, kolmo ratio +17.4%, bsc -m0 -e2 +23.2%

### img8, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 18 | -0.0% | 1.05x | 1.10x |
| balanced | zstd -3 | 18 | -16.6% | 0.46x | 1.16x |
| eco | brotli -11 | 18 | -4.9% | 18.82x | 1.27x |
| hc | zstd -19 --long=27 | 18 | -18.3% | 0.61x | 1.05x |
| ratio | bzip3 -b 16 | 18 | +0.4% | 0.43x | 2.60x |
| -x | zpaq -m5 | 18 | -8.7% | 0.83x | 1.20x |

Smallest of all on the 18 common files: **kolmo -x** (4,755,779 bytes); next: zpaq -m5 +9.5%, bzip3 -b 16 +17.0%, kolmo ratio +17.4%, bsc -m0 -e2 +23.2%

### numeric, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 7 | -5.5% | 1.21x | 1.32x |
| balanced | zstd -3 | 7 | -44.0% | 1.03x | 1.12x |
| eco | brotli -11 | 7 | -31.2% | 54.16x | 1.38x |
| hc | zstd -19 --long=27 | 7 | -37.2% | 2.75x | 1.03x |
| ratio | bzip3 -b 16 | 7 | -24.0% | 0.69x | 3.16x |
| -x | zpaq -m5 | 7 | -20.7% | 1.55x | 1.70x |

Smallest of all on the 7 common files: **kolmo -x** (8,246,806 bytes); next: kolmo ratio +2.6%, kolmo hc +8.8%, kolmo eco +8.9%, kolmo balanced +11.3%

### numeric, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 7 | -5.5% | 1.25x | 1.35x |
| balanced | zstd -3 | 7 | -44.0% | 1.27x | 1.75x |
| eco | brotli -11 | 7 | -31.2% | 53.10x | 1.36x |
| hc | zstd -19 --long=27 | 7 | -37.2% | 2.64x | 1.04x |
| ratio | bzip3 -b 16 | 7 | -24.0% | 0.73x | 3.62x |
| -x | zpaq -m5 | 7 | -20.7% | 1.62x | 1.73x |

Smallest of all on the 7 common files: **kolmo -x** (8,246,806 bytes); next: kolmo ratio +2.6%, kolmo hc +8.8%, kolmo eco +8.9%, kolmo balanced +11.3%

### numeric_npy, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 11 | -6.9% | 1.04x | 1.27x |
| balanced | zstd -3 | 11 | -37.9% | 0.32x | 1.07x |
| eco | brotli -11 | 11 | -20.3% | 44.00x | 1.14x |
| hc | zstd -19 --long=27 | 11 | -27.8% | 1.68x | 1.02x |
| ratio | bzip3 -b 16 | 11 | -5.6% | 0.71x | 2.41x |
| -x | zpaq -m5 | 11 | -6.3% | 2.01x | 2.56x |

Smallest of all on the 11 common files: **kolmo -x** (8,453,262 bytes); next: kolmo ratio +4.0%, zpaq -m5 +6.7%, bzip3 -b 16 +10.1%, kolmo hc +11.4%

### numeric_npy, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 11 | -6.9% | 1.21x | 1.32x |
| balanced | zstd -3 | 11 | -37.9% | 0.72x | 1.56x |
| eco | brotli -11 | 11 | -20.3% | 47.95x | 1.18x |
| hc | zstd -19 --long=27 | 11 | -27.8% | 1.98x | 1.04x |
| ratio | bzip3 -b 16 | 11 | -5.6% | 0.76x | 2.91x |
| -x | zpaq -m5 | 11 | -6.3% | 2.08x | 2.56x |

Smallest of all on the 11 common files: **kolmo -x** (8,453,262 bytes); next: kolmo ratio +4.0%, zpaq -m5 +6.7%, bzip3 -b 16 +10.1%, kolmo hc +11.4%

### structured, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 9 | +1.1% | 0.96x | 1.34x |
| balanced | zstd -3 | 9 | -59.2% | 0.27x | 0.83x |
| eco | brotli -11 | 9 | -46.1% | 71.49x | 1.08x |
| hc | zstd -19 --long=27 | 9 | -52.1% | 4.64x | 0.75x |
| ratio | bzip3 -b 16 | 9 | -50.5% | 0.76x | 4.90x |
| -x | zpaq -m5 | 9 | -45.7% | 1.67x | 5.47x |

Smallest of all on the 9 common files: **kolmo -x** (5,592,865 bytes); next: kolmo ratio +7.9%, kolmo hc +14.3%, kolmo eco +17.2%, kolmo balanced +32.3%

### structured, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 9 | +1.1% | 1.50x | 1.65x |
| balanced | zstd -3 | 9 | -59.2% | 0.80x | 1.58x |
| eco | brotli -11 | 9 | -46.1% | 85.89x | 1.10x |
| hc | zstd -19 --long=27 | 9 | -52.1% | 5.33x | 0.78x |
| ratio | bzip3 -b 16 | 9 | -50.5% | 0.77x | 5.28x |
| -x | zpaq -m5 | 9 | -45.7% | 1.97x | 5.68x |

Smallest of all on the 9 common files: **kolmo -x** (5,592,865 bytes); next: kolmo ratio +7.9%, kolmo hc +14.3%, kolmo eco +17.2%, kolmo balanced +32.3%

### text, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 5 | +0.7% | 0.93x | 1.17x |
| balanced | zstd -3 | 5 | +0.5% | 1.00x | 1.21x |
| eco | brotli -11 | 5 | +12.1% | 26.01x | 0.81x |
| hc | zstd -19 --long=27 | 5 | -4.7% | 1.06x | 0.74x |
| ratio | bsc -m0 -e2 | 5 | -5.3% | 0.75x | 0.97x |
| -x | zpaq -m5 | 5 | -4.8% | 0.73x | 0.83x |

Smallest of all on the 5 common files: **kolmo -x** (27,293,508 bytes); next: zpaq -m5 +5.0%, kolmo ratio +13.1%, bsc -m0 -e2 +19.4%, bzip3 -b 16 +21.2%

### text, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 5 | +0.7% | 1.34x | 2.27x |
| balanced | zstd -3 | 5 | +0.5% | 1.42x | 3.01x |
| eco | brotli -11 | 5 | +12.1% | 52.24x | 1.96x |
| hc | zstd -19 --long=27 | 5 | -4.7% | 1.05x | 0.80x |
| ratio | bsc -m0 -e2 | 5 | -5.3% | 1.42x | 3.85x |
| -x | zpaq -m5 | 5 | -4.8% | 0.59x | 0.66x |

Smallest of all on the 5 common files: **kolmo -x** (27,293,508 bytes); next: zpaq -m5 +5.0%, kolmo ratio +13.1%, bsc -m0 -e2 +19.4%, bzip3 -b 16 +21.2%

### video, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 3 | -0.0% | 0.91x | 1.27x |
| balanced | zstd -3 | 3 | -29.1% | 0.69x | 0.46x |
| eco | brotli -11 | 3 | -15.1% | 15.97x | 1.01x |
| hc | zstd -19 --long=27 | 3 | -22.4% | 1.06x | 0.62x |
| ratio | bzip3 -b 16 | 3 | -7.1% | 0.15x | 3.43x |
| -x | zpaq -m5 | 3 | -12.4% | 1.58x | 1.55x |

Smallest of all on the 3 common files: **kvid -m x** (51,475,453 bytes); next: kolmo -x +0.0%, kvid -m ratio +7.7%, kolmo ratio +7.7%, kolmo hc +12.1%

### video, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 3 | -0.0% | 1.41x | 1.85x |
| balanced | zstd -3 | 3 | -29.1% | 0.42x | 0.62x |
| eco | brotli -11 | 3 | -15.1% | 29.74x | 1.51x |
| hc | zstd -19 --long=27 | 3 | -22.4% | 1.97x | 0.73x |
| ratio | bzip3 -b 16 | 3 | -7.1% | 0.06x | 2.03x |
| -x | zpaq -m5 | 3 | -12.4% | 2.76x | 2.92x |

Smallest of all on the 3 common files: **kvid -m x** (51,475,453 bytes); next: kolmo -x +0.0%, kvid -m ratio +7.7%, kolmo ratio +7.7%, kolmo hc +12.1%

### weights, 1 thread

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 4 | -0.0% | 1.20x | 1.41x |
| balanced | zstd -3 | 4 | -12.6% | 0.74x | 1.67x |
| eco | brotli -11 | 4 | -4.4% | 41.81x | 2.86x |
| hc | zstd -19 --long=27 | 4 | -12.0% | 1.81x | 2.15x |
| ratio | xz -9 | 4 | +0.5% | 1.39x | 1.10x |
| -x | zpaq -m5 | 4 | -6.4% | 1.43x | 5.64x |

Smallest of all on the 4 common files: **xz -9** (53,058,244 bytes); next: 7-Zip LZMA2 -mx9 +0.0%, kolmo -x +0.2%, kolmo ratio +0.5%, kolmo hc +3.0%

### weights, 10 threads

| Kolmo | leader of the tier | files | size | compress | decompress |
|---|---|---|---:|---:|---:|
| speed | lz4 -1 | 4 | -0.0% | 1.34x | 1.38x |
| balanced | zstd -3 | 4 | -12.6% | 0.76x | 2.45x |
| eco | brotli -11 | 4 | -4.4% | 59.73x | 4.79x |
| hc | zstd -19 --long=27 | 4 | -12.0% | 3.01x | 2.60x |
| ratio | xz -9 | 4 | +0.5% | 1.37x | 1.07x |
| -x | zpaq -m5 | 4 | -6.4% | 2.55x | 5.68x |

Smallest of all on the 4 common files: **xz -9** (53,058,276 bytes); next: 7-Zip LZMA2 -mx9 +0.0%, kolmo -x +0.2%, kolmo ratio +0.5%, kolmo hc +3.0%

## Specialists

#### 8-bit images (scikit-image samples, PPM/PGM), 1 thread(s) (10 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| JPEG XL lossless e9 | 3,537,055 | +0.0% | 0.6 | 28.9 |
| JPEG XL lossless e7 | 3,606,303 | +2.0% | 3.6 | 30.5 |
| kolmo -x | 3,992,934 | +12.9% | 0.6 | 0.9 |
| WebP lossless m6 | 4,001,697 | +13.1% | 0.4 | 310.5 |
| JPEG 2000 reversible | 4,391,973 | +24.2% | 30.7 | 33.3 |
| kolmo -m ratio | 4,767,421 | +34.8% | 3.9 | 37.4 |
| JPEG-LS | 4,909,633 | +38.8% | 117.7 | 137.0 |
| PNG level 9 | 5,413,777 | +53.1% | 3.9 | 289.2 |
| kolmo -m balanced | 6,316,740 | +78.6% | 103.4 | 1172.6 |

#### 16-bit images (Silesia mr, x-ray), 1 thread(s) (2 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| JPEG XL lossless e7 (libjxl) | 5,485,501 | +0.0% | 7.2 | 55.5 |
| kolmo -x | 5,499,987 | +0.3% | 1.0 | 1.0 |
| kolmo -x -l 2 | 5,499,987 | +0.3% | 1.0 | 0.9 |
| JPEG XL lossless e9 (libjxl) | 5,507,143 | +0.4% | 1.0 | 49.4 |
| kolmo -m ratio | 5,606,441 | +2.2% | 7.0 | 58.1 |
| JPEG 2000 reversible (OpenJPEG) | 5,615,832 | +2.4% | 35.8 | 34.7 |
| JPEG-LS (CharLS) | 5,640,897 | +2.8% | 157.2 | 194.8 |
| kolmo -m balanced | 5,973,954 | +8.9% | 601.3 | 1181.5 |

#### audio (40 ESC-50 clips), 1 thread(s) (40 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kolmo -m ratio | 6,537,582 | +0.0% | 4.7 | 21.7 |
| kolmo -x -l 2 | 6,537,582 | +0.0% | 0.6 | 21.7 |
| kolmo -m balanced | 6,840,066 | +4.6% | 42.7 | 138.7 |
| FLAC level 8 | 7,119,853 | +8.9% | 177.0 | 325.0 |
| FLAC level 5 | 7,325,511 | +12.1% | 292.4 | 326.3 |
| kolmo -m balanced --typed-huf | 8,821,456 | +34.9% | 497.4 | 1221.4 |

#### model weights (3 safetensors), 1 thread(s) (3 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kolmo -m ratio | 50,712,451 | +0.0% | 3.8 | 102.8 |
| kolmo -m balanced | 53,526,218 | +5.5% | 663.2 | 3205.8 |
| ZipNN (2 byte groups, zstd, threads 1) | 54,970,184 | +8.4% | 2038.6 | 5064.4 |

#### video (3 CIF clips), 1 thread(s) (3 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kvid -m x -T 1 | 51,475,453 | +0.0% | 0.5 | 1.0 |
| kolmo -x -T 1 | 51,475,573 | +0.0% | 1.1 | 1.0 |
| kvid -m ratio -T 1 | 55,454,924 | +7.7% | 2.2 | 66.1 |
| kolmo ratio -T 1 | 55,455,044 | +7.7% | 2.3 | 64.9 |
| kolmo hc -T 1 | 57,692,520 | +12.1% | 4.8 | 514.3 |
| kolmo eco -T 1 | 59,154,684 | +14.9% | 13.2 | 312.6 |
| kvid -m balanced -T 1 | 60,850,615 | +18.2% | 217.1 | 622.9 |
| kolmo balanced -T 1 | 60,850,735 | +18.2% | 270.1 | 506.3 |
| xz -9 | 66,172,736 | +28.6% | 3.6 | 65.5 |
| FFV1 level 3, context 1 (FFmpeg) | 66,853,462 | +29.9% | 78.0 | 66.8 |
| zstd -19 --long=27 | 74,363,595 | +44.5% | 4.5 | 829.5 |
| zstd -19 | 74,498,518 | +44.7% | 4.8 | 761.8 |
| kolmo speed -T 1 | 122,438,477 | +137.9% | 1035.5 | 3040.5 |

#### FASTQ (3 runs), 1 thread(s) (3 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kfam -T 1 | 16,620,775 | +0.0% | 5.9 | 10.4 |
| kolmo -x -T 1 | 16,654,300 | +0.2% | 13.4 | 10.5 |
| zpaq -m5 | 21,993,408 | +32.3% | 0.6 | 0.6 |
| kolmo ratio -T 1 | 23,005,504 | +38.4% | 42.4 | 48.2 |
| xz -9 | 24,290,088 | +46.1% | 2.9 | 157.4 |
| bsc -m0 -e2 | 24,578,202 | +47.9% | 45.6 | 48.2 |
| CRAM 3.1 archive (htslib via pysam) | 25,289,976 | +52.2% | 4.1 | 102.6 |
| zstd -19 --long=27 | 25,371,176 | +52.6% | 3.2 | 1491.2 |
| zstd -19 | 28,014,884 | +68.6% | 3.3 | 1405.2 |
| kolmo hc -T 1 | 28,829,042 | +73.5% | 4.0 | 1711.1 |
| kolmo eco -T 1 | 33,759,905 | +103.1% | 17.3 | 636.2 |
| kolmo balanced -T 1 | 37,405,099 | +125.1% | 412.1 | 1486.0 |
| kolmo speed -T 1 | 60,723,691 | +265.3% | 667.7 | 2391.6 |

#### 8-bit images (scikit-image samples, PPM/PGM), 10 thread(s) (10 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| JPEG XL lossless e9 | 3,537,055 | +0.0% | 0.7 | 29.0 |
| JPEG XL lossless e7 | 3,606,303 | +2.0% | 4.5 | 30.7 |
| kolmo -x | 3,992,934 | +12.9% | 0.7 | 0.9 |
| WebP lossless m6 | 4,001,697 | +13.1% | 0.4 | 311.6 |
| JPEG 2000 reversible | 4,391,973 | +24.2% | 177.5 | 33.3 |
| kolmo -m ratio | 4,767,421 | +34.8% | 3.9 | 38.6 |
| JPEG-LS | 4,909,633 | +38.8% | 118.5 | 137.1 |
| PNG level 9 | 5,413,777 | +53.1% | 3.9 | 291.8 |
| kolmo -m balanced | 6,316,740 | +78.6% | 118.6 | 1734.5 |

#### 16-bit images (Silesia mr, x-ray), 10 thread(s) (2 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| JPEG XL lossless e7 (libjxl) | 5,485,501 | +0.0% | 39.1 | 332.1 |
| kolmo -x | 5,499,987 | +0.3% | 1.1 | 1.0 |
| kolmo -x -l 2 | 5,499,987 | +0.3% | 1.1 | 1.0 |
| JPEG XL lossless e9 (libjxl) | 5,507,143 | +0.4% | 5.9 | 310.3 |
| kolmo -m ratio | 5,606,441 | +2.2% | 7.4 | 61.0 |
| JPEG 2000 reversible (OpenJPEG) | 5,615,832 | +2.4% | 263.4 | 264.3 |
| JPEG-LS (CharLS) | 5,640,897 | +2.8% | 183.6 | 226.1 |
| kolmo -m balanced | 5,973,954 | +8.9% | 1014.9 | 5437.5 |

#### audio (40 ESC-50 clips), 10 thread(s) (40 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kolmo -m ratio | 6,537,582 | +0.0% | 4.6 | 21.7 |
| kolmo -x -l 2 | 6,537,582 | +0.0% | 0.7 | 21.7 |
| kolmo -m balanced | 6,840,066 | +4.6% | 42.4 | 139.5 |
| kolmo -m balanced --typed-huf | 6,840,066 | +4.6% | 42.6 | 139.6 |
| FLAC level 8 | 7,119,853 | +8.9% | 175.6 | 322.9 |
| FLAC level 5 | 7,325,511 | +12.1% | 289.1 | 323.9 |

#### model weights (3 safetensors), 10 thread(s) (3 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kolmo -m ratio | 50,712,451 | +0.0% | 4.6 | 102.8 |
| kolmo -m balanced | 53,526,218 | +5.5% | 1778.8 | 10645.4 |
| ZipNN (2 byte groups, zstd, threads 10) | 54,970,184 | +8.4% | 8350.7 | 30343.6 |

#### video (3 CIF clips), 10 thread(s) (3 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kvid -m x -T 10 | 51,475,453 | +0.0% | 0.9 | 1.9 |
| kolmo -x -T 10 | 51,475,573 | +0.0% | 1.9 | 1.9 |
| kvid -m ratio -T 10 | 55,454,924 | +7.7% | 2.2 | 85.9 |
| kolmo ratio -T 10 | 55,455,044 | +7.7% | 2.3 | 84.3 |
| kolmo hc -T 10 | 57,692,520 | +12.1% | 8.8 | 608.3 |
| kolmo eco -T 10 | 59,154,684 | +14.9% | 24.9 | 465.3 |
| kvid -m balanced -T 10 | 60,850,615 | +18.2% | 541.8 | 902.9 |
| kolmo balanced -T 10 | 60,850,735 | +18.2% | 563.2 | 681.6 |
| xz -9 | 66,172,760 | +28.6% | 3.6 | 66.7 |
| FFV1 level 3, context 1 (FFmpeg), 10 threads, 16 slices | 70,748,789 | +37.4% | 349.8 | 326.8 |
| zstd -19 --long=27 | 74,363,595 | +44.5% | 4.5 | 831.0 |
| zstd -19 | 74,498,518 | +44.7% | 6.3 | 758.3 |
| kolmo speed -T 10 | 122,438,477 | +137.9% | 3490.6 | 4356.8 |

#### FASTQ (3 runs), 10 thread(s) (3 files)

| tool | bytes | vs smallest | compress MB/s | decompress MB/s |
|---|---:|---:|---:|---:|
| kfam -T 10 | 16,620,775 | +0.0% | 13.9 | 25.5 |
| kolmo -x -T 10 | 16,654,300 | +0.2% | 29.6 | 25.0 |
| zpaq -m5 | 21,993,408 | +32.3% | 0.6 | 0.6 |
| kolmo ratio -T 10 | 23,005,504 | +38.4% | 88.3 | 206.4 |
| xz -9 | 24,290,112 | +46.1% | 3.0 | 165.9 |
| bsc -m0 -e2 | 24,578,202 | +47.9% | 45.6 | 48.3 |
| CRAM 3.1 archive (htslib via pysam) | 25,289,976 | +52.2% | 4.1 | 100.4 |
| zstd -19 --long=27 | 25,371,176 | +52.6% | 3.2 | 1500.8 |
| zstd -19 | 28,014,884 | +68.6% | 4.3 | 1398.0 |
| kolmo hc -T 10 | 28,829,042 | +73.5% | 7.4 | 2579.3 |
| kolmo eco -T 10 | 33,759,905 | +103.1% | 37.3 | 1256.3 |
| kolmo balanced -T 10 | 37,405,099 | +125.1% | 1716.9 | 3785.1 |
| kolmo speed -T 10 | 60,723,691 | +265.3% | 2860.0 | 4255.5 |

## Reproduce

`make_tools.py BIN SCRIPTS 1|10 tools.json`, then `run_cli2.py --tools tools.json --group G --files ... --out rows.jsonl`
per group; `run_spec3.py --threads T ...`; `run_img8.py --threads T ...`; `summarize.py results/T*-*.jsonl`.
