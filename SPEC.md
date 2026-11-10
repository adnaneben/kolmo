# kolmo container format

This describes the `CZN1` container as written by this implementation. Version 6 is current (written only for minified JSON; otherwise 3 to 5); versions 1 to 5
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
| 4 | 1 | version | 1 to 7 (4 or 5 only when a table column uses the delta-of-delta or third-order predictor; 6 only when a block uses the minified-JSON split, tflag 10; 7 when a block is a kolmo graph, codec 8) |
| 5 | 1 | mode | the `kolmo_mode` used (rt=0 … ultra=5); informational |
| 6 | 1 | transform | file-level plan type for version 1; 0 in versions 2–5, unless a dictionary is used (then bytes 6–9 hold the dictionary id) |
| 7 | 1 | a | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 8 | 1 | b | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 9 | 1 | c | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 10 | 1 | flags | bit 0 (v3+): block checksums cover the stored payload, not the decoded bytes (set in `rt`). bit 1 (v3+): the zstd-coded blocks were compressed with a dictionary, whose 32-bit id is in bytes 6–9; decoding requires the matching dictionary. Other bits 0 |
| 11 | 1 | reserved | 0 |
| 12 | 8 | orig | original (uncompressed) length in bytes |
| 20 | 4 | nblocks | number of blocks |
| 24 | 4 | block | nominal block size in bytes |
| 28 | 4 | crc32c | CRC-32C (Castagnoli) of header bytes 0..27 followed by the whole block index |

A reader rejects the file if the magic is wrong, the version is not 1–7, the index does not fit in the file, or the
header+index CRC does not match. If flag bit 1 is set, the reader must be given a dictionary whose id (the low 32 bits
of XXH3 of the dictionary bytes, seed `0x637a6e44`) equals bytes 6–9, or decoding fails with `KOLMO_ERR_DICT`; an older
reader that predates the flag refuses the newer version rather than returning wrong data.

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

**JSON.** The LOG plan carries the tokenizer in `a`: 0 = space-separated words (logs), 1 = JSON (keys and punctuation
form the template, every scalar value is a placeholder), 2 = minified JSON. The output of modes 0 and 1 is the same
log-template stream (tflag 5). Mode 2 inserts a newline after every `,` that follows `}` or `]` outside strings and
codes the result as JSON lines; such blocks have tflag 10 and an 8-byte prefix (log-stream length u32, number of
inserted newlines u32), and the decoder removes every newline from the decoded text (the encoder only uses mode 2 on
blocks that contain no newline). Files with a tflag 10 block are version 6.

## Entropy coders

`STORE` is the raw block. `LZ4`/`LZ4HC`, `ZSTD`, `BROTLI` and `LZMA2` are the standard formats from the vendored
libraries. `BSC` is libbsc's block-sorting stream, prefixed with a 4-byte XXH32 of the stream that is checked before
decoding. `CM` is this project's integer context-mixing coder; its stream starts with a 32-bit size word whose low 28
bits are the block length and whose top 4 bits `f` select the model: `f = 0` is model 1 with the legacy default table
size; otherwise `f & 7` is the log2 of the hash-table size minus 15, and `f & 8` selects model 2 (two more sparse
contexts, three record contexts, four mixers combined by a learned final mixer, and an extra order-2 APM). The arithmetic-coded bytes follow.
Encoders write model 2; decoders accept both, so files written with model 1 still decode. CM output is bit-exact across compilers and CPUs (integer-only, `-ffp-contract=off`).

## Guarantees

- **Seekable.** Any byte range decodes by reading the header and index and decoding only the blocks it overlaps
  (`kolmo_read_range`).
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
Produced and consumed by `kolmo_stream_compress` / `kolmo_stream_decompress`.

## Transform sub-formats

The byte layouts of the table, log, record, image and word transforms live with their code and are documented there,
not frozen here:

```
src/tbl2.inc   table v2: typed columns, predictors, formula discovery
src/log.inc    log templates and skeleton columns
src/sci.inc    fixed records and 16-bit images
src/word.inc   word dictionary
src/kolmo.c      container, typed transpose, byte-plane shuffle, x86 filter, coders
```

## Kolmo graph blocks (version 7)

A block with codec 8 and plan type 9 is a **kolmo graph**: instead of one fixed transform, the payload (after the
4-byte plan) holds a small program. Stream 0 is the block's bytes. Each node consumes streams and appends its outputs
to the stream table; streams no node consumes are leaves, each coded by an ordinary codec. The decoder decodes the
leaves, then runs the nodes' inverses from the last node to the first. New transforms add node ids; the container
version stays 7. A decoder rejects any node id it does not know.

