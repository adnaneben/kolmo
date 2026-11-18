# rc3 wave 2, memdyn lane: the dynamic memory model

Branch `claude/rc3w2-memdyn-20261117`, on `claude/release-rc3-20261116` (`1536036`). Specification:
`docs/MEMORY-MODEL.md` (section "Dynamic memory", and "As implemented" for what was built). Format: `SPEC.md`,
*Decode working set (header byte 11)*.

**The hard rule holds: available memory changes speed and peak memory, never the output bytes.** Every frame below is
identical under forced budgets of 64 MiB, 512 MiB, 4 GiB and unlimited (same SHA-256 in every row of a setting), and
`benchmark/determinism/check.py --budgets 64M,512M,4G,unlimited` passes on all 122 frames (12 settings × 10 inputs,
1 and 4 threads, plus `--typed-huf`).

## Machine and method

- Cloud VM: x86-64 (Sapphire Rapids class), 4 vCPUs, 15.7 GiB, Linux 6.18, gcc 13, `make` defaults
  (`-O3 -march=native`). "All threads" is 4 here. The integration lane repeats the key runs on the Apple M5; the macOS
  sampler (`host_statistics64`, memory-pressure source) was written for it but only built and tested on Linux here.
- Every number is one whole process (`benchmark/rc3w2/memdyn/run.py`): wall time and peak resident memory from
  `wait4`, encode and decode separately, decoded output compared byte for byte, frame SHA-256 recorded.
  `--budget` sets `KOLMO_MEM_FORCE_BUDGET`, `--memlimit` passes `-M`. Raw rows: `xtables.csv`, `budgets.csv`,
  `declared.csv`, `caps.csv` in this directory.
- Inputs (as the xmode lane): `text_*` = Silesia dickens, webster, nci, reymont, xml, dickens concatenated; `csv_*` =
  UCI household electric power consumption (`;`-separated); `bin_*` = Silesia mozilla, samba, ooffice, sao, x-ray, mr;
  the first 100 KB / 1 MB / 10 MB / 100 MB (CSV cut at a line end). `silesia.tar` = the 12 Silesia files concatenated
  (211.9 MB).
- The `-x` table runs overlapped the test suites on the same 4 vCPUs: their **sizes and memory are exact, their
  times indicative** (±15%). The budget, declared and cap runs ran on an otherwise idle machine.

## 1. `-x` tables sized to the block (now the default)

`-x -T 4`, full tables (`--x-tables=full`, rc2's sizing) → scaled (the new default). "declared" is header byte 11 (what
one block needs to decode), "decode" and "encode" are measured peaks (MiB), times in seconds.

| file | full (bytes) | scaled (bytes) | size cost | declared MiB | decode peak MiB | encode peak MiB | encode s | decode s |
|---|---:|---:|---:|---|---|---|---|---|
| text_100k | 28,061 | 28,080 | +0.07% | 304 → 166 | 277 → 161 | 281 → 165 | 0.625 → 0.474 | 0.382 → 0.484 |
| csv_100k | 6,604 | 6,604 | +0.00% | 304 → 166 | 277 → 161 | 651 → 491 | 0.794 → 0.718 | 0.302 → 0.306 |
| bin_100k | 14,276 | 14,291 | +0.11% | 215 → 140 | 205 → 124 | 207 → 127 | 0.309 → 0.267 | 0.234 → 0.231 |
| text_1m | 216,874 | 220,470 | +1.66% | 1448 → 181 | 1377 → 165 | 2357 → 278 | 8.332 → 6.788 | 3.743 → 4.112 |
| csv_1m | 51,395 | 51,395 | +0.00% | 395 → 140 | 1114 → 489 | 2770 → 498 | 4.542 → 3.649 | 1.316 → 1.272 |
| bin_1m | 622,662 | 624,302 | +0.26% | 1218 → 140 | 1166 → 126 | 1178 → 138 | 2.939 → 3.385 | 3.324 → 2.896 |
| text_10m | 2,025,970 | 2,129,407 | +5.11% | 5793 → 235 | 5400 → 210 | 5446 → 286 | 55.231 → 52.656 | 50.311 → 40.545 |
| csv_10m | 449,066 | 449,425 | +0.08% | 2435 → 152 | 6683 → 497 | 6802 → 628 | 23.983 → 18.403 | 14.050 → 10.945 |
| bin_10m | 4,333,900 | 4,379,930 | +1.06% | 4871 → 152 | 6814 → 374 | 4892 → 382 | 34.713 → 26.180 | 30.044 → 26.532 |
| text_100m | 9,749,249 | 9,888,003 | +1.42% | 6317 → 2048 | 5843 → 1859 | 5967 → 1983 | 471.976 → 459.945 | 411.489 → 397.833 |
| csv_100m | 4,110,806 | 4,110,806 | +0.00% | 861 → 861 | 242 → 242 | 8289 → 1515 | 162.469 → 140.157 | 18.930 → 21.051 |
| bin_100m | 25,433,340 | 26,031,309 | +2.35% | 4871 → 235 | 6855 → 533 | 4771 → 697 | 352.203 → 212.463 | 342.519 → 209.155 |
- **Size cost:** tables (CSV) 0.00–0.08%; binary 0.11–2.35%; text 0.07–5.11% (worst at 10 MB, where full tables are
  2^22 buckets per model against a 10 MB stream; at 100 MB, full and scaled tables are both large and the cost falls to
  1.42%). The xmode lane's figures (0–5.1%) are reproduced to the byte on the same inputs.
