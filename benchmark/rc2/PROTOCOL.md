# Kolmo 1.2.0-rc2 against the leaders: protocol

Written on 15 November 2026, before the first timed run of rc2. It extends the rc1 protocol
(`benchmark/rc1/PROTOCOL.md`), whose rules all hold: same Mac (Apple M5, 24 GiB), every program on one thread,
same session, no timing from a cache or another day, sizes within 1% and speeds within 10% count as the same,
"ahead" = smaller and not slower, "behind" = larger, or the same size and slower, "trade" = smaller but more than
10% slower in one direction.

## Builds

- Kolmo: commit `48acbf6` of `claude/release-rc2-20261115` (1.2.0-rc2), `make ARCH= all lib` (`-O3`, no `-mcpu`).
  Control: the same commit with `-DKOLMO_FORCE_GENERIC_INT_INV -DKOLMO_FORCE_GENERIC_INT_FWD`.
- Standalone research tools of the same commit (`experimental.mk`): `kolmo-fastq`, `kvid`, `krc-cpu`, `krc-auto`,
  `kbc`, `kbx`, built with the same flags.
- lzbench 2.4.1 at `9be0e25`, a fresh clone (`tools/lzbench-kolmo-rc2`), set up by `setup_macos.sh`.
- Rivals installed as prebuilt packages into the benchmark virtual environment (Python 3.9.6): `imagecodecs`
  2024.12.30 (JPEG-LS by CharLS, JPEG 2000 by OpenJPEG, JPEG XL by libjxl), `av` 15.1.0 (FFmpeg's FFV1),
  `pysam` 0.24.1 (CRAM 3.1 by htslib and htscodecs), `zipnn`. Command-line rivals as in rc1 (zstd 1.5.7, xz,
  brotli, bsc, bzip3 from Homebrew).
- Not available: source builds of third-party tools (NAF, fqzcomp5, Spring, WavPack, mcm, a full FFmpeg) were
  refused by this session's permission check, so those tools are not measured.

## Tiers

| Tier | What | Timing |
|---|---|---|
| A1, A2, A3 | as rc1: `silesia.tar`, the 35 files, the 18 typed arrays, in lzbench | in memory, lzbench |
| C | whole processes (`run_cli2.py`): FASTQ, video, the research codecs on six general files | `wait4`, fastest of up to 3 rounds within 20 s |
| S | in one Python process (`run_spec2.py`): FFV1, CRAM 3.1, JPEG-LS, JPEG 2000, JPEG XL 16-bit, ZipNN, FLAC, each beside Kolmo's binding | `perf_counter` and `process_time` around each call; at least 3 runs and 0.25 s, at most 3 s |
| G | the research codecs and Kolmo at 1, 4 and 10 threads and on the GPU | as tier C; **not** a one-thread comparison, reported apart |

## Inputs

- FASTQ: the first 100,000 forward reads of SRR1770413 (E. coli, MiSeq), SRR2584863 (E. coli; the file of Codex's
  7 November comparison, 36,637,534 bytes) and SRR835775 (human, HiSeq 2000), from ENA.
- Video: `akiyo_cif`, `foreman_cif`, `mobile_cif` (8-bit 4:2:0, 352 x 288, 300 frames) from media.xiph.org.
- 16-bit images: `mr_u16.npy`, `xray_u16.npy` (from Silesia's `mr` and `x-ray`, as in rc1).
- Model weights: the three safetensors files of the 35.
- Audio: the 40 ESC-50 clips of rc1.
- General files for the research codecs: `silesia.tar`, dickens, mozilla, osdb, nci, x-ray.

## Comparisons fixed before measuring

| Area | Kolmo setting | Leader |
|---|---|---|
| FASTQ | `kolmo-fastq`; `-m balanced`, `-m ratio`, `-x` | CRAM 3.1 (archive profile); zstd -19, xz -9, bsc |
| Video | `kvid -m ratio`, `kvid -x`; `kolmo -m ratio` on the raw file | FFV1 (level 3, range coder, one slice, one thread) |
| 16-bit images | `-m balanced`, `-m ratio`, `-x` | JPEG-LS, JPEG 2000 reversible, JPEG XL lossless effort 7 and 9 |
| Model weights | `-m balanced`, `-m ratio` | ZipNN |
| Research codecs | KRC1, KBC1, KBX3 (`-l 0, 1, 2`) | Kolmo `-m balanced`, `-m hc`, `-m ratio`; zstd -3, -19, xz -9, bsc |

## Changes after freezing

None yet.
