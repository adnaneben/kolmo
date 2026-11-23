# Changelog

## Unreleased

### Format

- **`FORMAT.md`: the specification of container version 12.** It is complete for the container layer: header, index,
  CRC-32C, `kolmo_khash64` written out with test vectors (`tests/khash64_vectors.txt`), plan prefix and transform flag
  rules, stored, self-contained, video and graph blocks, extensions, the `CZNS` stream and the declared decode working
  set. It has a registry of every codec id with the status of its specification. Coders whose bytes are not yet
  written out are specified by the reference implementation at `v0.0.2` (FORMAT.md section 13). The format itself is
  unchanged.
- **`COMPATIBILITY.md`: the format promise.** Every release reads every version-12 frame written by 0.0.1 or later,
  to the same bytes. Identifiers never change meaning. New features use identifiers older releases refuse. Reading
  version 12 is never removed.
- **Frozen corpus of 0.0.2 frames.** `tests/v12/r0.0.2/` adds 246 frames written by the 0.0.2 release binary, with
  their originals: every level, the format-changing options, the stream wrapper, and inputs that reach most block
  codecs and transforms 0.0.2 writes. Block codecs 25, 26 and 31 and tflags 2, 6 and 8 are not yet reached
  (COMPATIBILITY.md).
  - `make check` decodes them through the library (`test-v12-corpus`) and the CLI (`tests/check_v12_frames.py`, which
    also fails if a frozen frame changes).
  - It re-checks every frame of the corpus against FORMAT.md with an independent Python implementation of the
    container layer (`tests/format_reference.py`).
  - `tests/make_v12_frames.py` adds each release's frames (RELEASING.md).

### Packaging

- **Distribution drafts in `packaging/`, nothing submitted yet** (`packaging/README.md` has the steps for each
  channel). They build the 0.0.2 tarball, with the version and sha256 in one place per recipe:
  - conda-forge: `libkolmo` and `kolmo` in one recipe, `kolmoc` in another
  - Debian (`kolmo`, `libkolmo0`, `libkolmo-dev`), with autopkgtests and an ITP draft
  - an AUR `PKGBUILD`
  - a nixpkgs `package.nix`
  - a homebrew-core formula, which is not eligible until the notability rule is met
- **CI workflow `packaging`.** It builds and tests each draft in a clean environment, and builds the Debian package
  from the checkout as well.

## 0.0.2 (2026-11-23)

The level model and a new command line (`docs/LEVELS.md`, owner decisions of 2026-11-22), decoder hardening, and
match finders that use several threads inside a block. **The frame format is unchanged** (container version 12):
0.0.1 reads 0.0.2 output and 0.0.2 reads 0.0.1 output. Levels 1, 3, 5, 7 and ultra write the same bytes as 0.0.1's
`speed`, `balanced`, `eco`, `hc` and `-x`; level 9 writes the same bytes as 0.0.1's `ratio` except where block sorting
now wins on a binary block (see Performance). **The command line breaks**: 0.0.1's options are refused with a hint.

### Levels and command line