- **Decode memory saved:** 1.4 GiB → 165 MiB (text 1 MB), 5.4 GiB → 210 MiB (text 10 MB), 6.7 GiB → 497 MiB (CSV
  10 MB, columns in parallel), 6.9 GiB → 533 MiB (binary 100 MB, two halves in parallel), 5.8 → 1.9 GiB (text 100 MB).
  A 10 MB file of any kind now decodes in under 512 MiB at 4 threads, and in 135–211 MiB at one thread (section 3).
- Scaled tables are also faster on most rows (fewer TLB and cache misses): binary 100 MB encodes 1.66× and decodes
  1.64× faster.
- CSV 100 MB is unchanged: its blocks are coded as columns (codec 25) whose parts are small enough that both sizings
  pick the same tables.

## 2. Peak memory and wall time per mode under each budget

`-T 4`, `KOLMO_MEM_FORCE_BUDGET` = the budget (the governor behaves as if that much memory were available). Same frame
in every column of a row (SHA-256 in `budgets.csv`). Encode / decode: seconds, peak MiB.

| input, mode | frame bytes | unlimited | 4 GiB | 512 MiB | 64 MiB |
|---|---:|---|---|---|---|
| silesia.tar, speed | 100,925,313 | 0.14 s 46 / 0.53 s 112 | 0.16 s 45 / 0.29 s 112 | 0.18 s 47 / 0.37 s 112 | 0.32 s 111 / 0.36 s 112 |
| silesia.tar, balanced | 66,488,842 | 0.27 s 38 / 0.53 s 79 | 0.26 s 38 / 0.51 s 79 | 0.29 s 39 / 0.60 s 79 | 0.93 s 77 / 0.61 s 79 |
| silesia.tar, hc | 52,867,567 | 55.7 s 490 / 0.61 s 256 | 58.8 s 490 / 1.07 s 256 | 109.5 s 469 / 0.71 s 256 | 95.9 s 469 / 0.78 s 236 |
| silesia.tar, ratio | 47,962,044 | 6.5 s 1153 / 3.9 s 691 | 8.0 s 1153 / 4.7 s 691 | 12.4 s 795 / 6.6 s 694 | 10.3 s 795 / 6.0 s 694 |
| text_10m, -x | 2,129,407 | 58.6 s 286 / 48.5 s 210 | 63.2 s 287 / 48.8 s 210 | 61.7 s 286 / 48.6 s 210 | 56.3 s 229 / 46.8 s 210 |
| bin_10m, -x | 4,379,930 | 21.9 s 381 / 23.9 s 374 | 24.9 s 381 / 23.5 s 374 | 26.3 s 267 / 25.2 s 374 | 31.8 s 157 / 29.1 s 143 |

