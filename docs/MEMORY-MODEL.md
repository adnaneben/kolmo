# Memory model (proposal for review)

Status: **proposal**, 16 November 2026. Nothing here is implemented until the owner approves it.

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