- **Levels.** One scale from `-1` (fastest) to `-9` (max), `-3` (light) the default, and `-U` (ultra) outside it.
  Named anchors with letters: `-F --fastest` (1), `-L --light` (3), `-B --balanced` (5), `-C --compact` (7),
  `-X --max` (9), `-U --ultra`; `--level N|ultra`; the last level option wins. Levels 2, 4, 6 and 8 are new settings
  (the fast KLZH2 parse with minimal analysis, KLZH2 5 with hc's planning, KLZH2 8, block sorting for every block); the table will be retuned by
  measurement, never moved across the scale.
- **Library:** `kolmo_params_for_level(level, &params)` and `KOLMO_LEVEL_MIN/MAX/DEFAULT/ULTRA/FASTEST/LIGHT/
  BALANCED/COMPACT/MAX_RATIO`; a level outside 1..10 returns the new `KOLMO_ERR_PARAM` (-15). The command line and both
  bindings use this one table, so a level gives the same bytes everywhere.
- **Command line** (gzip/xz/zstd conventions): `kolmo FILE` writes `FILE.kolmo` and keeps `FILE` (`--rm` removes it);
  `kolmo -d FILE.kolmo` writes `FILE`; no FILE or `-` is standard input to standard output; several files in one run
  (processing continues after an error); `-c`, `-o FILE` (one input), `-k`, `-f` (an existing output is never
  overwritten without it), `-t` test, `-l` list, `-q`, `-v`, `-T`, `-M`; combined short options (`-dc`, `-9T4`),
  `--opt=value`, `--`. Compressed data is not written to (or read from) a terminal without `-f`. Every output is staged
  and published atomically with the input's permissions and mtime, and a failed run never touches an existing file;
  the output is never the input file. `-d` decodes concatenated frames in order (as zstd) and fails on trailing data.
  Exit status 0, 1 (error) or 2 (usage). Expert options: `--block-size`, `--analysis`, `--stream[=SEGMiB]`,
  `--range`, `--codec-level`, with `--target`, `--calib`, `--memlimit`, `--native-leaf`, `--typed-huf`, `--gpu` and
  `--x-tables` as before.
- **Archive subcommands renamed:** `kolmo a`, `x`, `l` are now `kolmo archive`, `extract`, `contents` (recognised only
  as the first argument; a file of that name is `kolmo ./NAME` or `kolmo -- NAME`). `--index`, `archive` take levels.
- **Removed** (each prints a one-line hint naming the replacement and exits with status 2): `-m MODE`, `-x`, the old
  `-l LEVEL`, `-B MiB`, `-Q`, `-n`, `-s`, `-S`, `-r`, `-D`, `-t DICT`, `-P`, `kolmo train`, `kolmo profile`,
  `kolmo a|x|l` and the two-positional `kolmo IN OUT` / `kolmo -d IN OUT`. The old analysis option `-q` now means
  `--quiet` and cannot be detected.
- **Bindings:** Python `compress(data, level=3, threads=0, memlimit=None, block=0, ...)` with `level` 1..9, a name or
  `"ultra"`; Rust `Level` (`Level::new(1..=9)`, `FASTEST`, `LIGHT`, `BALANCED`, `COMPACT`, `MAX`, `ULTRA`,
  `Default` = light) with `compress_limited`/`decompress_limited`. `mode` / `Mode` and the dictionary calls are
  removed. Both check the frame (`kolmo_check_frame`) before allocating the output, so a hostile header cannot ask for
  a huge allocation, and take an optional bound on the decoded size (Python `max_output_size`, Rust
  `decompress_limited(..., Some(bytes))`), advisable for untrusted input. Tests cross-check that the CLI, Python and Rust give the same bytes at every level.

Upgrading from 0.0.1:

| 0.0.1 | 0.0.2 |
|---|---|
| `-m speed` | `-1` / `-F` |
| `-m balanced` (default) | `-3` / `-L` (default) |
| `-m eco` | `-5` / `-B` |
| `-m hc` | `-7` / `-C` |
| `-m ratio` | `-9` / `-X` |
| `-m max -l 1` | `-8` |
| `-x` | `-U` |
| `-x -l 1`, `-x -l 2` | `-U --codec-level 1`, `-U --codec-level 2` |
| `-m rt` | `-1 --analysis full` |
| `-q` / `-Q` / `-n` (analysis) | `--analysis quick` / `minimal` / `none` |
| `-l N`, `-B MiB` | `--codec-level N`, `--block-size MiB` |
| `-s`, `-S N`, `-r OFF:LEN` | `--stream`, `--stream=N`, `--range OFF:LEN` |
| `kolmo in out` | `kolmo in -o out` |
| `kolmo -d in out` | `kolmo -d in -o out` |
| `kolmo a`, `kolmo x`, `kolmo l` | `kolmo archive`, `kolmo extract`, `kolmo contents` |
| Python `mode="speed"` / `"balanced"` / `"ratio"` | `level=1` / `3` / `9` |
| Rust `Mode::SPEED` / `BALANCED` / `RATIO` | `Level::FASTEST` / `LIGHT` / `MAX` |

### Decoder hardening and library

From five independent reviews of 0.0.1 (memory safety, denial of service, API). No valid frame decodes differently.

- **Memory safety:** query/index threads clamped to 64 (a stack array overflowed); table v1 row counts bounded before
  allocation; KLZMA rejects a wrapped repeat distance and checks its end bound at every symbol; QLFC rejects ranks
  past its MTF table; khuf uses size_t stream offsets and bounds stream lengths; the log decoder bounds its column
  count; the Y4M header parse stays inside the header line.
- **Denial of service:** CM, GR6 and kimg stop decoding 8 bytes past the end of their input (a forged 6 KB frame cost
  17 s of CPU); graph leaves are bounded together like one stream (a forged block reserved 329 MiB, now 13 MiB); IDX1
  block flags must chain; table formula checks are capped.
- **Frames:** `kolmo_original_size` and `kolmo_check_frame` check the header and index (CRCs, block sizes) before
  any allocation (`kolmo_check_frame` takes an optional bound on the decoded size); a forged header's size claim is no longer
  returned. The decoders take exactly one frame: trailing bytes other than container extensions are
  `KOLMO_ERR_CORRUPT` (concatenations used to be truncated silently). The new `kolmo_frame_size` walks concatenated
  frames. New errors: `KOLMO_ERR_PARAM` (-15) for an out-of-range mode, level or transforms value, `KOLMO_ERR_RANGE`
  for a range read past the end; stream read/write failures are `KOLMO_ERR_IO`.
- **Threads:** after a threaded call, a forked child no longer hangs (`pthread_atfork` resets the worker pool and the
  memory governor). `kolmo_release_buffers` frees the per-thread buffers of the caller and the idle workers.
- **Tests:** a frozen corpus of 84 version-12 frames decoded in `make check`; hardening tests under ASan/UBSan, which
  now also instrument the native coders; fuzzing with version-12 seeds, a native-coder target and timed campaigns in CI.

### Performance

- **Threads inside a block:** the match finders of levels 4-9 (KLZH2's binary tree, KLZMA's binary tree and hash chain)
  split their work by hash bucket across threads, with output identical at any thread count. On 10 cores (Apple M5,
  21 held-out files): level 5 19.1 -> 30.5 MiB/s, level 7 6.7 -> 13.0, level 8 22.6 -> 32.7, level 9 11.8 -> 22.0.
  Compress memory at 10 threads rises by up to about 60 MB. One-thread speed and all decoders are unchanged.
- **Level 9:** block sorting is now tried on every binary block LZMA2 codes (it was tried only where LZMA2 reached 35%
  or less), and the full LZMA2 pass is skipped when a slice already shows block sorting ahead. On the 21 held-out files
  level 9 is 2.2% smaller and compresses 1.8x faster at one thread; only two raw videos change.
- **khuf:** the four-stream Huffman encoder codes its streams interleaved and in place (same bytes).

### Documentation

- Package READMEs (PyPI, crates.io) rewritten for the Kolmo-native line; `zstd` removed from the PyPI keywords.
- README: results re-stated for 0.0.x (21 held-out files, Apple M5, one thread; the 1.x tables are in
  `benchmark/BENCHMARKS.md`, marked as 1.x); build, install, status and layout sections corrected; a tested recipe to
  convert 1.x files.
- SPEC: the decode working set listed per native codec. Design notes written before 0.0.1 carry a banner mapping the
  old coder names to the native ones.
- New CI check, `tests/check_release_docs.py`: release-facing files must not describe removed libraries, switches or
  1.x versions as current, and every version label must match `VERSION`.

## 0.0.1 (2026-11-22)

First release of the renumbered, Kolmo-native line; supersedes 1.0.x. Same code and output as 0.0.1-rc1 below, plus:

- `kolmo -V` / `kolmo --version` prints the release and the container version.
- Homebrew formula: `version_scheme 1`, so `brew upgrade` moves 1.0.1 installs to 0.0.1.

## 0.0.1-rc1 (first release candidate of the renumbered, Kolmo-native line; branch `review/rc4-native-J62kQD`; not tagged)

Renumbered: the 1.x line (1.0.0 and 1.0.1 on PyPI, crates.io and Homebrew) used library codecs. 0.0.1 is the first line built only from Kolmo's own codecs, with a new frame format (container version 12) that does not read 1.x files.

- **No third-party codec library:** zstd, Brotli, LZ4, liblzma and xxHash are removed from the code, the build, both
  bindings and the tree (`vendor/`, `bindings/*/native/vendor`). Kolmo's own coders replace them: KLZ (speed),
  KLZH2 (balanced level 3, eco level 7, hc level 9), KLZMA (ratio's LZ coder and the -x trials), khuf (Huffman
  leaves), `kolmo_khash64` (every checksum and hash).
- **Format: container version 12.** Every frame is version 12; the decoder reads only version 12 and refuses
  versions 1-11 with the new `KOLMO_ERR_LEGACY` (-13). **Files written by earlier Kolmo versions cannot be read.**
  Codec ids 1-4, 6, 13 and 22 are retired.
- **Removed features:** dictionaries (`-D`, `-t`, `kolmo_train_dict`, `kolmo_*_dict` with a dictionary) and trained
  profiles (`kolmo train`, `kolmo profile`, `-P`, the `kolmo_*profile*` calls, CZC1 frames). The calls remain and
  return the new `KOLMO_ERR_UNSUPPORTED` (-14).
- **Planner:** the size probes use KLZH2 (level 1; level 3 where zstd -3 was used) instead of zstd and LZ4; the
  thresholds were not retuned. The zstd wide-window and hc 128 MiB-window paths are gone.

## 1.2.0-rc4 (release candidate; not tagged)

Built from C2 (`review/c2-integration-J62kQD`, 25bedda8) plus owner decisions A6 and A8 of the RC3 review (2026-11-21).

- **Format:** container v11 is written only when a native codec id is actually emitted. Plain frames written with KOLMO_NATIVE set are again readable by 1.1.0 and rc2 (finding 6).
- **New `-x` generation (W4 cm):** CM model nibble 9, with explicit refusal of reserved nibbles (SPEC.md). About 2× faster encode and decode, peak memory about 226 MiB instead of 3 GiB, output about 0.3% larger. Older decoders refuse these frames cleanly.
- **New block codec 31, kimg (lossless 8-bit images, stream v2):** used by ratio and ultra. About 14% smaller than rc3 on images, within about 3% of JPEG XL lossless (-e7). Image decode is about 0.7× of rc3's. Decoders before rc4 refuse kimg frames cleanly.
- **Encoder tuning (W4 docs):** balanced uses an 8 MiB window; hc text uses level 19 with a 128 MiB window; table prefetch. Default output bytes change for balanced and hc. The asynchronous decode writer is not included, so -T1 stays single-threaded.
- **Eco:** text block sorting (BWT) is off by default (KOLMO_ECO_TEXT_BWT=0, owner decision A6), so eco keeps its fast text decode. On the test corpus, eco text output is byte-identical to rc3 again.
- **Memory:** brotli encoder buffers of 1 MiB and above are mmapped, cutting eco encode peak from about 1.08 GB to about 160–230 MiB with identical bytes.

## Unreleased (rc3 wave 3, decmap lane: `claude/rc3w3-decmap-20261118`)

Decode maps: prebuilt decoder tables stored in the frame were measured and rejected (Huffman tables cost 4-8 KiB per
block for at most 8% of decode; more inverse-BWT sample points stay inside the noise; GR6+ and CM searches are under
5% of their decoders). Three decoder-only changes are kept. No format change, no frame changes. Measurements in
`benchmark/rc3w3/decmap/RESULTS.md`.

### Changed
- **x86 AVX2 word-transform decoder** (`src/arch/x86_64/word_avx2.c`, chosen by CPUID; the counterpart of the NEON
  kernel): text `eco`/`hc` decode +14.8% (1 thread) / +12.1% (4 threads), JSON +13.7% / +12.1% on the VM.
- **khuf (codec 27) builds one decode table instead of two**, with bit reversal from a constant table: khuf decode
  +7.6% to +8.9% on Silesia; the fixed cost of a 4 KiB block drops by 20%.
- **KLZH (codec 29) checks its codes in one pass and refills the bit buffer only when needed**: decode +10% to +17%.
  With `KOLMO_NATIVE=all`, `balanced`/`eco`/`hc` frames decode +16.3% (1 thread) / +14.2% (4 threads) faster.
## Unreleased (rc3 wave 3, phrase lane: `claude/rc3w3-phrase-20261118`)

Research only: no change to the library, the format or any frame. Measurements in `benchmark/rc3w3/phrase/RESULTS.md`.

### Research
- **Multi-item decisions (one coded decision for a whole phrase) measured and not adopted.** A deterministic per-file
  phrase dictionary (Re-Pair-style pair merging over word tokens) was tried in three ways: phrases substituted by
  codes before context mixing; phrase ids as extra CM contexts with the dictionary sent; adaptive phrase contexts
  learnt by the decoder too. A one-flag-per-row decision was tried in GR6+. Against `-x` no group gains the 0.5%
  required: JSON −0.38% (one catalogue file −14%), logs −0.09%, prose, XML and tables 0% (substitution is +0.9% to
  +140% there). The adaptive context gives −0.14% to −0.39% on 1–10 MB of prose and −0.02% on enwik8 100 MB. The GR6+
  row flag gives −0.07% to +0.29%. Study tools in `benchmark/rc3w3/phrase/` (`phr`, `g6census`, `bench.py`).
- Found on the way: codec 7's word hash applies the ASCII case fold to bytes ≥ 128, merging UTF-8 bytes. Folding
  ASCII only makes a Japanese JSON file 4.3% smaller. Recorded as a candidate for the CM owner, not changed (it would
  change `-x` frames).

## Unreleased (wave 4, BWT lane: `claude/w4-bwt-20261118`)

Block-sorting pipeline of `ratio` (codec 18) and codec 5. **No format change:** every frame is byte-identical to rc3.
Measurements: `benchmark/w4/bwt/RESULTS.md`.

### Changed
- **Huge pages for the suffix array and the inverse BWT's index array** (arrays of 4 MiB or more, Linux
  `MADV_HUGEPAGE`; `KOLMO_NO_HUGEPAGE=1` turns it off). enwik8 inverse BWT +33% on 1 thread and +40% on 4; suffix
  sort about +15% on 1 thread.
- **Codec 18 works in 4n instead of 5n+.** LZP and the BWT run in the caller's output slot, QLFC codes into the freed
  suffix array, and the decoder inverts in place in its output. enwik8 `ratio` compress peak RSS 664 → 600 MiB;
  enwik9 1857 → 1740 MiB (1 thread) and 3888 → 3546 MiB (4 threads), decompress 788 → 666 MiB (1 thread).
- **Faster LZP encoder** (four positions per 8-byte load), with output identical to the previous loop: 17/96 on
  enwik8 253 → 357+ MB/s, on dickens 216 → 580+ MB/s.
