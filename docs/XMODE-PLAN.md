# `-x` for files of every size: which technique serves which file

Status: rc3 xmode lane, 16 November 2026. Measurements: `benchmark/rc3/xmode/RESULTS.md` (cloud VM, x86-64, 4 vCPU,
15.7 GiB). The Apple M5 numbers are to be repeated by the integration lane.

## Why `-x` was slow and big on files under 128 MiB

`-x` codes each block (up to 128 MiB) with one context-mixing (CM) model on one thread, at 0.2–0.3 MB/s on the VM
(0.4–1 MB/s on the M5). Three things made it worse than it had to be:

1. **Everything ran in series.** A block was coded by CM, then by LZMA2 or block sorting, then (for a transformed
   block) by block sorting of the plain bytes, then by the FASTQ family coder; the planner coded two 1 MiB CM samples
   one after the other. With one block per file, three of four cores sat idle.
2. **Trials that never win.** On FASTQ the family coder (codec 16) always wins, but the CM trial ran first: 20× the
   family coder's time and 1.4–5.4 GiB of memory.
3. **Tables sized for 128 MiB.** The hashed tables grow with the input up to 2^22 buckets per model (5.4 GiB in all),
   reached from 4 MiB. A 10 MB text needed 5.4 GiB to compress and to decompress.

## Techniques, in the plan's order

| # | technique | output | helps | where it matters |
|---|---|---|---|---|
| 1 | trials on their own threads (`src/core/trials.c`); the planner's two CM samples in parallel | unchanged | encode, threads > blocks | every file under 128 MiB with `-T` ≥ 2; 1–10 MB text and CSV |
| 2 | FASTQ: the family coder first, CM and the other trials skipped when it applies (blocks ≥ 64 KiB) | unchanged | encode, all thread counts | FASTQ of every size: ~20× faster, memory 1.4–5.4 GiB → 30–150 MiB |
| 3 | prefetch of the next bit's mixer and APM rows; AVX2 build of the CM loops chosen at run time | unchanged | encode and decode, every size | under 1 MiB too: 20–25% faster on the VM; a further 21% on baseline-ISA (release) builds |
| 4 | codec 25, columns: a table is transposed and each column gets its own CM model, coded and decoded in parallel | new codec, version 11 | encode and decode | CSV/TSV from 64 KiB: smaller *and* up to 4× faster at 4 threads |
| 5 | codec 25, halves: a non-text single stream from 32 MiB is coded as two halves with half-size tables | new codec | encode and decode | non-text blocks ≥ 32 MiB: 2× faster for 0.14% (100 MB binary); 5 MB halves cost 0.14–2.3%, so smaller blocks stay whole |
| 6 | family coder: smaller quality and name chunks (see RESULTS) | changed chunking | decode | FASTQ ≥ 4 MiB |
| 7 | tables sized to the block (`--x-tables=scaled`, the default since the memory model was approved; `--x-tables=full` for rc2's sizing) | smaller tables | memory, small-file speed | every size: decode memory from GiB to the block's ~16× |

What was measured and **not** adopted:

- Hand-written AVX2 mixer kernels: 2% (noise) over the compiler's AVX2 code, so no kernel file under `src/arch/`
  (`docs/LAYOUT.md`: a kernel lands only with a gain above noise). The profile shows why: the mixer is 11% of the
  instructions, the counter and state-map updates of 30 models 45%, and memory latency the rest.
- A byte-ahead prefetch of the next byte's hashed buckets (both candidates, one bit early): no gain.
- Splitting text into parts: +2.8% (2 parts), +6.4% (4), +10.9% (8) on 10 MB of text. Text stays one model.
- Splitting binary data into 4 or 8 parts: +0.9% and +1.3% on 100 MB, over the 0.5% budget.
- A split that depends on `-T`: rejected, because the frame must be the same for every thread count (the determinism
  check compares 1 and 4 threads). Every cut in codec 25 depends on the input only.

## By file size

- **Under 1 MiB.** One block, one CM model; the planner's samples are the whole file. What helps: 3 (faster per
  byte), 1 (the planner's samples and the alternative coder overlap CM), 4 for tables (the planner's column trial is
  the block itself and is reused, not coded twice), 2 for FASTQ, 7 for memory (a 100 KB file needs 280 MiB today).
- **1–32 MiB.** Same, plus 4 (columns decode in parallel). Halves (5) are not used: on 5 MB halves the cost was up
  to 2.3% (Silesia osdb).
- **32–128 MiB.** One block still: 3, 4 and 5 carry it; text remains one model on one thread (the size cost of
  splitting text is 3–11%), so plain text above 10 MB is where `-x` stays slowest. A shared static prior (each part
  warmed on the same leading bytes) is the next candidate there; it was not measured in this lane.
- **Over 128 MiB.** Several blocks, one per thread, as before; 2–5 still apply inside each block.

## Memory

Peak memory falls with 2 (FASTQ), 4 and 5 (tables per part follow the part's size) and 7. Step 1 raises the encoder's
peak at 4 threads (the alternative coder and a second planner sample run beside CM). The opt-in table sizing of 7 is
the lever the memory model (`docs/MEMORY-MODEL.md`) needs. Measured (RESULTS): "scaled" (about 16× the block) costs
0–0.2% on tables, 0.3–2.4% on binary data and up to 5.1% on text (10 MB), and brings decoding of a 10 MB file from
5.4 GiB to 135–210 MiB; on text a cap of 2^20 buckets per model costs 0.67% at 1.4 GiB. The owner's choice is
between these per kind of data. The owner approved the dynamic memory model (`docs/MEMORY-MODEL.md`) and the memdyn lane
made scaled tables the default (`benchmark/rc3w2/memdyn/RESULTS.md` repeats the size cost on this build).

## Bug found by the measurements

Before `937ef77` the halves path wrote plain non-text blocks of 8 MiB or more as codec 7 with a codec-25 payload
(undecodable). Fixed, with `make check-cmpar` covering both codec-25 paths.
