# Memory model

> **Since 0.0.1 (container version 12)** Kolmo has no library coders. This document was written before that, and its
> coder names map as follows in 0.0.x (`src/codecs/native.c`): LZ4 → KLZ level 1 and LZ4-HC → KLZ level 3 (28); zstd
> level 1-2 / 3-5 / 6-9 / 10-15 / 16-18 / 19+ → KLZH2 (32) level 1 / 3 / 5 / 6 / 8 / 9; Brotli → KLZH2 level 7; LZMA2 →
> KLZMA (24); zstd's Huffman (HUF) → khuf (27); XXH3/XXH64 → `kolmo_khash64`. Dictionaries
> and trained profiles are removed. Figures below were measured with the library coders.

Status: **approved with the owner's changes of 17 November 2026: the model is dynamic** (section "Dynamic memory",
below, takes precedence over the static rules where they differ). Implemented for rc3 by the `memdyn` lane (branch
`claude/rc3w2-memdyn-20261117`); what was built and measured is in the last section, "As implemented", and in
`benchmark/rc3w2/memdyn/RESULTS.md`.

## The problem

1. **Trust.** The unlimited stream decoder reserves a frame's *claimed* original size before checking it. An 8 GiB claim
   becomes an 8 GiB allocation (fuzzing campaign 2, `src/stream.inc:108`).
2. **No user control.** Kolmo has decode limits in the API, but no memory budget for encoding or decoding, and the
   command line has no option for one.
3. **`-x` is memory-hungry regardless of input size.** Measured peak resident memory on the Apple M5 (rc2):

| input | mode | encode, 1 thread | encode, 10 threads | decode, 1 thread | decode, 10 threads |
|---|---|---:|---:|---:|---:|
| dickens, 10 MB | speed | 4 MiB | 21 MiB | 4 MiB | 18 MiB |
| dickens | balanced | 7 MiB | 27 MiB | 4 MiB | 16 MiB |
| dickens | hc | 184 MiB | 184 MiB | 21 MiB | 21 MiB |
| dickens | ratio | 106 MiB | 106 MiB | 66 MiB | 66 MiB |
| dickens | **-x** | **5,555 MiB** | **5,555 MiB** | **4,732 MiB** | **4,732 MiB** |
| FASTQ, 23 MB | -x (family coder wins) | 4,649 MiB | 4,848 MiB | 252 MiB | 447 MiB |
| silesia.tar, 212 MB | balanced | 56 MiB | 163 MiB | 20 MiB | 118 MiB |
| silesia.tar | hc | 570 MiB | 1,626 MiB | 57 MiB | 246 MiB |
| silesia.tar | ratio | 1,247 MiB | 2,255 MiB | 485 MiB | 770 MiB |

`-x` needs 4.7 GiB to decode a 10 MB text, because its hash tables are sized for blocks of up to 128 MiB. A machine
with 4 GiB of RAM cannot open such a file.

## Industry practice

| tool | model |
|---|---|
| xz / liblzma | `--memlimit-compress`, `--memlimit-decompress` (absolute or % of RAM). The encoder lowers threads and then dictionary size to fit; the decoder stops with the exact figure needed. Each stream's header declares the dictionary size, so the need is known before anything is allocated. |
| zstd | The frame header declares the window size. The decoder refuses windows above 128 MiB (`windowLogMax` 27) unless the user passes `--memory=`/`--long`. The encoder's `-T` adapts. |
| 7-Zip | `-mmemuse`; reduces threads to fit. |
| Browsers, HTTP | Hard per-object caps, no override. |

The common pattern: **the format declares the need up front, the user sets a limit (with a sensible automatic
default), and the tool adapts or refuses with a clear message.** Kolmo should follow it.

## Proposal

### Rule 1: never allocate on trust (always on, not configurable)

- Every frame header declares `mem_decode`: the decoder working set per block (model tables, window, scratch) plus the
  largest block's output. The encoder computes it; it is the figure the decoder checks before allocating.
- A claimed original size is checked against the mode's maximum block size and against the frame's declared totals
  before any allocation. Output buffers grow as data is produced (as the frame buffer already does since `9d4496a`),
  so a lying header costs at most one growth step before the checksum or the coder rejects it.
