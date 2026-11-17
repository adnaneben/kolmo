# Metadata: structure read from the input's own headers

Status: rc3 wave 2 (meta lane, branch `claude/rc3w2-meta-20261117`). Code: `src/plan/meta.c`, `src/transforms/arm64.c`;
hooks in `core/compress.c` (segments, AIFF), `plan/parallel.c` (plans from a part's kind), `plan/planner.c` (image
geometry), `core/blocks.c` / `core/decode.c` (stored packed blocks, ARM64 filter, AIFF swap). Measurements:
`benchmark/rc3w2/meta/RESULTS.md`. Format: `SPEC.md` (plan type 11, codec 19 prefix `0, 1`).

## The question

Kolmo infers structure by sampling bytes: a lag search for an image's row length, entropy of byte planes for an
array's element width, trials for records and code. Many formats state that structure in a header. Does reading it
make the output smaller or the encoder faster? Measured type by type, the answer is: **a lot for images with
uncommon widths, executables, ARM64 code, AIFF, archives and packed content; nearly nothing for tensor files, whose
element width the sampler already finds.**

## Rules

1. **Content only.** Every decision comes from the bytes. File names and extensions are not read, by the CLI or the
   API, so the same bytes give the same frame whatever they are called. No hint parameter was added: every format
   below has a magic number or a header, and the only formats without one (CSV, TSV, raw arrays) are already found
   by sampling. A name hint would make two identical inputs code differently, and would trust a label more than the
   content; measured cases where a hint would have helped: none.
2. **Metadata is untrusted.** Each parser checks every offset and size against the input length before reading,
   caps counts (sections 4096, zip members 65536, tensors 65536, GGUF keys 65536, nesting depth 2), and treats a
   header that does not parse as absent. `tests/test_meta.c` runs every parser on 3000 mutations of each of 18
   synthetic files (flipped bytes, lengths and offsets set to extremes, truncation) under ASan and UBSan.
3. **A hint, never a dependency.** The decoder needs no metadata: segments only move block boundaries, and each
   block carries its plan as before. Only two additions are in the format (version 11): the ARM64 branch filter and
   the byte swap of AIFF samples.

## What is read, and what it changes

| input | read from | used for | effect (vs the rc3 base) |
|---|---|---|---|
| PGM/PPM (P5/P6), PAM (P7), BMP (8/24/32 bit, `BI_RGB`), uncompressed chunky TIFF, NPY uint8 | width, height, channels, maxval, pixel offset, BMP row padding | the IMG8 node's row length and channel step, without the lag search; an image is never taken for text | −9% to −18% on odd widths at `balanced`, −13% to −30% at `hc`; also on standard widths the sampler missed (−11.5%) |
| tar | member headers | members ≥ 1 MiB (as rc2) **and** members ≥ 64 KiB with a known header get their own plan; such a member is cut further by its own format | mixed tar −3.6% / −3.9% |
| zip, NPZ | central directory (zip64 too) | stored members ≥ 64 KiB own a plan, so an NPY member's header gives its dtype and shape; compressed members are packed parts | mixed NPZ −24%; NPZ of one dtype ±0.2% |
| ELF, Mach-O (thin and universal), PE | section tables, machine | code sections and the rest planned apart; x86 code takes the call/jump trial alone | x86 ELF −0.2% to −5.4% |
| ARM64 executables (ELF `EM_AARCH64`, Mach-O `CPU_TYPE_ARM64`, PE `0xAA64`) | as above | the new ARM64 branch filter (BL, ADRP) on code sections, with the page phase of each block | −4.9% to −7.7% `balanced`, −6.8% to −8.8% `hc`, −6.8% to −9.5% `max` |
| AIFF / AIFF-C (`NONE`, `twos`), 16-bit | `COMM`, `SSND` | the audio codec (19) on byte-swapped samples, as for WAV | −34% `balanced`/`hc`, −24% `max` (now equal to the same audio as WAV) |
| gzip, xz, zstd, bzip2, LZ4, 7z, RAR, PNG, JPEG, WebP, JPEG XL, ISO media, FLAC, Ogg, MP3, Matroska; compressed zip members | magic numbers | no analysis; a block is stored without running its coder when a collision-entropy and LZ4 probe finds it incompressible | `hc` 40-75× faster to compress on such files (2.15 s → 0.03 s on an 11 MB NPZ of deflated members); size equal (+65 B on a 6 KB JPEG zstd squeezed 13 B from) |
| safetensors | JSON header (dtype, offsets) | the header and each run of one dtype planned apart | −0.01% to −0.28%; `hc` 11.0 s → 1.0 s on a checkpoint with fp16 and uint8 tensors |
| GGUF v2/v3 | key/values, tensor infos, alignment | the same per run of a ggml type | ±0.05% (and +3.5% / −0.4% on a 1.2 MB file, block-boundary luck); `hc` 3.3 s → 1.5 s |
| WAV, Y4M | `fmt `, stream header | already exact before this lane (codecs 19, 20) | — |
| CSV / TSV, JSON | header row, keys | already read by the table and JSON transforms | — |
| Parquet, Arrow, Avro, HDF5 | — | not parsed: their pages are mostly compressed (Parquet: snappy/zstd) and Kolmo's output is the input + 52 to 272 B; the packed path is not taken for them because some pages are plain (a text Parquet compresses 13% at `balanced`, 18% at `hc`) | none larger by more than the container's bytes |