```
payload := 'G' | 1 (graph version) | nodes uv | node * nodes | leaves uv | leaf * leaves
node    := op u8 | nin uv | input index uv * nin | nout uv | param length uv | params
leaf    := stream index uv | codec u8 (0-7 as for blocks, 13 or 14) | raw length uv | coded length uv | coded bytes
```

Outputs of a node take the next stream indices, in order. Every stream is consumed at most once; stream 0 must be
consumed when there are nodes; every unconsumed stream must appear exactly once as a leaf; inputs must refer to
earlier streams. `uv` is an unsigned LEB128 varint.

| Op | Name | Params | Inputs → outputs |
|---|---|---|---|
| 1 | FIELDS | delimiter u8, k uv | text → shape, column 0 … column k−1. A line with exactly k fields sends each field, `\n`-terminated, to its column. Any other line is kept in shape: line count uv, final-newline u8, then per irregular line its line-index gap uv (from the previous irregular line, or from 0), length uv and bytes. |
| 2 | DECIMAL | separator u8, predictor u8 | values (`\n`-terminated) → tags, scales, mantissas, texts. Tag per value: 0 empty, 1 number, 2 text. A number matches `-?(0\|[1-9][0-9]*)(sep[0-9]+)?` with at most 18 digits and is not a negative zero; its scale byte is the digit count after the separator, or 255 for none; its mantissa (digits as one integer, signed) is stored as the zigzag uv of the difference from the previous number in the column (predictor 1) or of itself (predictor 0). Text values are stored `\n`-terminated. |
| 3 | CONCAT | count uv, length uv * count | count streams → their concatenation |
| 4 | INT | width u8 (1, 2, 4, 8), flags u8 (bit 0 signed, bits 1–2 order 0–2, bit 3 byte planes), output width u8 (1, 2, 4, 8), head uv, count uv, gcd uv (≥ 1), bias uv (zigzag), seeds uv (zigzag) * order | bytes → residuals, rest. The input is `head` bytes, `count` little-endian integers of `width` bytes (sign-extended to 64 bits when signed, else zero-extended), then trailing bytes; rest holds the head bytes followed by the trailing bytes. Order 0 stores (v − bias) / gcd; orders 1 and 2 store zigzag((v − p) / gcd), where p is the previous value (order 1) or twice the previous minus the one before (order 2), for every value after the first `order` (the seeds). Arithmetic wraps modulo 2^64 and divisions are exact. Each residual takes `output width` little-endian bytes; with bit 3 the residual stream is byte planes (all low bytes, then all second bytes, …). |

| 5 | FLOAT | width u8 (2, 4, 8), head uv, count uv | bytes → plane 0 … plane width−1, rest. The input is `head` bytes, `count` little-endian words of `width` bytes, then trailing bytes (rest = head bytes then trailing bytes). Each word is rotated left by one bit, so the sign moves to bit 0 and the top byte of a float32 is its exponent; byte b of every rotated word goes to plane b. |

| 6 | IMG8 | row bytes uv, channel step u8 (1–4), head uv | bytes → residuals, rest. After `head` bytes the input is rows of `row bytes`; each byte is predicted from the bytes `step` to the left (a), one row up (b) and up-left (c) by the median of a, b and a + b − c (LOCO-I); the first row uses a, the first `step` bytes of a row use b. Residual = zigzag of (x − prediction) mod 256 read as int8. Bytes after the last whole row and the head go to rest. |

Leaf codec 14 is static order-0 rANS: chunks of at most 1 MiB, each `raw length uv | frequency table | coded length uv |
words`. The table gives each byte's frequency (scaled to 2^16) as a uv, a 0 being followed by one byte counting the
further absent symbols. The words are 32-bit little-endian: four 64-bit states (position mod 4, low word first), then
the renormalisation words, read forwards (rans64). The encoder tries it at hc, max and ultra.

Leaf codec 13 is Huffman-only coding (zstd's Huffman coder, no matches), for skewed byte streams without repeats such
as an exponent plane: chunks of at most 128 KiB, each `mode u8 | raw length uv` then for mode 0 the raw bytes, for
mode 1 one byte repeated, for mode 2 `coded length uv | zstd HUF 4-stream data`. The encoder keeps, per leaf, the
smallest of the block codec, Huffman-only and stored.

The planner uses graphs for delimited tables with more than 256 columns (the limit of the table transform) and for
arrays of 16-, 32- and 64-bit integers and float arrays (a `.npy` header becomes the node's head), when a slice codes at least 3%
smaller than the other candidates. A
version 1–6 decoder rejects version 7 files.