- `ratio` no longer runs the word transform on text blocks whose codec discards it (10% of `ratio`'s encode time on
  dickens; same frames).
- Result: `ratio` on 1 thread, enwik8 compress 10.1 → 11.0 MB/s, enwik9 9.1 → 12.1 MB/s, dickens 10.4 → 11.5 MB/s;
  on 4 threads, enwik8 16.6 → 19.2 MB/s and decode 47 → 61 MB/s.
## Unreleased (wave 4, CM lane: `claude/w4-cm-20261118`)

`-x` context mixing rebuilt for speed and memory: codec 7's **model 4** (`benchmark/w4/cm/RESULTS.md`). Format 2,
container version 11; frames of earlier models still decode. `-x` frames change (codec 7, codec 25 parts, `-x -l 2`
graph leaves); every other mode is byte-identical.

### Added
- **Codec 7 model 4 (stream model nibble 9).** Model 3's contexts restricted to a mask carried in the stream (17 for
  text, logs, CSV and JSON, 15 for other data, chosen by backward elimination on 1 MB samples); 16-byte nibble buckets
  of 15 one-byte bit-history states, four per 64-byte line behind a 4-byte check word (one cache line per context and
  nibble, a quarter of model 3's memory per bucket); two inputs per context (state map by state and bit position, and
  the state's run); a 16-bit mixer (`kolmo_cm_dot16`/`kolmo_cm_train16`: C reference, SSE4.1, AVX2; byte-identical,
  `tests/test_kernels.c`); mixer and APM rows set when first used; tables of any size (`n / 32` lines per context up
  to 48 MiB, `n / 64` to 128 MiB, `n / 128` beyond); a match table that follows the stream and a match chain of the last
  2^24 positions; its own shared priors (`cm_prior4`). Format in `SPEC.md`, *Model 4*. VM, one thread, against zpaq
  -m5 (full files): Silesia (its 12 files) -7.1%, structured -55%, numeric -22%, text -4.7% (xml -10.6%, reymont -8.6%, enwik8 -5.0%, webster -4.1%, dickens -0.7%), logs -14.0%,
  CSV -46% and -60%, x-ray -2.9%, mr -11.0%, sao -45.6%, nci -15.6%, ooffice -5.0%, samba -3.8%, osdb -1.6%, mozilla
  +1.2%; compression 0.98-3.3x and decompression 0.97-9.8x zpaq's speed; peak memory 0.48-1.58x zpaq's (was 5-7 GB on
  every file of 4 MB or more).
- `tests/test_cm4.c` (`make check-cm4`, in `check-rc`): model-4 round trips, the same stream at every kernel level,
  the declared decode working set, damaged headers and streams under AddressSanitizer.
- `benchmark/w4/cm/`: `cmx` (codes one file with one model, times both ways), `cmprior4` (trains the priors),
  `summarize.py`.

### Changed
- **`-x` writes model 4** for codec 7 streams and codec-25 parts (level 1 keeps model 1). Model 4 is not raced (the
  race gained it 0.05-0.15% for twice the encode time of a 1 MB stream at one thread); it takes one preset.
- **The planner's `-x` transform check codes 256 KiB samples with model 4** (was 1 MiB with model 2): the same
  decision on every file measured, a third of the time on 1 MB files; a transform that does not apply to the sample
  leaves the block plain (a 1 MB log slice was 9% larger when it kept the transform untested).
- Context mixing, every model: the match model compares 8 bytes at a time and prefetches its hash slot one bit
  ahead; model 4 hashes only the contexts it uses. Models 1-3 write the same bytes as before.

## Unreleased (rc3 wave 2, regress lane: `claude/rc3w2-regress-20261117`)

Fixes for the regressions of rc3 against rc2 measured on the Apple M5 (`benchmark/rc3/mac/RESULTS.md`). Measurements
in `benchmark/rc3w2/regress/RESULTS.md`. No format change: every frame written here was already valid rc3 output.
Frames change only in `balanced`, on files whose graph leaves take codecs 21-23.

### Changed
- **`balanced` tries the format-2 graph leaves (codecs 21-23) only where a cheap estimate says they can win.** Trying
  them on every leaf made `balanced` 7-10x slower than rc2 on DEC64 float arrays and wide CSVs (and 2-6x on small
  integer files). Now: the Huffman-then-zstd leaf on leaves up to 192 KiB, and on larger leaves only where zstd -3
  reaches less than 30% of the order-0 entropy (above 1.5% of the raw size); its zstd pass is probed at level 1 and
  runs at level 5 (was 19) only when the probe already wins; the zero-run leaf on leaves at most 1/8 non-zero (was
  1/2); inside per-plane and zero-run leaves, Huffman is skipped when the block codec is already below Huffman's
  bound. `eco`, `hc` and above keep the full trials. VM, 1 thread: OWID energy (NPY, pickle) 9.3-9.7x -> 1.3-1.5x
  rc2's compress time, keeping 67% of the format-2 gain (-3.6% instead of -5.4% vs rc2); wide CSV 7.0x -> 1.15x,
  keeping 78% (-3.5% instead of -4.5%); the small integer files keep all of it (year -34%, timestamps -24%,
  population -2.6%) at rc2's speed within a few milliseconds. `balanced` frames of such files change.
- **GR6+ (codec 26, `-x`) is tried only on tables:** at least 90% of the first 256 lines with the same number of
  fields, 3 to 100. Prose, logs and JSON (never chosen; +3% to +11% `-x` time) and wider tables (+126% on a 130-column
  CSV) skip it. The formula search stops after 2^29 rows scored (a count, not a time: same output everywhere); the
  files where GR6+ wins need at most 0.34 G. `-x` frames are unchanged on every file measured; energy_test.csv
  compresses 1.8x faster, dickens 11% faster.
- **GR6+'s formula search runs on the block's spare threads**: target columns in parallel, merged in order (the
  work cap replayed in order), so the formulas and the frame are the same for every thread count. `-x -T 4` on the
  OWID CO2 tables 1.4-2.9 s faster.
- **Video (codec 20) codes and decodes its frames on all threads**, with the SIMD strip kernels: the encoder's
  prediction runs frames in parallel, the decoder rebuilds frames as a wavefront (each frame one strip behind the
  previous). Same bytes for every thread count and ISA.
- `ratio` keeps adaptive QLFC (owner's decision). Static QLFC where adaptive gains under 1% was measured (decode
  1.35-1.9x faster on the three files it applies to, but two thirds of their adaptive gain given back and +1% to +33%
  compress time on every file) and left off; `KOLMO_BWT_STATIC_PCT=1` turns it on. `ratio` frames are unchanged.
- The zero-run split skips zero words 8 bytes at a time (same bytes).
## Unreleased (rc3, memdyn lane: `claude/rc3w2-memdyn-20261117`)

The memory model of `docs/MEMORY-MODEL.md`, dynamic as the owner approved it: **available memory changes speed and
peak memory, never the output bytes.** Every frame changes in its header (byte 11 and the CRC); `-x` frames also change
in their payloads (scaled tables). Measurements: `benchmark/rc3w2/memdyn/RESULTS.md`.

### Added
- **The frame declares its decode working set** (header byte 11, `memd`: 2^(v/8) MiB, rounded up; `SPEC.md`,
  *Decode working set*). Decoders check it before allocating anything: the index against it, it against the memory
  limit, every block and every context-mixing model against it. Frames written before rc3 (byte 11 = 0) are held to
  the limit instead. **The 8 GiB stream case of fuzzing campaign 2 is refused without any limit**: the stream decoder
  checks each frame whole before reserving output and decodes it in windows of whole blocks; stored blocks must be
  exactly their size.
- **One memory limit for encoding and decoding**: `-M SIZE` / `--memlimit SIZE` (`512M`, `8G`, `50%`, `auto`, `max`),
  `KOLMO_MEMLIMIT`; API `kolmo_params.mem_limit`, `kolmo_decode_limits.max_memory`, `kolmo_set_memlimit`,
  `kolmo_mem_parse`, `kolmo_mem_floor`, `kolmo_mem_auto`, `kolmo_decode_memory`, `kolmo_check_frame`, error
  `KOLMO_ERR_MEMORY` (-12). auto = 50% of physical memory or of the cgroup's limit. A frame that needs more stops with
  `needs X to decode (limit Y); run with --memlimit NG or --memlimit max`. An explicit limit below the mode's floor
  (speed and balanced 64 MiB, eco and hc 256 MiB, ratio 1 GiB, -x 256 MiB) is refused. An explicit limit is a format
  decision: ratio and -x use smaller blocks and -x smaller tables so the output decodes within it; auto and the free
  memory of the moment never are.
- **The memory governor** (`src/core/memory.c`): budget = min(limit, available memory + what Kolmo holds), re-read at
  most every 100 ms (Linux `MemAvailable`, cgroup v2 and v1, PSI; macOS `host_statistics64` and the memory-pressure
  source), lowered at once on pressure and raised after 1 s of calm. Block jobs of the encoder and the decoder reserve
  their working set and wait when it does not fit (one always runs); concurrent `-x` trials, codec-25 parts and the
  planner's second sample start only when their reservation fits, otherwise they run afterwards with the same result.
  `KOLMO_MEM_FORCE_BUDGET` forces the budget (tests); `benchmark/determinism/check.py --budgets` checks that frames are
  identical under 64 MiB, 512 MiB, 4 GiB and unlimited.
