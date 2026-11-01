# kolmo container format

This describes the `CZN1` container as written by this implementation. Version 5 is current; versions 1 to 4
remain readable. The container framing below is stable and intended to be frozen at 1.0. The per-block *transform*
sub-formats (tables, logs, scientific records, images, words) are not yet frozen and may change before 1.0; a decoder
identifies them per block and this document specifies where that identification lives, not every transform's bytes.

All multi-byte integers are little-endian. Offsets are in bytes from the start of the file.

## File

```
+----------------+------------------------+-------------------------+
| header (32 B)  | block index (16 B/blk) | block payloads          |
+----------------+------------------------+-------------------------+
```

The payloads follow the index in block order; payload *i* occupies the next `comp[i]` bytes.

## Header (32 bytes)

| off | size | field | meaning |
|----|----|----|----|
| 0 | 4 | magic | ASCII `CZN1` |
| 4 | 1 | version | 1 to 5 (4 or 5 only when a table column uses the delta-of-delta or third-order predictor) |
| 5 | 1 | mode | the `czn_mode` used (rt=0 … ultra=5); informational |
| 6 | 1 | transform | file-level plan type for version 1; 0 in versions 2–3 (plan is per block) |
| 7 | 1 | a | plan parameter (v1) |
| 8 | 1 | b | plan parameter (v1) |
| 9 | 1 | c | plan parameter (v1) |
| 10 | 1 | flags | bit 0 (v3): block checksums cover the stored payload, not the decoded bytes (set in `rt`); other bits 0 |
| 11 | 1 | reserved | 0 |
| 12 | 8 | orig | original (uncompressed) length in bytes |
| 20 | 4 | nblocks | number of blocks |
| 24 | 4 | block | nominal block size in bytes |
| 28 | 4 | crc32c | CRC-32C (Castagnoli) of header bytes 0..27 followed by the whole block index |

A reader rejects the file if the magic is wrong, the version is not 1–5, the index does not fit in the file, or the
header+index CRC does not match.

## Block index — one 16-byte entry per block

| off | size | field | meaning |
|----|----|----|----|
| 0 | 4 | raw | decoded length of this block |
| 4 | 4 | comp | stored length of this block's payload |
| 8 | 4 | checksum | low 32 bits of a block checksum (see below) |
| 12 | 1 | codec | entropy coder: STORE=0, LZ4=1, LZ4HC=2, ZSTD=3, BROTLI=4, BSC=5, LZMA2=6, CM=7 |
| 13 | 1 | tflag | transform applied before the coder (see below) |
| 14 | 2 | reserved | 0 |

`raw` values sum to `orig`. Payload offsets are the running sum of `comp`, starting at 32 + 16·nblocks.

**Block checksum.** Version 3 uses the low 32 bits of XXH3-64; versions 1–2 use XXH64. By default it is computed over
the *decoded* block and checked after decoding. When header flag bit 0 is set (rt), it is instead computed over the
stored payload after its 4-byte plan prefix, seeded with those 4 bytes, and checked *before* decoding.

## Block payload

In versions 2 and 3 every payload begins with a 4-byte **plan prefix** `t, a, b, c`, followed by the coded stream.
`t` is the transform/plan type:

```
T_NONE=0  TP=1 (typed transpose)  SHUF=2 (byte-plane shuffle)  TBL=3 (table v2)  LOG=4 (log templates)
IMG=5 (16-bit image)  REC=6 (fixed records)  X86=7 (call/jump filter)  WORD=8 (word dictionary)
```

`tflag` in the index records which transform actually produced the stored bytes (0 = none; the transform-specific
values are defined in the source). When a transform is present, a small transform header may precede the coded data
inside the payload (for example a 4-byte transformed-length for the table, log, record, image and word transforms, or
k×4 column sizes for the transpose). The decoder reads the plan prefix, undoes the coder to recover the transformed
bytes, then undoes the transform named by `tflag` to recover the block.

In version 1 there is no plan prefix; the file-level `transform`/`a`/`b`/`c` in the header apply to every block.

## Entropy coders

`STORE` is the raw block. `LZ4`/`LZ4HC`, `ZSTD`, `BROTLI` and `LZMA2` are the standard formats from the vendored
libraries. `BSC` is libbsc's block-sorting stream, prefixed with a 4-byte XXH32 of the stream that is checked before
decoding. `CM` is this project's integer context-mixing coder; its stream starts with a 32-bit size word whose top 4
bits carry the log2 of the model's hash-table size minus 15 (0 = the legacy default), and the arithmetic-coded bytes
follow. CM output is bit-exact across compilers and CPUs (integer-only, `-ffp-contract=off`).

## Guarantees

- **Seekable.** Any byte range decodes by reading the header and index and decoding only the blocks it overlaps
  (`czn_read_range`).
- **Parallel.** Blocks are independent in both directions.
- **Integrity.** The header+index CRC and per-block checksums are verified; corrupt input is reported as an error,
  never returned as data.
- **Portable.** For a given mode and input, the bytes are identical across Intel and Apple-silicon builds.

## Stream format (`CZNS`) — large inputs

For inputs processed in bounded memory (and over pipes), a stream wraps independent containers:

```
"CZNS"  ver(1 = 1)  reserved(3)  seg_bytes(u32 le)      # 12-byte stream header
then, repeated:   len(u64 le)  frame[len]               # frame = one complete CZN1 container
end:              0 (u64 le)                             # zero-length terminator
```

Each frame is a normal container (above) for one segment of the input (default 64 MiB), so encode and
decode hold at most one segment plus its frame in memory. Segments are independent — compressed and
decoded across cores like blocks — so very small segments cost a little ratio at their boundaries. A
decoder distinguishes the two formats by the first four bytes (`CZN1` one-shot vs `CZNS` stream).
Produced and consumed by `czn_stream_compress` / `czn_stream_decompress`.

## Transform sub-formats

The byte layouts of the table, log, record, image and word transforms live with their code and are documented there,
not frozen here:

```
src/tbl2.inc   table v2: typed columns, predictors, formula discovery
src/log.inc    log templates and skeleton columns
src/sci.inc    fixed records and 16-bit images
src/word.inc   word dictionary
src/czn.c      container, typed transpose, byte-plane shuffle, x86 filter, coders
```