- With rule 1, the 8 GiB case is refused without any cap at all.

### Rule 2: one memory limit, used by encoder and decoder

- CLI: `-M SIZE` / `--memlimit SIZE`. SIZE is bytes with a unit (`512M`, `8G`), a share of RAM (`50%`), `auto`
  (default) or `max` (no limit beyond physical RAM).
  - Environment: `KOLMO_MEMLIMIT`.
  - API: `kolmo_params.mem_limit` and `kolmo_decode_limits.max_memory`, where 0 means auto.
- **auto = 50% of physical RAM**, never below the mode's floor (rule 3). On a 24 GB Mac that is 12 GB; on a 4 GB
  machine, 2 GB.
- **Encoder adapts, in this order, and says so on stderr:**
  1. fewer threads;
  2. smaller blocks;
  3. smaller model tables (`-x`), with the size cost measured;
  4. only with `--memlimit-strict` unset, a lighter mode.

  The output always decodes within the same limit on a machine of the same size.
- **Decoder adapts:** if the frame fits with fewer threads, it uses fewer. If even one thread does not fit, it
  stops with: `needs 4.7 GiB to decode (limit 2.0 GiB); run with --memlimit 5G or --memlimit max`.
- **`max` keeps your flexibility.** Kolmo then uses whatever the machine has, as now, with rule 1 still protecting
  against forged headers.

### Rule 3: floors, the minimum each mode needs

A limit below a mode's floor is rejected for that mode, with the floor in the message. Proposed floors:

| mode | floor | from |
|---|---:|---|
| speed, balanced | 64 MiB | 4 MiB blocks, per-thread buffers |
| eco, hc | 256 MiB | 16 MiB blocks and zstd 19/22 or Brotli windows, one thread |
| ratio | 1 GiB | 128 MiB blocks with BWT or LZMA2, one thread (measured 1.25 GiB encode on Silesia; floor applies with blocks reduced to 64 MiB) |
| `-x` | 256 MiB | **after the change below**: model tables scaled to the block, one thread |

### Change that comes with it: `-x` tables sized to the block

Size the context-mixing tables from the block length, with an upper bound set by the memory limit. For example,
table bits are chosen so that the tables are about 16× the block (minimum 2^20 entries per table). The chosen bits are
already stored in the stream header (`bb`), so the decoder knows the need up front.

Expected effect (to be measured by the xmode lane):
- a 10 MB file decodes in about 200–300 MiB instead of 4.7 GiB;
- a 1 MiB file in under 100 MiB;
- size cost: about 0 on small files, where the large tables are mostly empty.

This also makes `-x` usable on phones, CI runners and 4 GB machines, and leaves room to run several `-x` blocks in
parallel.

## Open questions for the owner

1. Is 50% of RAM the right `auto` default? xz uses 25% for multithreaded decompression. 50% favours speed on a
   workstation; 25% is kinder to shared servers.
2. Is the `-x` floor of 256 MiB acceptable, given that `-x` will then pick smaller tables (and be slightly larger)
   when the limit is low?
3. Should the library default (NULL limits) be `auto` (proposed) or unlimited, as today? `auto` is safer for services
   that decode untrusted input.

## Dynamic memory (owner's direction, 17 November 2026)

The owner asked for a dynamic model: use more memory when that makes Kolmo faster and the memory is free; use less when
the machine gets tight; go back up when memory frees again. This is the right approach, with one rule that makes it
safe.

### The rule: memory changes speed, never bytes

The compressed output must not depend on how much memory happened to be free. Otherwise the same file compresses to
different bytes on different machines or at different moments. That would break the determinism check, reproducible
archives, deduplication, caching by content hash, and trust ("the file I archived yesterday re-encodes identically").

So every decision splits into two classes:

| class | examples | decided by |
|---|---|---|
| **format decisions** (change the bytes) | block size, `-x` table bits per block (`bb`), the codec and transform chosen per block, the split into parts (codec 25) | the input, the mode and level, and an explicit `--memlimit` cap only. Fixed before encoding starts; the same on every machine. Never the free memory of the moment. |
| **execution decisions** (change only speed and peak memory) | threads in flight, how many blocks are coded at once, whether trials run concurrently or one after another, buffer pools, read-ahead, decoder threads | **adapted continuously** to the free memory, as below |