- `make check-memdyn` (in `check-rc`): declaration coding, hysteresis and pressure, a budget lowered mid-run (jobs fall
  to one and recover, same frame), forced budgets in every mode, forged frames refused unallocated, explicit caps and
  floors.

### Changed
- **`-x` model tables are sized to the block by default** (`--x-tables=scaled`: one bucket per 4 input bytes, 2^16 to
  2^21 per model; was opt-in). `--x-tables=full` (`kolmo_set_x_tables(0)`) restores rc2's sizing (2^22). Decoding a
  10 MB file with `-x` needs about 2.5 GiB instead of 5.4 GiB, at +0.25% on dickens and +0.8% on enwik8 against full
  tables. The first rule (tables within 16× the stream, ~200 MiB) cost +1.7% to +5.1% on 1–10 MB text (+2.6% on the
  text group, +2.4% on Silesia) and was replaced.
- The context-mixing coders' private memory budget (60% of RAM in `cm.c`) is replaced by the governor.
- Bindings: `kolmo_params` gained `mem_limit` (Python `_Params`, Rust `Params`); `native/src` resynced.
## Unreleased (rc3 wave 2, router lane: `claude/rc3w2-router-20261117`)

### Changed
- **`-x` tries its alternatives where they can win.** Every block is now also coded by block sorting (binary blocks
  too; beside context mixing when the block has a spare thread), and LZMA2 is coded in full when a 1 MiB slice comes
  within 25% of context mixing's rate (it had to be 3% below, and slices code worse than whole blocks). The smallest
  coding is kept, so frames change only where an alternative is smaller: on 56 public files (25 input types) two
  changed, Silesia osdb −0.97% (now block sorting, which also decodes 50× faster) and OWID energy CSV −0.98% (table +
  LZMA2); `-x` is no longer larger than `-m ratio` on any of them. Determinism set: `stamps.i64` at `-x` and `-x -l 2`
  (28,524 → 28,410 bytes, version 3 → 11). `check-cmpar`'s codec-25 halves case uses a new
  input, since block sorting now wins on the old one. Compression at one thread takes up to 6% longer (about 2% on text), at four threads no measurable change (`benchmark/rc3/router/RESULTS.md`).

### Fixed
- **`kolmo calibrate` wrote tables it could not read back** when a point measured under 0.05 MB/s (`-x` on a small
  corpus on a loaded machine): rates were printed with one decimal, so they read as 0.0, which the table parser
  refuses. Rates under 0.1 MB/s now keep four decimals (at least 0.0001). Seen as an intermittent `check-profile`
  failure under `make -j4 check-rc`.

### Added
- `docs/ROUTING-MAP.md`: every input type (tables, JSON, logs, prose, code, markup, arrays, weights, images, audio,
  video, FASTQ/FASTA, executables, archives, compressed data) × every level: detection signal, transform and codec,
  measured ratio and speed against the leaders, and the gaps. `docs/ROUTING-PLAN.md`: what to build next, in order,
  with expected gains (colour transform for RGB images first: −30% residual entropy, estimated).
- `benchmark/rc3/router/`: the corpus builder, the matrix runner (every level, general tools, FLAC, FFV1, JPEG XL,
  JPEG-LS), the frame route reader, and the evaluation of the 6-bit study's GR6-L3 (`lzp` phrases and level race; not
  ported: no `-x` output would change).
## Unreleased (rc3 wave 2, meta lane: `claude/rc3w2-meta-20261117`)

Structure read from the input's own headers (`docs/METADATA.md`, `benchmark/rc3w2/meta/RESULTS.md`). Decisions use
the bytes only, never a file name. Frames of other inputs are unchanged (determinism check: 122 frames identical).

### Added
- **Image headers** (binary PGM/PPM, PAM, BMP, uncompressed TIFF, as NPY before): the IMG8 node takes the width,
  channels and pixel offset from the header instead of a lag search, and such an image is never taken for text.
  −9% to −18% at `balanced`, −11% to −30% at `hc` on photographs with uncommon widths or misdetected rows.
- **Executables** (ELF, Mach-O thin and universal, PE): code sections and data are planned apart (x86 ELF −0.3% to
  −5.4%); ARM64 code takes the new **ARM64 branch filter** (BL and ADRP made position-absolute; plan type 11,
  tflag 14, container version 11): −4.9% to −7.7% `balanced`, −6.8% to −8.8% `hc`, −6.8% to −9.5% `max`.
- **AIFF** 16-bit (and AIFF-C `NONE`/`twos`): coded by the audio codec (19) like WAV, samples byte-swapped (codec 19
  plan prefix `0, 1`, version 11): −17% to −45%.
- **Archives**: tar members of 64 KiB or more with a known header get their own plan and are cut by their own format
  (mixed tar −3.6% to −4.5%); zip / NPZ stored members are planned apart, so NPY headers give dtype and shape (mixed
  NPZ −20% to −24%).
- **Packed content** (gzip, xz, zstd, bzip2, LZ4, 7z, RAR, PNG, JPEG, WebP, JPEG XL, MP4/HEIF/AVIF, FLAC, Ogg, MP3,
  Matroska, deflated zip members): no analysis, and blocks a probe finds incompressible are stored without running
  the coder: `hc` 16-46× and `max` 50-140× faster to compress on such files, same size.
- **safetensors / GGUF**: the header and each run of one tensor type are planned apart (`hc` 11.1 s → 1.0 s on a
  checkpoint mixing fp16 and uint8 tensors; sizes within 0.05%, one-dtype files unchanged).
- `make check-meta`: every parser on 3000 mutations of 18 synthetic files under ASan/UBSan, ARM64 filter bijection,
  frame round trips (part of `check-rc`).

### Changed
- `src/transforms/tar.c` moved into `src/plan/meta.c` (tar segmentation is one of the header readers).
## Unreleased (rc3 wave 2, twopass lane: `claude/rc3w2-twopass-20261117`)

Reading the whole file first: what pays and what does not (`docs/TWOPASS.md`).

### Added
- **Content segments** (balanced, eco, hc, ratio): a file that holds several kinds of data (prose, a CSV export, an
  executable, an array of doubles) gets one plan per homogeneous run of 1 MiB or more, as tar members already did.
  4.2–13.3% smaller on such files; files of one kind are unchanged (identical frames). No format change.
- **Per-stream learning settings and a shared prior for context mixing** (`-x`; format 2, container version 11):
  four presets, each starting from mixer weights pre-trained once and compiled in, are raced on the first 256 KiB of
  each CM stream up to 16 MiB and the smallest is kept (longer streams take the prior alone). 1.6–5.4% smaller at
  100 KB, 0.1–2.2% at 1 MB, 0.1–0.8% at 10 MB; decoding costs the same; encoding 100 KB–1 MB files is 2–4× slower at
  1 thread, 1.1–1.4× at 4.

### Fixed
- `-x` on numeric arrays: the plain context-mixing trial of a graph block was gated by a block-sorting slice that
  missed the cases where it wins; 10 MB of float64 values were 33.6% larger than needed (and larger than `ratio`).

## Unreleased (rc3, features-a lane: `claude/rc3-features-a-20261116`)