Reading it:
- **ratio:** at 4 GiB both 128 MiB blocks are coded at once (1153 MiB, 6.5–8 s); at 512 MiB and 64 MiB one at a time
  (795 MiB: one block needs more than the budget and runs alone, the one-job floor), 1.6–1.9× slower.
- **hc:** at 512 MiB and below fewer of its 16 MiB blocks run at once (55.7 s → 96–110 s). The peak barely falls
  (490 → 469 MiB) because one block of this input dominates: hc gives text segments a 128 MiB window, and that single
  job alone peaks at 368 MiB (one thread, section 3).
- **-x, binary 10 MB** (two halves of codec 25, plus the concurrent LZMA2 trial): at 512 MiB the trial waits and runs
  after context mixing (encode 381 → 267 MiB); at 64 MiB the two halves also run one after the other, encoding and
  decoding (381 → 157 MiB, 374 → 143 MiB), for 1.2–1.45× the time.
- **-x, text 10 MB:** one model on one thread either way; at 64 MiB the block-sorting trial waits (286 → 229 MiB).
- **speed and balanced:** their working sets are a few MiB per thread; peaks are dominated by pages of the mapped
  input and output files (the file path of the CLI from 8 MiB), which is why the 64 MiB row does not fall and can even
  read higher (slower runs keep more mapped pages resident at once). The budget makes them run blocks one at a time
  (0.14 → 0.32 s, 0.27 → 0.93 s).
- Decoders: the governor limits blocks in flight in the same way; decode peaks of speed/balanced/hc/ratio here are
  dominated by the output path, and bin_10m shows the effect (374 → 143 MiB).

## 3. The declared figure against the measured decode peak (one thread)

| input, setting | declared (byte 11) | decode peak | encode peak |
|---|---:|---:|---:|
| silesia.tar, speed | 10 MiB | 112 MiB | 15 MiB |
| silesia.tar, balanced | 12 MiB | 79 MiB | 15 MiB |
| silesia.tar, hc | 279 MiB | 161 MiB | 368 MiB |
| silesia.tar, ratio | 1024 MiB | 615 MiB | 795 MiB |
| text_1m, -x | 181 MiB | 166 MiB | 170 MiB |
| csv_1m, -x | 140 MiB | 126 MiB | 134 MiB |
| bin_1m, -x | 140 MiB | 126 MiB | 128 MiB |
| text_10m, -x | 235 MiB | 211 MiB | 228 MiB |
| csv_10m, -x | 152 MiB | 135 MiB | 173 MiB |

The declaration is a per-block working set: exact for context mixing (its tables follow from the stream header),
a measured bound elsewhere, and it does not count the caller's input and output buffers. For `-x`, which is where
memory matters, the process peak including the CLI's buffers stays within 10% under the declaration. For speed and
balanced the process peak is the CLI's file I/O (the decode working set of a 4 MiB LZ4 or zstd block is ~10 MiB). At
4 threads the decoder may run several units at once when the budget allows (CSV 1 MB: 489 MiB with four column models
in parallel), each reserving its own figure.

## 4. Explicit caps (`-M`): the output decodes within the cap

| input, setting | cap | frame bytes | vs auto | declared | decode peak | encode peak |
|---|---|---:|---:|---:|---:|---:|
| text_10m, -x | 256M | 2,155,555 | +1.23% | 215 MiB | 204 MiB | 372 MiB |
| text_10m, -x | 512M | 2,129,407 | same size | 235 MiB | 210 MiB | 287 MiB |
| bin_10m, -x | 256M | 4,379,930 | same size | 152 MiB | 139 MiB | 161 MiB |
| silesia.tar, ratio | 1G | 45,652,802 | −4.8% | 512 MiB | 467 MiB | 905 MiB |
| silesia.tar, ratio | 2G | 47,962,044 | identical | 1024 MiB | 691 MiB | 795 MiB |