The table sizes of `-x` are a format decision. They follow the block. Since w4, `-x` writes codec 7's model 4, whose
tables hold four 16-byte buckets per 64-byte line: `n / 32` lines per context up to 48 MiB, `n / 64` up to 128 MiB,
`n / 128` beyond (half for the model-2 contexts, a quarter for the structure contexts; the shift is in the stream
header), capped by `--memlimit` if given (`cm4_shift_for`). That is about 26 table bytes per input byte with the text
contexts (60 MiB to decode 1 MB, 350 MiB for 10 MB, about 1.5 GiB for enwik8), about as much as zpaq -m5
(`benchmark/w4/cm/RESULTS.md`). Model 3's rule (rc3 wave 2, still how old streams are read): one 64-byte bucket per 4
input bytes, 2^16 to 2^21 per model, about 0.5 GiB to decode 1 MB, 1.3 GiB for 4 MB, 2.5 GiB from 8 MiB (`cm_bits_for`);
the first wave-2 rule (tables within 16× the block) cost 1.7–5.1% on 1–10 MB text and was replaced. This makes `-x` both lean and deterministic. Bigger tables would make `-x`
smaller, not faster; speed comes from concurrency, which is the dynamic part.

### The governor (src/core/memory.c)

- **A budget, sampled continuously.** It is the smaller of:
  - the user's cap (`--memlimit`, `KOLMO_MEMLIMIT`, the API field; `max` = no cap);
  - the memory the system reports as available:
    - macOS: `host_statistics64` free + inactive + purgeable, plus the `DISPATCH_SOURCE_TYPE_MEMORYPRESSURE` events;
    - Linux: `MemAvailable` from `/proc/meminfo`, the cgroup v2 `memory.max` / `memory.current` and PSI
      `/proc/pressure/memory`;
    - elsewhere: physical RAM / 2.

  It is re-read at most every 100 ms and on every pressure event.
- **Reservations.** Every unit of work, a block job or a trial, reserves its working set before it starts, using its
  `mem_needed` (the codec table already carries the field). It releases it when done. CM already does this with
  `cm_acquire`/`cm_release`; the governor generalises it.
- **Adapting:**
  - when a reservation does not fit, the job waits, so fewer jobs run at once, down to one;
  - one job always runs. That is the floor, so a tight machine slows Kolmo down but never stops it;
  - when memory frees, waiting jobs start again, up to the thread count;
  - hysteresis: a pressure warning lowers the budget at once, and it only grows again after 1 s without pressure,
    to avoid oscillation.
- **Concurrent trials** (xmode's alternative coders, the planner's samples) start only when their reservation fits;
  otherwise they run after the main coder. The result is the same, it just takes longer.
- **Decoder.** The same governor sets how many blocks decode at once. A frame that needs more than the cap even for
  one block fails up front with the figure needed (the static rule 2 above).

### Testing it

- The determinism check runs with forced budgets of 64 MiB, 512 MiB, 4 GiB and unlimited
  (`KOLMO_MEM_FORCE_BUDGET`); all frames must be identical.
- A test lowers the budget mid-run and checks that the job count drops and recovers.
- Peak RSS is measured on the Mac under each budget.

## As implemented (rc3, memdyn lane)