### Added
- **Trained profiles: `kolmo train`, `kolmo -P`.** A versioned, checksummed profile file (`KPF1`, `SPEC.md`) holds the
  setting, pinned plans per kind of file, a zstd dictionary and a raw dictionary (Brotli, LZ4), and reserves a section
  for context-mixing priors. Frames written with a profile carry its id (container version 11, header flag bit 2);
  one-block frames up to 1 MiB use a compact form (`CZC1`, 14–21 bytes of header), and their zstd leaves up to
  64 KiB are raw zstd blocks. Training measures on a held-out fifth of the samples to choose the dictionary trainer
  (zstd's fastCover or COVER), the largest leaf that takes the dictionary and, with `--target`, the setting. API:
  `kolmo_profile_train`, `kolmo_profile_load`, `kolmo_compress_profile`, `kolmo_decompress_profile`,
  `kolmo_frame_profile_id`; error `KOLMO_ERR_PROFILE`.
- **`--target`** (`size`, `decode>=R`, `encode>=R`, `cost:<formula>`, `cost:cloud[=months]`, `cost:link=R`): the
  setting of each 16 MiB chunk is chosen by trial compression and a calibrated speed table; chunk frames are joined
  into one ordinary frame. **`kolmo calibrate`** measures the table on this machine. API: `kolmo_target_parse`,
  `kolmo_compress_target`, `kolmo_calibrate`; error `KOLMO_ERR_TARGET`.

## Unreleased (on 1.2.0-rc2: branches `claude/kfam-fastq-20261116`, `claude/kbin-20261116`, `claude/simd-dispatch-20261116`)

The SIMD and build changes below change no output and no format (measurements in `benchmark/rc2/SIMD-ASM.md`). The
FASTQ family blocks add container version 10 for `-x` frames of FASTQ input only.

### Changed
- **x86-64 runtime ISA dispatch.** Release builds and wheels compile for baseline x86-64, which compiled out the AVX2
  decimal formatter (table decode), the SSE4.2 CRC32C and xxHash's AVX2 kernel. These are now chosen by CPUID at
  first use: numeric CSV decode (`-m balanced` = zstd 3 + table transform) is 19–23% faster in a generic build.
  `-DKOLMO_NO_DISPATCH` restores compile-time selection.
- **Integer inverse for 32-bit arrays** (all modes' integer graph, order 1/2, factor 1): SSE2 (baseline) and AVX2
  (dispatched) prefix-sum loops. `-m balanced` decode of an int32 array is 18% faster. Same bytes.
- **No floating-point contraction in any codec.** `-ffp-contract=off` now applies to every C/C++ file, including the
  vendored codecs and the rc2 research codecs, in the Makefile, the Python build and the Rust build. With `-mfma`,
  `-march=native` or on arm64, the Brotli encoder (`-m eco` = Brotli 8) no longer uses FMA in its cost model.
  Generic x86-64 machine code is unchanged.
- **Rust crate builds for the target's baseline ISA** (was `-march=native`); `KOLMO_NATIVE=1` opts back in.
  `bindings/*/native/` carry the dispatch code.

### Fixed
- **JSON lines and logs with fixed-width 17- or 18-digit numbers did not decode** (`balanced`, `eco`, `hc`, `ratio`; since
  after 1.0.1, in rc2). The decoder's fast path wrote the high part of such a run with an 8-digit formatter; the block
  checksum rejected the result (no silent corruption). Decoder-only fix; output of the encoder is unchanged, so files
  written by rc2 decode correctly now. Regression test for widths 9 to 19 in `tests/kolmotest.c`.
- **Tables of short numbers keep the table transform (container version 11).** Two faults turned it off on long-form
  tables such as daily counts per country (`Date,Country,Confirmed,Recovered,Deaths`, 5.5 MB). (1) The transformed
  table is a few percent larger than the CSV before coding, and a transform output above block + 16 KiB was refused, so
  every block of about 400 KiB or more was stored untransformed; table blocks may now use block + 1/8 + 16 KiB, and
  only files that need it are version 11. (2) The group (entity) column was taken only if some sampled row differed from
  the row above it, which long groups rarely show; it now only has to take more than one value across the sample.
  countries-aggregated.csv: `balanced` 4.16× → 15.94×, `ratio` 5.18× → 17.85×, `-x` 15.68× → 18.87× (xz -9e 8.42×).
  Ported from a separate session's fix on `main` (branch `claude/nice-faraday-7c4fjq`).
- `kfam` (rc2 research codec): `make kfam-check` failed with LeakSanitizer reports (the command line never freed its
  buffers). A damaged frame could make the decoder decode zeros from an exhausted stream up to the stream's claimed
  length. The family decoder (`src/kfam.inc`, also used for codec-16 blocks in `kolmo`) now reports a stream corrupt
  once it has read 8 bytes past its end; a valid stream never reads past its end. Damaged-frame test added to
  `kfam-check`.

### Added
- **GR6+ blocks at `-x`** (block codec 26, container version 11). In `ultra` (levels 0 and 2) a text block of at
  least 64 lines is also coded by GR6+: fields as one decision each (same as the line above, column and graph
  move-to-front ranks, column dictionary, numbers as residuals from the best of four column predictors including a
  derived-column formula, new text with a prefix shared with a reference field), values as 6-bit units, an integer
  context-mixing model and the CM arithmetic coder. Formula discovery adds running totals `lag(t) + a`, copies and
  products `a * b / 10^k` to the ratio and growth formulas, scored on near hits. The block keeps GR6+ only when it is
  smaller than context mixing, LZMA2 and BWT, so no file grows. Apple M5, one thread, against rc3 `-x`: covid
  countries-aggregated 18.87× → 19.48×, owid_test 12.23× → 12.70×, OWID CO2 12.84× → 12.93×, and those decode 5–11×
  faster; energy, wide table, loghub, dickens and api.json unchanged. Compression costs 3–30% more on text, and
  126% more on a 130-column table where GR6+ is tried and loses (formula search). Frames with a codec 26 block are
  version 11; every other mode and `-x -l 1` keep their bytes. From branch `claude/dazzling-maxwell-p8y4u0` (commit
  `12f0c0e`, where it was codec 15 / version 9); code `src/codecs/gr6.c`, format `SPEC.md` *GR6+ blocks*,
  measurements `benchmark/rc3/gr6/RESULTS.md`.
- **FASTQ family blocks at `-x`** (block codec 16, container version 10): at `ultra` levels 0 and 2, a block that
  is complete FASTQ text is also coded by the family coder (`src/kfam.inc`: layout, names, bases, base exceptions
  and qualities, each with a model of its own, cut into chunks coded in parallel), and that coding is kept when it
  is smaller. On three runs (SRR835775, SRR2584863, SRR1770413) it is 16.6 MB against 22.2 MB for `-x` before.
  Only such frames are version 10, so readers up to 1.2.0-rc2 refuse them at the header ("corrupt or unsupported
  input"); every other mode, `-x -l 1`, and every input that is not FASTQ keep their bytes (35 bench files compared
  at `-m balanced`, `-m hc`, `-m ratio` and `-x -l 1`). `make check-fqfam` (part of `check-rc`).
- `build/kfam`: new quality model (fqzcomp-style contexts, two mixers, a second-layer mixer, two APMs), new base
  model (orders 6 and 12 with forward, reverse-complement and local match models), faster-adapting name model,
  chunked jobs; frame `KFM2`. Results in `src/experimental/KFAM.md`.
- `make check-dispatch` (part of `check-rc`): CRC32C and XXH3 against scalar references.

## 1.2.0-rc2 (2026-11-15) — release candidate, not tagged, not published

1.2.0-rc1 plus the improvements that no branch carried yet and the research codecs, as opt-in tools.
No default output changes; no format change.

### Changed
- **Faster encoding of integer arrays** (default modes): the integer node's predictor choice evaluates its three
  orders in one sweep and its residual pass skips the division when the common factor is 1 (research P2-E2,
  P2-E9, P2-E30 on PR #7). Same bytes as rc1 (5,181 frame pairs compared with a control build). In memory on one
  thread at `-m balanced`: 1.18x to 2.20x faster compression on integer arrays; other data unchanged.
  `-DKOLMO_FORCE_GENERIC_INT_FWD` builds the old path as a control.
- **`--typed-huf`, 16-bit WAV**: clips with long repeats (a near-free repetition score of at least 0.03) get
  zstd-3 leaves on the residual instead of per-plane Huffman (research P2-E30). rc1 decodes the output.
  ESC-50, 40 clips: 3.1% smaller in total, no clip larger.

### Added (experimental, standalone, `make experimental-check`)
- KRC1, KBC1 and KBX3 research codecs (Codex), KRC1's threaded encode and decode, and its Metal GPU path with
  CPU fallback (`build/krc-auto`).
- `build/kolmo-fastq`: the four-line FASTQ prototype (Codex, 7 November) with a command line of its own.
- `build/kvid`: a lossless video front end prototype for 8-bit y4m (temporal and intra prediction per strip,
  residuals coded by the Kolmo library).
- `build/kfam`: a family-stream coder (FASTQ, CSV, or one stream) with small dedicated models per family and its
  own binary arithmetic coder; streams code in parallel.
None of these is used by `kolmo`, and none writes a Kolmo frame.

### Licence hygiene
- The context-mixing coder's squash table, its inverse and the arithmetic coder's renormalisation, which followed
  lpaq1 (GPL), are rewritten from their mathematical definitions. The bitstream is unchanged: 246 frames
  byte-identical over `-x`, `-x -l 1` and `-x -l 2`, both directions of cross-decoding checked.

## 1.2.0-rc1 (2026-11-14) — release candidate, not tagged, not published

One build that carries the best of the two integration branches: `claude/integration-20261113` (the base) and
`integration/best-features-rc-20261113` (PR #10). What came from where, what was left out and why:
`integration/rc1/FEATURES.json`; what was verified: `integration/rc1/STATUS.md`.

### Merged since 1.1.0
- **Planner record-ownership fix** (PR #6).
- **DEC64** (graph node 7): binary64 values that are short decimals are stored as decimal digits and exponents;
  dictionary columns; a typed-first order in the balanced planner (branch `claude/dec64`).
- **`ultra`: image model (block codec 17) and CM model 3** for text (branch `claude/dec64`).
- **Native plane-context leaf**, on request: `--native-leaf` / `KOLMO_T_NATIVE_LEAF` (PR #9; container version 9
  only when such a leaf is written).
- **`ultra` level 2**, on request: `-x -l 2`, context mixing on graph leaves (PR #8).
- **Fast integer inverse** in the decoder, checked against the generic inverse (144 of 144 cases); no format change.
- **Typed Huffman route**, on request: `--typed-huf` / `KOLMO_T_TYPED_HUF` (PR #10); a speed option.

### From the comparison with PR #10
- A compile-time control build for the fast inverse, `-DKOLMO_FORCE_GENERIC_INT_INV`, and the conformance test built
  three ways (sanitizers, optimised, control): `make check-fastint`.
- `make check-rc`: one target for the component tests of the merged features; CI runs it under ASan and UBSan.
- CI: leak detection and halt-on-error in the sanitizer and fuzz jobs; `fuzz_nplane` joins the fuzz job; the fast
  inverse and the opt-in routes are tested in the release build on all three runners.
- A feature manifest and status record under `integration/rc1/`.

### Format
- No change to any byte this build writes relative to `claude/integration-20261113`.
- `SPEC.md` has a new section, *Identifiers and versions*: every container version, codec id, node id and CM model,
  the first reader of each, and what older readers do. It also corrects three stale lines (the graph block is codec
  12, not 8; the highest readable version; the current version).
- Open decision, not taken here: block codec 17, graph node 7 and CM model 3 are written without raising the
  container version, as the graph section of `SPEC.md` prescribes for new identifiers. Readers 1.0.1 and 1.1.0
  refuse such frames with "corrupt or unsupported input" and return no data. Raising the version (to 10) would make
  them refuse at the header instead.

### Not in this build
- `--csv-archive` from PR #10 (never smaller than `eco`; default balanced is smaller and decodes faster on the same
  tables since DEC64).

## 1.1.0 (2026-11-10)

### Ratio
- **`ultra`: context-mixing model 2.** Two sparse contexts, three record contexts (a detected fixed record length:
  the bytes one and two records back, and the column within the record), four mixers combined by a learned final
  mixer whose learning rate decays over the block, and an order-2 APM. Silesia under lzbench: 17.91% → **17.01%**
  (5.58× → 5.88×); zpaq -5 is 19.09%. enwik8: 19.57 MB → 18.96 MB (5.27×). Files written by 1.0.0 (model 1)
  still decode.
- **`ultra -l 1`**: model 1 only, without the LZMA2/BWT alternatives: 17.99% at 0.43 / 0.45 MB/s on Silesia —
  ahead of zpaq -5 (19.09%, 0.39 / 0.39 MB/s) on ratio and on both speeds.
- **`max`**: binary blocks keep the better of LZMA2 and BWT (Silesia 19.68% → 19.34%).
- **`max -l 1`**: BWT for every block — 20.93% at 21.6 MB/s on Silesia, ahead of bsc, kanzi -7 and bzip3 -5 on
  ratio, compression and decompression speed.
- **JSON**: JSON lines and pretty-printed JSON are detected; keys and punctuation form templates and every value
  becomes a typed column (OWID data as JSON lines: −51% at `fast`, −30% at `max`). Same block format as the log
  transform, so 1.0.0 decodes these files. **Minified JSON** (one line of records) is split into records first
  (block flag 10, container version 6): OWID records as one JSON array, −51% at `fast`, −37% at `max`.
- **Integer arrays (kolmo graph INT node, branch `claude/graph`)**: 16/32/64-bit arrays, with or without a `.npy`
  header, are coded as residuals: per block the order (value, delta, delta-of-delta), signedness, common divisor and
  output width are chosen, and the residuals are stored as byte planes. Millisecond timestamps (int64): 10.6× →
  24.5× at `fast` (9.3 MB/s compress+decompress) and 27.1× at `max`; OpenZL 0.3.0 reaches 20.6× at 5.2 MB/s.
  Population counts (int64): 3.80× at `fast` (12.8 MB/s) vs OpenZL's 3.27× (10.8 MB/s). Other inputs are unchanged
  (51 reference outputs byte-identical).

- **Float arrays (graph FLOAT node + Huffman-only leaves)**: float32 words are rotated so the exponent fills one byte,
  and each byte plane becomes its own leaf, coded by Huffman alone when that is smallest (the near-random mantissa
  planes are stored). Model weights (safetensors, ONNX, GGUF): 1.17× → 1.20× at `fast`, level with OpenZL 0.3.0.
- **Merge of Codex's `codex/lossless-frontier`**: blockwise file I/O, lower fast-file memory, legacy-compatible
  wide-table lanes and the opt-in frontier build. Codex's frontier codecs keep ids 8–11; the graph codec is 12.

- **8-bit images (graph IMG8 node)**: LOCO-I median prediction per channel, row length from a `.npy` header or detected
  among common widths × 1/3/4 channels. img_camera 1.62× → 1.82× at `fast`, 1.97× at `max` (OpenZL 0.3.0: 1.75×);
  astronaut 1.63× → 1.84× at `fast`.
- **16-bit floats (bf16/fp16)** in the FLOAT node: a BF16 LLM shard 1.28× → 1.50× at `fast` (its entropy bound).
- **rANS leaf codec (14)** at hc/max/ultra: model weights 1.2007× → 1.2036× (OpenZL's best ratio).
- **Wide tables at hc/max/ultra**: the graph is kept only when it also wins with the setting's strong codec on a slice
  from the middle of the file. Fixes a regression against 1.0.0 on the UCI electricity table at `max` (8.37× → 11.26×,
  equal to 1.0.0).

### Speed
- **Fast tier, per TRADEOFFS.md (unchanged output may not be more than 5% slower than 1.0.1):** graph trials at `fast`
  use 256 KiB and are compared by their gain over plain coding of the same slice; image detection is a lag scan over
  4096 sampled bytes; float detection samples 8192 words; graph leaves are coded from three samples (stored, Huffman
  only or zstd). Unchanged binary files went from up to 40% slower than 1.0.1 to level or faster; model weights are
  now both smaller (1.169× → 1.201×) and faster (+12% combined) than 1.0.1 at `fast`.
- **CLI:** writing over an existing file replaces it atomically (rename) instead of copying the staged output over it
  — decompression onto an existing file is about 30% faster; symlinks and hard-linked files keep the copy.
- Graph decoder: table numbers print straight into rows (no column text), leaf parts are views instead of copies,
  two-digit number printing; minified JSON: newline removal by runs. Minified JSON decodes 1.8× faster, wide tables
  1.65× faster (in memory).
- Fast/hc planner: text-transform trials on 256 KiB instead of 1 MiB (same decisions on every file tested), the log
  trial reuses the JSON trial's result; wide-table column analysis at `fast` samples runs of lines (+0.05% size,
  about 1.3× faster to compress). `json_split` copies runs (1.5× faster, byte-identical).
- **Quick analysis (`-q`, `transforms = 2`)**: only the cheap structure transforms, scored with zstd -1 on a 256 KiB
  slice. `fast -l1 -q` (30.33% at ~350 MB/s) beats zstd -2; `fast -l2 -q` (28.89% at ~300 MB/s) and `fast -q`
  (27.87% at ~220 MB/s) beat zstd -3.
- **Context-mixing tables on transparent huge pages** (Linux): ultra back to 1.0.0's speed with model 2.
- `ultra` tries LZMA2/BWT on a 1 MiB slice before coding a large block with them (same output on Silesia, up to 12%
  faster).
- Log transform: one-pass tokenizer for short tokens.

### Verification
- The full test suite (round trips in every mode, range reads, corruption fuzzing, JSON and minified-JSON cases) and
  Codex's resource and parallel suites pass; the full suite is also clean under AddressSanitizer and UBSan.
- Every change that is not meant to alter output was checked against 51 reference outputs (byte-identical).

### Format
- CM streams flag model 2 in the top nibble of the size word (`f & 8`); 1.0.0 decoders reject them as corrupt input.
  Minified-JSON blocks (tflag 10) raise the container to version 6, which 1.0.0 rejects. Every other output added
  here (JSON columns, `max` choices, quick analysis, `ultra -l 1`) is readable by 1.0.0.
- `max` level 1 now means "BWT for every block" (it was LZMA2 preset 1); `ultra` level 1 selects model 1.

### Memory
- `ultra` (model 2) needs about 4.5 GB at the largest block size (1.0.0: 3.8 GB); `ultra -l 1` keeps 1.0.0's 3.8 GB.

### Benchmarks
- `benchmark/lzbench`: kolmo enters lzbench as levels 0–12, each an ordinary CLI setting; results in
  `benchmark/lzbench/results/`.

### Naming
- **The old `cz` / `czn` name is gone from the source.** `src/czn.c` and `src/czn.h` are now `src/kolmo.c` and
  `src/kolmo.h`; the C API is `kolmo_*` (was `czn_*`) and constants are `KOLMO_*` (was `CZN_*`); the static library
  is `build/libkolmo.a`; tests and the benchmark tool are `kolmotest`, `kolmoparallel`, `kolmoresource` and
  `kolmobench`. This breaks C callers of the 1.0.x `czn_*` names; the CLI, the Python package and the Rust crate
  keep their interfaces. The on-disk magic bytes (`CZN1`, `CZNS`) are unchanged, so every existing file still
  decodes and output is byte-identical to before the rename.

## Unreleased (on main, not tagged)

- **Budget rule and a typed order in the balanced planner (branch `claude/dec64`).** A change is now judged by
  whether a mode's output gets smaller while its cost stays within its competitor's (zstd -3 for `speed` and
  `balanced`, OpenZL for `hc` and `ratio`): `benchmark/accept/budget.py`, rule and status in
  `benchmark/accept/TARGETS.md`. First step in the planner: at `balanced` the typed trials for binary data (float,
  integer, decimal, 8-bit image) run before the record search, and the search is skipped when a typed plan already
  saves a fifth. Planning time over 79 files falls 29% (24.5% to 20.8% of encode); one of 83 files changes (3.7%
  smaller).
- **DEC64 sampling skips NaN and infinity (branch `claude/dec64`).** A missing value was tried against all sixteen
  exponents before being counted as not decimal. Output identical on 83 files at `balanced`, `hc -l 9` and `ratio`;
  encode instructions -5.3% on a 127-column table and -10.7% on one with many missing values.
- **DEC64: dictionary columns (branch `claude/dec64`).** A decimal or integer column of few distinct values is coded
  as indices into its sorted values (column flag bit 5; the values go to their own stream, layout bit 1). At
  `balanced` it is taken when the index saves a byte plane and the order-0 size of its planes is a tenth smaller. At
  `hc` and `ratio` a one-byte index is also tried against the residuals by coding up to 16,384 rows both ways with
  the coder that follows, because a column of repeated values keeps its repeats as an index and loses them as
  differences. massdata: -5.7% at `balanced`, -6.2% at `hc`, -6.4% at `ratio`, in every container; the ozl tables
  -1.5 to -4.2% at `hc` and `ratio`; skl_digits -2.8% at `hc`; no file larger; the large decimal tables are
  unchanged and cost 2% more instructions to encode. First item from the research on basic transforms that stack
  (`research/transforms-20261113`); the second, a byte delta inside float planes, was measured on nine float files
  and not taken (0.0% at best on seven of them).
- **`ratio`: float and decimal arrays code their planes with block sorting (branch `claude/dec64`).** The budget rule
  applied to the largest single cost found in `ratio`: LZMA2 at its extreme preset over the byte planes. On float
  arrays (model weights) block sorting gives the same size four to six times faster (a 10 MB safetensors file 5.4 to
  0.8 s). On decimal arrays LZMA2 was 10 to 12% smaller at 25 to 30 times the encode time (a 24 MB table: 8.0 s for
  1.26 MB against 0.3 s for 1.40 MB), which is 18 KB saved per CPU second where the cost model breaks even at 54
  (data kept a year) or 652 (30 days). With block sorting that table costs less than OpenZL (CPU and storage) and
  is 23% smaller; with LZMA2 it cost 3.3 times OpenZL. Decimal files at `ratio` are 2 to 12.6% larger than at the
  previous commit and still 5 to 43% smaller than with 1.0.1 (eight files checked); `-x` is unchanged. Integer arrays keep LZMA2.
  Both are kept: `ratio -l 10` (`-m max -l 10`) keeps LZMA2 on decimal planes and gives the earlier sizes.
  `hc`: float planes at zstd 9 instead of 19 (same size, up to 16 times faster); decimal planes keep zstd 19 (zstd 12
  was 3 to 36% larger).
- **More encode time off `balanced`, output unchanged (branch `claude/dec64`).** Measured by instruction counts
  (callgrind) and CPU time; every change below leaves all 83 test files byte-identical at `balanced`.
  - A plan marked "check each block" coded every block twice at the fast settings (the graph and plain zstd). The
    plain pass now runs only when three 64 KiB slices coded plain come within 35% of the graph's rate: a 31 MB
    decimal table goes from 185 to 109 ms.
  - Delimited tables (graph path): fields are found eight bytes at a time, numbers are read in one pass, and tag and
    scale streams are reserved once per block: a 316-column table 54 to 48 ms.
  - Planner: the integer trial looks at four of its sixteen sample runs first and stops a width whose estimate is a
    quarter above plain coding; plane entropies read one byte per value. On mozilla (no plan wins) total
    instructions fall 8.4%.
  - DEC64 writes only the byte planes a column reaches.
  - Planner: the 8-bit image trial runs first at `balanced`, and an image layout that saves a fifth ends the typed
    trials (the integer trial was a quarter of the encode of a 0.8 MB photograph): instructions on that file -41%.
- **Graph leaves are coded faster at `balanced` (branch `claude/dec64`).** The byte count that decides whether a
  leaf tries Huffman used one table, which stalls on runs of one byte value (most of a number plane); it now uses
  four. Huffman is not tried where it cannot win: a prefix code spends at least one bit per byte, and at `balanced`
  a leaf whose samples put Huffman a tenth behind zstd skips it. Output is byte-identical on 83 files at
  `balanced`, `hc -l 9` and `ratio -l 1`. Encode CPU on decimal arrays: 118 to 75 ms (24 MB), 323 to 205 ms (31 MB);
  a 316-column table 87 to 76 ms. With the planner order below, `balanced` encode CPU over 79 files is 13% lower
  (5,506 to 4,793 ms).
- **DEC64: second difference (branch `claude/dec64`).** A column or segment may store the difference of successive
  deltas (flag bit 4), chosen when it is 5% shorter again: running totals and steady trends. A 31 MB table with
  cumulative columns (`co2.npy`, float64 or float32): 10.6% smaller at `balanced`, 11.4% at `ratio`; other tables
  within 0.7%.
- **Graph leaves: the Huffman-only shortcut no longer misfires on sparse leaves (branch `claude/dec64`).** At
  `balanced`, three 32 KiB samples decide whether a large leaf is coded with Huffman alone. When all three fall in the
  busy part of a leaf that is mostly one byte value, Huffman was chosen and spent one bit per byte on the rest (found
  by the E02C research harness on another branch: 164 KB against 3 KB on one plane). Both coders are now tried when
  one value fills half the leaf. A 31 MB decimal table (`co2.npy`): 2,397,150 → 1,963,547 bytes (18.1% smaller); nine
  other decimal files 1.5 to 3.5% smaller; no other output in the 81-file set changed.
- **Planner: the record-plan leak is fixed the way `fix/planner-record-ownership` fixes it** (one owner, freed at the
  end), replacing this branch's site-by-site frees, and the image model's block codec moves from 15 to 17, so the two
  branches and `feat/native-residual-leaf` (graph leaf codec 15) can be brought together without clashes.
- **Decimal mapping for float32 (branch `claude/dec64`).** The DEC64 node takes 4-byte words (a per-column flag):
  binary32 values that were written in decimal are stored as the integer `m` with value = `(float)(m / 10^e)`.
  `.npy` `<f4` tables and float32 arrays inside containers. Four holdout tables converted to float32: `balanced`
  5.63 MB → 2.59 MB, `ratio` 3.79 MB → 2.11 MB (per file 3 to 58% smaller). Float64 output is unchanged.
- **`ultra` model 3: structure contexts and a better match choice.** Five more contexts for text: the key a value
  belongs to (`"key":`, `key=`, the tag after `<`) and the field of a delimited line with the byte above it in the same
  field; the match model now takes, of the last 16 places a context was seen, the one that agrees for longest. On
  top of the first model 3: CSV 4.4% smaller, JSON 2.4%, XML 2.3%, logs 2.0%. Against model 2 in all: JSON 6.7%, CSV
  5.4%, XML and logs 4.7%, Polish prose 3.4%, English prose 0.3%, at about 35% more time. Model 3 uses about 0.8 GB
  more memory than model 2, plus 4 bytes per input byte.
- **`ultra`: context-mixing model 3 for text (branch `claude/dec64`).** Model 2 plus four contexts (orders 12 and 20,
  and the byte the match model expects with orders 1 and 2), finer length classes for long matches, and a match that
  carries on past a one-byte difference. Used for text blocks; binary blocks keep model 2, where model 3 gained
  nothing (-2.2% to +0.06% on seven binary files). JSON 142,217 → 135,936 bytes (4.4% smaller), Polish prose 3.4%,
  logs 2.8%, XML 2.5%, CSV and source code about 1%; about 20% slower than model 2. Streams carry an extended header
  (flags 8, then model and table size); an older reader stops with an error.
- **`ultra`: an image model for the context mixer (block codec 17; branch `claude/dec64`).** For 8-bit images the
  mixer's hashed contexts are the pixel's neighbours (left, above, the diagonals, two back), the earlier channels of
  the same pixel, and gradient, median and inter-channel predictions with the local activity, in place of the
  preceding bytes; the mixers are selected by the prediction and the neighbours. A 512x512 grey photograph: 132,883 →
  116,664 bytes (paq8px -8: 113,869); a 512x512 RGB photograph: 404,896 → 323,467 (paq8px -8: 308,539). Encode about
  150 KB/s, as the mixer elsewhere. Files that use it need this version; an older reader stops with an error.
  `make check-cmimg` round-trips generated images and decodes damaged streams under AddressSanitizer.
- **`ratio` and `ultra`: image graphs try block-sorted leaves; `ultra` tries a transformed block plain.** Colour
  photographs are 5 to 6% smaller at `ratio` with block-sorted leaves in the image graph. At `ultra` a transformed
  block is also coded plain when plain block sorting is within 2% of it, which removes a case where `ultra` was
  larger than `ratio` (a 1 MB image array: 276,549 → 203,747; `ratio` 245,200).
- **Integer columns inside containers (branch `claude/dec64`).** The array finder of the DEC64 node also takes runs
  of small int64 values (a per-segment flag), and a whole-file numeric plan that the probes mark gives way to the
  segment plan. The int64 pickle that regressed on main: 129,908 → 98,236 bytes at `balanced` (1.0.1: 129,908), 76,398
  → 71,804 at `ratio`; a mixed pickle 8.5% smaller. Other outputs within 0.1%.
- **Regression ledger (`benchmark/accept/REGRESSIONS.md`, branch `claude/dec64`).** Every file of the corpus and
  the holdout sets measured against 1.0.1 and main at three settings: what got slower or larger, what was fixed, what
  is open, and which costs are accepted for what gain. From it: the plain attempts at `ratio` are now made only when
  a block-sorted slice says they can win (ratio encode over 81 files: 384 s → 228 s, main 262 s), and DEC64 must
  beat the next plan by 7%.
- **`ratio`: a transformed block is also tried plain (branch `claude/dec64`).** Transforms are chosen by their zstd
  score; under block sorting or LZMA2 the plain bytes are sometimes smaller. A transformed block now tries block
  sorting of the plain bytes when a slice from the middle is within reach, blocks up to 2 MiB also try plain LZMA2,
  and a plain result is kept when it is at least 3% smaller. Silesia `nci`: 1,533,927 → 1,225,386 bytes (20%
  smaller; its decode goes from about 120 to 67 MB/s, the block-sorting rate). Silesia tar: 40,996,509 → 40,687,968
  (19.34% → 19.20%), encode time unchanged, decode about 10% slower on the cloud VM. A 1 MB image array was 16%
  smaller. On 81 corpus and holdout files plain coding now beats the `ratio` plan by more than 1% on none (before: 2).
- **Mixed inputs: a numeric plan is checked away from its trial slice (branch `claude/dec64`).** A numeric graph or
  byte shuffle is chosen on one slice from the middle of the input. Four 32 KiB probes elsewhere now test that the
  rest is the same kind of data; where the plan codes a probe over 10% larger than plain coding, each block is coded
  both ways and the smaller kept. Fixes a regression of the `balanced` quick analysis that is on main: a pandas
  pickle of int64 columns followed by strings coded 69% larger than 1.0.1 (219,892 against 129,908 bytes; now
  129,908). A table of full-precision doubles that every version shuffled is 20% smaller at `balanced` and 26% at
  `ratio`. Both files are outside the test corpus; on the 81 corpus and holdout files, plain coding now beats the
  chosen plan by more than 1% on none (before: these two). No corpus output changed.
- **Decimal doubles: a new kolmo graph node (DEC64, node id 7; branch `claude/dec64`, not on main yet).** Binary
  float64 values that were written in decimal (prices, measurements, counts, most CSV data loaded into arrays) are
  stored as the integer `m` with value = `m / 10^e`: one exponent per column, the integers as zigzag byte planes with
  an optional delta, one tag byte per value. A column's most common other pattern (usually NaN for missing) is a tag;
  anything else (NaN payloads, -0.0, full-precision values) is kept verbatim as an exception, so every input round
  trips exactly. A row-major `.npy` table uses its shape for the columns. Any other input is searched for arrays of
  doubles at any byte alignment (a raw dump, a column-major `.npy`, and the columns inside a pandas pickle, an
  uncompressed `.npz` or a MATLAB `.mat` file); each array is cut into segments of 4,096 values that choose their
  own exponent, and everything between the arrays stays as plain bytes. On a holdout set built after the design was fixed (OWID CO2, three OpenZL sample tables, a
  nuclear mass table, scikit-learn digits and a telemetry table, each as `.npy` and as a raw column-major array; 14
  files, 95 MB): `balanced` 11.9 MB → 5.8 MB (every file 25 to 57% smaller), `ratio` 7.2 MB → 4.6 MB (0 to 49%
  smaller; xz -9: 8.3 MB). The three control files without decimal values are byte-identical. On the affected files
  encode takes 1.4 to 2.4 times the CPU and decode 1.3 to 1.8 times (still 120 to 240 MB/s and 420 to 770 MB/s on the
  cloud VM); other files pay one 64-value sample. At `ratio` and `ultra` the plan is scored with zstd, so each such
  block is also coded plainly and the smaller result kept. Files that use the node need this version or later; an
  older reader stops with an error at the unknown node. The inverse checks every tag and count before it writes
  (`make check-dec64`: round trips with awkward values and 59,000 damaged payloads under AddressSanitizer).
  A second holdout of containers (the same tables as pandas pickle, `.npz` and `.mat`; 12 files, 123 MB):
  `balanced` 15.5 MB → 7.8 MB, `ratio` 8.9 MB → 6.6 MB, no file larger (xz -9: 10.6 MB). Not covered yet: integer
  arrays inside containers (a pickle of int64 columns is unchanged), and doubles interleaved with other fields.
- **Line classes: a new transform for fixed-width text (block flag 12, container version 8).** Text whose lines come
  in a few fixed lengths (chemistry and geometry files, fixed-width reports and dumps) is stored column by column
  per line length. `balanced` takes it where it codes a trial slice at least as small as the template transform; it
  costs a line scan and a transpose instead of tokens and templates. Silesia `nci` at `balanced`: 2,038,889 →
  2,027,737 bytes, encode 254 → 92 ms, decode 57 → 36 ms, peak memory 54 → 14 MB. Files that use it carry version 8
  and need this version or later to decode; every other file is written as before (version 3 to 7). The decoder
  checks every length and count (`make check-lines`: round trips and 51,200 damaged inputs under AddressSanitizer).
- **`balanced` plans binary data with the quick analysis.** Trial slices of 256 KiB scored at zstd level 1, shuffle
  trials only at the byte width whose planes are most predictable and only where one is, the remaining layout trials
  skipped once a 16-bit image layout saves a fifth, and the x86 check first on code. The text trials and the integer
  trial are unchanged. Sizes on the 132-file test corpus are unchanged except one weights file (0.02% smaller).
- **Template transform: 25% less encode time on repetitive logs.** A line with the same shape as the last line of its
  length (digits in the same places, every other byte equal) reuses that line's tokens and template; runs of spaces
  are not scanned; the template table starts small. Output is byte-identical.
- Silesia as one tar, one thread, in memory, same process as zstd 1.5.7 (cloud VM, best of seven): `balanced` goes
  from 124 to 176 MB/s round trip (encode 146 → 221, decode 851 → 867) at 27.48% of the input; zstd -3 is 189 MB/s
  at 31.20%. The 11.9% size advantage is kept and the round trip is at 93% of zstd -3, up from 66%. `fast -l 2` is
  8.7% smaller than zstd -3 and 6% faster on the round trip (200 MB/s). What remains for `balanced` is planning time
  (about 130 ms on 212 MB) and the inverse transforms on decode; the LZ pass is zstd's in both.
- **Three modes.** `-m speed`, `-m balanced` (default) and `-m ratio`; `-x` selects the smallest output of all
  (`ultra`). The older names (`rt`, `fast`, `eco`, `hc`, `max`, `ultra`) remain as advanced settings.
- **`max` no longer gives up decode speed for almost nothing.** Block sorting replaces LZMA2 on a binary block only
  when it is at least 3% smaller, and is not tried at all on x86 code or where LZMA2 is above 35% of the input
  (where it lost in every test). One weights file had dropped from 508 to 29 MB/s decode for a 0.2% gain.
- **Cheaper planning.** Byte histograms in the 16-bit image detector use four counters (same result, a third less
  time); integer and image graph trials are screened on a short run first; the strong-codec check for wide tables
  uses 384 KiB.
- `benchmark/accept/`: release targets and the script that checks a results file against them.
- **Quick analysis (`-q`) is quick again.** Record layouts are searched only when the record period is clear, and the
  integer and image graph trials are skipped or shortened unless a `.npy` header names the type. Planning time on a
  browser tarball falls from 331 to about 100 ms; sizes on the test files are unchanged.
- **Faster command line at every size.** The file path with reused block buffers and staged output, which `fast`
  used for inputs of 8 MiB and more, now serves `speed` and `balanced` compression of any size and decompression in
  every mode. Output bytes are unchanged. On the Linux test machine: `speed` encodes 14 to 62% and decodes 33 to 73%
  faster; `balanced` encodes 7 to 17% and decodes 18 to 29% faster on files under 8 MiB.
- **Planning skips two searches that never paid off.** Record layouts are searched only when the record period
  stands at least 4x above the median lag, and the x86 transform is tried only where call/jump opcodes are at least
  1 in 1000 bytes. Outputs are byte-identical across the test corpus; planning time at `balanced` falls from 435 to
  about 150 ms on a browser tarball and by a third on record and database files.
- **Speed mode is speed first.** `-m speed` now uses minimal analysis (`-Q`): only the byte-plane shuffle is tried,
  and text is coded as it is. The table, record and image transforms, which made some files 2 to 4x slower to encode,
  stay in `balanced` and in the advanced `rt` setting. `-Q` also works with `fast`.
- **`max` on wide tables encodes ten times faster.** Column leaves take BWT like other text at `max`; LZMA2 on the
  leaves was 3% smaller and ran at about a tenth of the speed. Against 1.0.1 the file is 32% smaller and faster in
  both directions.
- **Speed mode plans in a fraction of a millisecond.** The entropy screen runs before any coder is set up, and a
  shuffle is skipped where LZ4 finds the input incompressible (it would trade most of LZ4's pass-through speed for
  ratio; `balanced` still takes that trade).
- **`hc` keeps its fast reads on model weights.** rANS leaves, which decode about four times slower than Huffman, are
  taken with the zstd family only when at least 3% smaller. Weights grow 0.24 to 0.35% and decode 40 to 80% faster.
- **Small arrays never lose to 1.0.1.** For blocks under 256 KiB the block's own coder on the plain bytes is compared
  with the graph, and the smaller is kept (a 93 KB integer file at `eco` had grown from 660 to 717 bytes).
- **`max` skips hopeless block-sorting attempts on large blocks.** A slice from the middle is coded both ways first;
  outputs are unchanged on the test corpus and a 21 MB source tarball encodes 9% faster.
- **`max` on minified JSON encodes nine times faster.** The record streams go straight to BWT, which had been winning
  over LZMA2 anyway; the LZMA2 pass is no longer run first. Output is unchanged.
- Smaller planning costs at the slow settings: the wide-table trial uses half a MiB from `eco` up, block sorting is
  not attempted on typed table columns at `max`, and the command-line file path applies from 8 MiB as in 1.0.1.
  Outputs are unchanged on the test corpus.
- `max`: JSON token streams (one record per line or minified) go straight to BWT, and block sorting is not attempted
  on log token streams, where it lost by 4 to 12%. JSON lines encode nine times faster at `max`; outputs unchanged.
- The minified-JSON trial runs only when the first records share a few key layouts, so a nested JSON document no
  longer pays for a trial it cannot win.
- Speed mode asks for stronger evidence before trying a shuffle, which cuts its planning on tarballs by two thirds.
- macOS builds link Homebrew's liblzma statically when the archive is present (less start-up time per run).