An explicit cap is a format decision (smaller blocks for ratio and `-x`, smaller `-x` tables), so the frame differs from
the auto frame where the cap is binding; a cap that does not bind gives the same frame (ratio at 2 GiB), or the same
size with a different nominal block size in the header (`-x` at 512M and 1G on 10 MB). Every frame decoded within its
cap. Two findings:
- **Encoder estimates are approximate.** `-x` on 10 MB of text under a 256 MiB cap peaked at 372 MiB while encoding
  (the planner's samples and the block-sorting alternative are not fully counted, and one job always runs); decoding
  stayed at 204 MiB. The governor's encode estimates (`kmem_enc_*`) decide only concurrency, so this never changes
  bytes, but a tight `-M` on `-x` is a decode guarantee, not an encode one. Refining the estimates is follow-up work.
- **ratio with 64 MiB blocks (cap 1 GiB) came out 4.8% smaller on silesia.tar, but 7.7× slower to encode** (88.8 s
  against 11.5 s). Not investigated further in this lane; it suggests the per-block BWT/LZMA2 choices of ratio behave
  differently at 64 MiB and is worth a look by the bwt or integration lane.

## 5. Tests

- `make check-memdyn` (part of `check-rc`), `tests/test_memdyn.c`:
  - declaration coding (monotone, never below the need), size parsing (`512M`, `8G`, `1.5GiB`, `50%`, `auto`, `max`);
  - hysteresis with a fake sampler: a drop applies at once, a rise only after 1 s of calm; PSI-style pressure caps the
    budget at what is held and optional work is refused; one job always runs;
  - **budget lowered mid-run**: 48 hc blocks on 4 threads; with the budget unlimited 3 jobs ran at once, after the
    budget was forced to 1 byte the count fell to 1 and stayed there (25 million samples over 1 s), and after it was
    raised it recovered to 3; the frame equals the reference;
  - forced budgets 1 B, 64 MiB, 512 MiB, 4 GiB, unlimited in rt, fast, hc, max, ultra level 1 and ultra, text and binary,
    1 and 4 threads: identical frames; decoding under 64 MiB;
  - **forged frames, refused before allocation**: the fuzzing case (a stream frame claiming 8 GiB) with no limit at all
    (`max`), with a garbage CRC and with a valid CRC (stored blocks must be exactly their size); a context-mixing block
    asking for 2^22-bucket tables (5 GiB) in a frame declaring 16 MiB (no model is made: `cm_models_made` unchanged);
    the same block in an undeclared frame, held to a 1 GiB limit (`KOLMO_ERR_MEMORY`); an index above its declaration;
  - explicit caps: `-x -l 1` under 256 MiB declares 91 MiB and decodes under 256 MiB, fails one byte below its figure;
    caps below the floors of `-x` and ratio are refused; balanced at 64 MiB works.
- Before every push: `make check`, `make check-rc`, `make check-sanitizers`, `make experimental-check GPU_CHECK=`,
  `make kfam-check`, `python3 benchmark/determinism/check.py build/kolmo build/determinism` (and with `--budgets`): all
  pass. `make check-cm-compact` does not link (duplicate `kolmo_set_x_tables`), on the base commit as well; it is not
  in the required list and was left alone.

## 6. Determinism file

`expected.sha256` was regenerated in a commit of its own. All 122 frames change: 96 only in the header (byte 11 and
the header CRC), 26 `-x` frames also in the payload (scaled tables). The list per frame is in
`determinism-changes.txt` (base `1536036` against this branch, one thread).

Correction (18 November): the manifest of `ca0ec66` was written with a stale `build/kolmo` that predated the typed
route's declaration (`aa8d8ec`), so its `walk.wav -m fast --typed-huf` line held that frame with byte 11 = 0
(`133388…`). A clean build gives `7dcee998…` (byte 11 = 29) on x86-64 Linux and, as the integration lane found, on
arm64 macOS; the line is fixed. The frame is in `xplat/walk_typedhuf_linux.k`. No code depends on the platform: the
declaration is a function of the input length only.

## What was not done, or differs from the proposal

- **Lighter mode under a cap** (`--memlimit-strict`): not built. A cap below the mode's floor is refused instead
  (rule 3), so a mode never changes behind the user's back.
- **macOS**: the sampler and the pressure source compile only on Apple targets; they were not built here.
- **Encode estimates** are coarse (section 4). The decode figure is the precise part, because it is what a reader
  must trust.
- The declaration of codec-25 frames is the largest part (a unit of work); several parts in parallel reserve one
  figure each, so a 4-thread decode can exceed the declaration when the budget allows it (never when it does not).