| piece | where | notes |
|---|---|---|
| frame declaration | header byte 11 (`memd`, `SPEC.md` *Decode working set*) | 2^(v/8) MiB, rounded up (9% steps); computed per block from the payload by `blk_dec_need` (`src/core/memneed.c`), the largest declared. Exact for context mixing (the tables follow from the stream header), measured bounds elsewhere. No new version: a reader that ignores it decodes correctly. |
| rule 1 | `src/core/decode.c` (`frame_admit`, `blk_dec_admit`), `src/core/stream.c`, `src/codecs/cm.c` | Check the header and index CRC, decoded sizes, and declared working set before allocating offset arrays or decoded output. Each block's estimated working set must fit the declaration or, for frames with no declaration, the codec budget. `kolmo_check_frame` performs the buffer admission check without allocating an offset array. |
| rule 2 | `kolmo_params.mem_limit`, `kolmo_decode_limits.max_memory`, `kolmo_set_memlimit`, `-M/--memlimit`, `KOLMO_MEMLIMIT` | `512M`, `8G`, `50%`, `auto` (half the smaller of physical memory and the cgroup limit), `max`. The decoder returns `KOLMO_ERR_MEMORY` when the admission check fails. See the path-specific accounting below. |
| encoder under an explicit cap | `kmem_block_for_cap`, `cm_bits_for` | ratio halves its 128 MiB blocks down to 64 MiB, -x down to 1 MiB, until a block's estimate fits; -x tables shrink until the block's decode figure fits. Format decisions from (input, mode, level, cap) only. The "lighter mode" step and `--memlimit-strict` were not built: a cap below the mode's floor is refused (rule 3) instead, which is simpler to reason about and never changes the mode behind the user's back. |
| rule 3 floors | `kolmo_mem_floor` | speed, balanced 64 MiB; eco, hc 256 MiB; ratio 1 GiB; -x 256 MiB, as proposed. |
| `-x` tables | `cm4_shift_for` (model 4), `cm_bits_for` (models 1-3), `kolmo_set_x_tables`, `--x-tables` | model 4 (w4): `n / 32` lines per context up to 48 MiB, `n / 64` up to 128 MiB, `n / 128` beyond; `--x-tables=full` gives model 4 four times the lines. Models 1-3: scaled (one bucket per 4 bytes, at most 2^21; was about 16× the stream in the first wave-2 build), `--x-tables=full` rc2's sizing (up to 2^22 buckets per model). |
| governor | `src/core/memory.c` | budget = min(cap, available + what Kolmo holds), re-read at most every 100 ms: Linux `MemAvailable`, cgroup v2 `memory.max`/`memory.current` (v1 `limit_in_bytes`/`usage_in_bytes`), PSI `some avg10` ≥ 10% = pressure; macOS `host_statistics64` free + inactive + purgeable and the `DISPATCH_SOURCE_TYPE_MEMORYPRESSURE` source (warning or critical = pressure). Pressure caps the budget at what is held; growth only after 1 s calm. `KOLMO_MEM_FORCE_BUDGET` replaces the sample. |
| reservations | `compress_worker`, `decompress_worker`, `core/trials.c`, `codecs/cm_par.c`, `plan/parallel.c` | every block job (encode and decode) reserves its estimate and waits when it does not fit (one always runs); concurrent trials, extra codec-25 parts and the planner's second CM sample are optional work: they start only when their reservation fits now, otherwise the owning thread runs them afterwards, with the same result. A thread inside a reserved job does not reserve again (no nested waits, so no deadlock); helpers release their own reservation when they finish. |

The macOS sampler and pressure source are written against the documented APIs but were built and tested only on Linux
in this lane; the integration lane builds them on the Mac.

### Decoder metadata and stream buffers

The v12 header declares the working set for one block. `kolmo_decode_memory` exposes this figure, or the largest
block's buffer estimate for a frame with no declaration. A frame with a large index can need metadata headroom
above the reported figure.

- Buffer decoding charges two offset arrays at 16 bytes for each of `blocks + 1` entries. Regular file-to-file decoding
  also charges a copy of the header and index at `32 + 16 * blocks` bytes. Bytes above the 64 KiB allowance
  reduce the codec budget before the offset arrays are allocated. That reduction starts at 4,096 blocks for the
  buffer path and 2,047 blocks for the file-to-file path. The regular-file path checks the fixed-header estimate
  before allocating the full index and verifies its CRC after reading it.
- Longest-first sorting requires another 8 bytes per scheduled block. Sorting is optional. If the frame has no
  declaration, the scheduler skips any sort that would reduce the admitted codec budget.
- The stream decoder includes the output window in the nested codec budget when space permits. At a tight cap it may
  decode one block at a time and allocate that block's output separately. The stream builds and charges one shared
  offset index per member. Its compressed input buffer remains outside the codec budget.
- File-to-file workers allocate packed-input and transform scratch buffers before reserving a block's estimated
  codec working set. Worker caches can retain buffers across calls. Treat the cap as an admission and scheduling
  limit and measure process peak RSS separately.