### Tensors: what the metadata could still give

Per tensor, the order-0 entropy of exponent and of sign+mantissa is a bound for a per-tensor static coder. On the
bf16 checkpoint it is 0.76% below `balanced` (178.07 vs 179.42 MB); column contexts for the exponent and mantissa
contexts given the exponent add about another 1%. Splitting the file by tensor (each tensor its own plan) reached
only 0.05% of this: the float graph already finds the 2-byte element and splits the exponent. A dedicated tensor
codec (exponent with column context, mantissa given exponent) is what would collect the rest; it is not in this
lane (no codec id reserved, ~1% at `balanced` on bf16).

## The ARM64 branch filter

BL (26-bit word offset) and ADRP (21-bit page offset within ±2^17 pages) become position-absolute, as the ARM64 BCJ
filter of XZ Utils does (written from its description; 0BSD). Two Kolmo-specific details:

- **Blocks are 4096-aligned** for this plan, so every block starts on an instruction word and on a page boundary
  relative to the part.
- **The page phase.** ADRP's arithmetic depends on the instruction's address modulo 4096; the file offset of an
  executable's code is congruent to its address modulo the page size. The plan carries the part's offset modulo 4096
  and each block records its own phase in the plan prefix. Without the phase the filter was 0.35% larger.

On x86 nothing new was needed: the call/jump transform exists, and the gain comes from planning code and data apart.

## Routing (for `docs/ROUTING-MAP.md`)

| detected by metadata | `rt` | `speed` | `balanced` | `eco`/`hc` | `max` | `ultra` |
|---|---|---|---|---|---|---|
| 8-bit image header | as before | IMG8 with header geometry | IMG8 (graph, zstd leaves) | IMG8, zstd 19 / Brotli leaves | IMG8, BWT or LZMA2 leaves | CM image model (17) or IMG8 |
| x86 code section | LZ4 | call/jump filter | call/jump + zstd | + zstd 19 / Brotli | + LZMA2 | + CM |
| ARM64 code section | LZ4 | ARM64 filter + LZ4 | ARM64 filter + zstd | + zstd 19 / Brotli | + LZMA2 | + CM |
| packed content | stored if incompressible, else LZ4 | same | stored or zstd | stored or the mode's coder | same | same |
| AIFF 16-bit | LZ4 | as before | kaud profile 0 (19) | kaud profile 0 | kaud profile 1 | kaud profile 1 |
| tensor run / archive member | its own plan, as any input of that content | | | | | |

## Not done, and why

- **`kolmo a` archives** keep one stream of deduplicated chunks; a member's header is rarely at a chunk-store
  segment start. Routing per member there needs the archive to pass member boundaries to the compressor (an API
  change), left to the features-b owner.
- **WAV inside tar** gets its own plan (the int16 graph) but not codec 19, which is chosen for a whole WAV file only.
- **ONNX** (protobuf, tensors in `raw_data` fields) is not parsed; its tensors are found by sampling as before.
- **16-bit PGM/PNG-like samples**: the IMG16 detector already searches widths 16 to its maximum on every input; no
  case was found where it missed.
