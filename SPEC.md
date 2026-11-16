# kolmo container format

This describes the `CZN1` container as written by this implementation. The writer emits the lowest version a file
needs: 3 by default, and 4 to 10 only when a block uses a feature that needs it; versions 1 to 10 are readable. The
section *Identifiers and versions* lists every version, codec id, node id and model, and which release reads each.
The container framing below is stable and intended to be frozen at 1.0. The per-block *transform*
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
| 4 | 1 | version | 1 to 10: the highest version any block needs (3 when none needs more; 4 or 5 only when a table column uses the delta-of-delta or third-order predictor; 6 only when a block uses the minified-JSON split, tflag 10; 7 when a block is a kolmo graph, codec 12; 8 when a block uses the line-class transform, plan type 10; 9 only when a graph leaf uses codec 15; 10 only when a block uses the FASTQ family coder, codec 16) |
| 5 | 1 | mode | the `kolmo_mode` used (rt=0 … ultra=5); informational |
| 6 | 1 | transform | file-level plan type for version 1; 0 in versions 2–5, unless a dictionary is used (then bytes 6–9 hold the dictionary id) |
| 7 | 1 | a | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 8 | 1 | b | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 9 | 1 | c | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 10 | 1 | flags | bit 0 (v3+): block checksums cover the stored payload, not the decoded bytes (set in `rt`). bit 1 (v3+): the zstd-coded blocks were compressed with a dictionary, whose 32-bit id is in bytes 6–9; decoding requires the matching dictionary. bit 2 (v11+): the frame was written with a trained profile, whose 32-bit id is in bytes 6–9; decoding requires that profile (*Profile frames*). Bits 1 and 2 are never both set. Other bits 0 |
| 11 | 1 | reserved | 0 |
| 12 | 8 | orig | original (uncompressed) length in bytes |
| 20 | 4 | nblocks | number of blocks |
| 24 | 4 | block | nominal block size in bytes |
| 28 | 4 | crc32c | CRC-32C (Castagnoli) of header bytes 0..27 followed by the whole block index |

A reader rejects the file if the magic is wrong, the version is not one it knows (1–10 for this implementation; older
releases stop earlier, see *Identifiers and versions*), the index does not fit in the file, or the
header+index CRC does not match. If flag bit 1 is set, the reader must be given a dictionary whose id (the low 32 bits
of XXH3 of the dictionary bytes, seed `0x637a6e44`) equals bytes 6–9, or decoding fails with `KOLMO_ERR_DICT`; an older
reader that predates the flag refuses the newer version rather than returning wrong data.

## Block index — one 16-byte entry per block

| off | size | field | meaning |
|----|----|----|----|
| 0 | 4 | raw | decoded length of this block |
| 4 | 4 | comp | stored length of this block's payload |
| 8 | 4 | checksum | low 32 bits of a block checksum (see below) |
| 12 | 1 | codec | entropy coder: STORE=0, LZ4=1, LZ4HC=2, ZSTD=3, BROTLI=4, BSC=5, LZMA2=6, CM=7, GRAPH=12 (kolmo graph block), FQFAM=16 (FASTQ family coder), CMI=17 (context mixing with the image model); the full list is in *Identifiers and versions* |
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
`f = 8` exactly is the extended header: one more byte follows the size word, `model << 4 | (log2 of the table size − 15)`;
it carries model 3 (orders 12 and 20, the byte the match model expects as a context, finer classes for long matches).
Encoders write model 3 for text, model 2 for other data, and model 1 at level 1 and for blocks of 2^28 bytes or more;
decoders accept all three, so files written with an older model still decode. `CMI` (codec 17) is the same coder with
an image model for 8-bit samples: `iw u32 | ih u32 | ich u8` (row width in bytes, bytes before the first pixel,
interleaved channels), then a size word as above and the coded bytes. CM output is bit-exact across compilers and
CPUs (integer-only, `-ffp-contract=off`). `FQFAM` (codec 16) is described in *FASTQ family blocks*.

## Identifiers and versions

One table of every identifier a frame can carry, the first build that writes it, and what an older reader does with
it. A new identifier is claimed here first, so that two lines of work do not take the same number.

**Container versions** (header byte 4). The writer emits the highest version any block needs; a reader refuses a
version above the highest it knows.

| version | written when | first reader |
|----|----|----|
| 1, 2 | never (historical; read only) | 1.0.0 |
| 3 | by default | 1.0.0 |
| 4, 5 | a table column uses the delta-of-delta (4) or third-order (5) predictor | 1.0.0 |
| 6 | a block uses the minified-JSON split (tflag 10) | 1.1.0 |
| 7 | a block is a kolmo graph (codec 12, plan type 9) | 1.1.0 |
| 8 | a block uses the line-class transform (plan type 10, tflag 12) | 1.1.0 |
| 9 | a graph leaf uses codec 15; on request only (`--native-leaf`) | 1.2.0 |
| 10 | a block uses the FASTQ family coder (codec 16); `ultra` only | this branch (after 1.2.0-rc2) |
| 11 | the frame was written with a trained profile (flag bit 2); see *Profile frames* | rc3 (features-a lane; final numbering at integration) |

Highest version read: 5 by 1.0.0 and 1.0.1; 8 by 1.1.0; 9 by 1.2.0-rc1 and rc2; 10 by this branch. (1.1.0 is the state of `main` under that
version number; it was not tagged or published. 1.2.0 is this build, a release candidate.)

**Block codecs** (index byte 12) and **graph leaf codecs**.

| id | name | where | first reader |
|----|----|----|----|
| 0–7 | STORE, LZ4, LZ4HC, ZSTD, BROTLI, BSC, LZMA2, CM | blocks and graph leaves | 1.0.0 |
| 8–11 | DNA, DNA_PREFIX, PCM, DNA_RUNS | blocks; experimental, read and written only by builds made with `-DKOLMO_FRONTIER` | none (not in release builds) |
| 12 | GRAPH | blocks, version 7 or later | 1.1.0 |
| 13, 14 | HUF (Huffman only), RANS (order-0 rANS) | graph leaves | 1.1.0 |
| 15 | NPLANE (native plane-context leaf) | graph leaves, version 9 only | 1.2.0 |
| 16 | FQFAM (FASTQ family coder) | blocks, version 10 or later; written by `ultra` (not level 1) | this branch |
| 17 | CMI (context mixing, image model) | blocks; written by `ultra` | 1.2.0 |

**Graph nodes** (op byte of a node).

| id | node | first reader |
|----|----|----|
| 1–6 | FIELDS, DECIMAL, CONCAT, INT, FLOAT, IMG8 | 1.1.0 |
| 7 | DEC64 (binary64 values that are short decimals) | 1.2.0 |

**CM models** (top bits of the size word, above).

| model | flag | written when | first reader |
|----|----|----|----|
| 1 | `f` = 0–7 | `ultra` level 1; blocks of 2^28 bytes or more | 1.0.0 |
| 2 | `f` = 9–15 | `ultra`, data that is not text | 1.1.0 |
| 3 | `f` = 8, then one byte | `ultra`, text | 1.2.0 |
| 1, 2, 3 with exact logistic tables | `f` = 8, then one byte whose high nibble is 5, 6 or 7 (model 1, 2, 3) | every `ultra` stream under 2^28 bytes, from 1.2.0-rc2 | 1.2.0-rc2 |
| image model with exact logistic tables | block codec 17, bit 7 of the channel byte | `ultra`, 8-bit images, from 1.2.0-rc2 | 1.2.0-rc2 |

The exact logistic tables use the logistic curve round(4096 / (1 + e^(-x/256))) at every x in [-2048, 2047] instead of the
33 interpolated knots of the earlier models; stretch is its inverse (the smallest x whose squash reaches p). Readers before
1.2.0-rc2 refuse these streams (an unknown model nibble or channel count) with `KOLMO_ERR_CORRUPT`; 1.2.0-rc2 reads both kinds.

**What an older reader does.** Only versions 4 to 10 are announced in the header. Codec 16 is announced: a frame
with a codec 16 block has version 10, so every earlier reader (1.2.0-rc2 and before) refuses it at the header with
`KOLMO_ERR_CORRUPT` ("corrupt or unsupported input") before decoding any block, and returns no bytes; frames without
codec 16 keep the version they had (the writer's output for them is unchanged). A version 10 reader rejects codec 16
in a frame of version 9 or lower, and as a graph leaf. The rest of this paragraph is about the identifiers that are
not announced. Block codec 17, graph node 7 and
CM models 2 and 3 are written inside frames whose header carries the version the rest of the frame needs (3, 7 or
8), following the rule in *Kolmo graph blocks*: new identifiers do not raise the container version, and a decoder
rejects an identifier it does not know. So an older reader refuses such a frame part-way, when it meets the block,
and reports `KOLMO_ERR_CORRUPT` ("corrupt or unsupported input"); it does not return wrong bytes. Measured on 580
frames of 35 files written by 1.2.0-rc1: 1.0.1 decoded 403 and refused 177; 1.1.0 decoded 547 and refused 33 (DEC64
nodes, `ultra` text and `ultra` images); no frame decoded to different bytes. Every frame written by 1.0.1 and
1.1.0 decodes with 1.2.0-rc1. Whether these three identifiers should instead raise the container version (to a new
version of their own), so that an older reader refuses at the header with a clearer cause, is an open decision for
the project owner; it would change the bytes of the frames concerned and is not made here.

## Guarantees

- **Seekable.** Any byte range decodes by reading the header and index and decoding only the blocks it overlaps
  (`kolmo_read_range`).
- **Parallel.** Blocks are independent in both directions.
- **Integrity.** The header+index CRC and per-block checksums are verified; corrupt input is reported as an error,
  never returned as data.
- **Portable.** For a given mode and input, the bytes are identical across Intel and Apple-silicon builds.

## Trained profiles (`KPF1`)

A profile (`kolmo train`, `kolmo_profile_train`) is a file learnt from a sample of a dataset and needed again to
decode the frames written with it. All integers little-endian.

```
"KPF1" | version u16 = 1 | flags u16 = 0 | profile id u32 | section count u32 | sections | crc32c u32
section: tag[4] | flags u32 (bit 0: critical, other bits 0) | length u32 | data[length]
```

- **profile id**: the low 32 bits of XXH3-64 of the section bytes (from offset 16 to the CRC), seed `0x4B504631`.
- **crc32c**: CRC-32C of every byte before it.

A reader rejects the file (`KOLMO_ERR_PROFILE`) when the magic, version, flags, CRC or id do not match, a section does
not fit, the sections do not end exactly at the CRC, a known section appears twice, a known section has the wrong
critical bit or a malformed body, or an unknown section has the critical bit set. Unknown sections without it are
skipped. Version 1 sections:

| tag | critical | body | use |
|----|----|----|----|
| `PARM` | no | mode u8, level i8, analysis u8 (0 none, 1 full, 2 quick, 3 minimal), 0 u8, block u32 | the setting the profile was trained for; what `kolmo -P` uses unless `-m` or `-l` is given |
| `PLAN` | no | count u32 (at most 256), then per pinned plan: key length u8 (≤ 32), key, binary u8, mode u8, analysis u8, size band u8 (0: < 64 KiB, 1: < 1 MiB, 2: < 16 MiB, 3: larger), plan type, a, b, c, wide, verify, kgw (u32 each), image plan (be, off, mode u32, W u64), has formulas u8, formula count u16, group column u32, formulas (y, a, b u16, form u8, 0 u8, k f64) | the plan the analysis chose for a kind of file; matching inputs skip the analysis |
| `ZDIC` | yes | a zstd dictionary (ZDICT format) | zstd leaves of profile frames |
| `RDIC` | yes | raw bytes (the content part of `ZDIC`) | Brotli leaves (attached as a raw prefix dictionary) and LZ4 leaves (its last 64 KiB) |
| `DLIM` | yes | u64 | leaves of at most this many bytes use the dictionaries; absent: 256 KiB. The trainer picks it among 64 KiB, 256 KiB, 1 MiB, 4 MiB and unlimited on a held-out fifth of the samples |
| `INFO` | no | UTF-8 text | summary of the training run |
| `CMPR` | yes (reserved) | context-mixing priors | reserved; no writer yet, so this reader rejects a file that carries it |

**Pinned plans.** The key of an input is its first line with every run of digits replaced by `0` and every run of
ASCII letters by `a`, at most 32 bytes (text), or its first 32 bytes as they are (binary: under 95% printable in the
first 4 KiB). A pinned plan is used for an input of at least 4 KiB when key, binary flag, mode, analysis and size
band match, and, for transposition and table plans, when lines 1 to 32 have the plan's field count (quotes respected);
for wide-table graph plans when line 1 has more than 256 fields; for numeric graph and image plans when the input
is longer than the plan's header. Otherwise the analysis runs as usual. The trainer pins a plan only when the analysis
chose it for at least 90% of the training bytes under that key and band. A pinned plan changes how a file is
encoded, never how it is decoded: every block still carries its plan.

## Profile frames

A frame written with a profile is a `CZN1` frame of version 11 or later with flag bit 2 set and the profile id in
bytes 6–9, or the compact form below. Inside it, every leaf coded with zstd, Brotli or LZ4 whose decoded size is at
most the profile's `DLIM` uses the profile's dictionaries:

- **zstd** (codec 3): with `ZDIC` as dictionary. A leaf of at most 64 KiB is one raw zstd compressed block (the
  zstd block format without block header, as `ZSTD_compressBlock` writes it); a larger leaf is a zstd frame without
  magic number, content size, dictionary id or checksum (`ZSTD_f_zstd1_magicless`).
- **Brotli** (codec 4): a Brotli stream with `RDIC` attached as a raw prefix dictionary.
- **LZ4** (codec 1): an LZ4 block with the last 64 KiB of `RDIC` as dictionary.

Leaves larger than `DLIM`, and every other codec, are coded exactly as in frames without a profile. A decoder given
no profile, or a profile with another id, fails with `KOLMO_ERR_PROFILE` before decoding any block; a decoder given
a profile for a frame that does not need one ignores it.

**Compact form (`CZC1`).** A profile frame of one block whose original size is at most 1 MiB is written as:

```
"CZC1" | profile id u32 | A u8 | [block flag u8, when A bit 6] | original size (LEB128, 1 to 3 bytes) |
checksum u32 | [block plan 4 bytes, when A bit 5] | block payload
```

`A`: bits 0–4 the block codec, bit 5 plan bytes present (otherwise the plan is 0 0 0 0), bit 6 block flag present
(otherwise 0; a present flag is never 0), bit 7 zero. The checksum is the block checksum over the decoded bytes
(low 32 bits of XXH3-64, seed `0x637a6e31`), also for speed-mode blocks, so every header field is covered by the check
of the decoded output. The block is decoded as the only block of a version 11 frame with the same codec, flag, plan
and payload. The header is 14 to 21 bytes, against 52 for the one-block `CZN1` frame; with the raw zstd blocks above,
a small message costs 14 to 16 bytes of framing in all (the zstd command line writes 13 to 18 for a frame with a
dictionary id, content size and checksum).

## Target frames (`--target`)

`kolmo_compress_target` codes each chunk of the input with its own setting and joins the chunk frames into one
`CZN1` frame: the indexes and payloads are concatenated in order, the version is the highest of the chunk frames,
flag bit 0 is set only when every chunk frame had it (otherwise the speed-mode chunks' checksums are recomputed over
their decoded bytes), and byte 5 holds the most common mode. No reader change is needed: blocks were always
independent and carry their own codec, plan and flag.

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
src/transforms/table.c       table v2: typed columns, predictors, formula discovery
src/transforms/log.c         log templates and skeleton columns
src/transforms/sci.c         fixed records and 16-bit images
src/transforms/word.c        word dictionary
src/transforms/table_v03.c   table v1 (tflag 3); transpose.c, shuffle.c, x86.c: typed transpose, byte-plane shuffle,
                             x86 filter
src/core/                    container (container.h, compress.c, decode.c); src/codecs/: the block coders
```

## Kolmo graph blocks (version 7)

A block with codec 12 and plan type 9 is a **kolmo graph**: instead of one fixed transform, the payload (after the
4-byte plan) holds a small program. Stream 0 is the block's bytes. Each node consumes streams and appends its outputs
to the stream table; streams no node consumes are leaves, each coded by an ordinary codec. The decoder decodes the
leaves, then runs the nodes' inverses from the last node to the first. New transforms add node ids; the container
version stays 7. A decoder rejects any node id it does not know.

```
payload := 'G' | 1 (graph version) | nodes uv | node * nodes | leaves uv | leaf * leaves
node    := op u8 | nin uv | input index uv * nin | nout uv | param length uv | params
leaf    := stream index uv | codec u8 (0-7 as for blocks, 13, 14, or 15 from version 9) | raw length uv | coded length uv | coded bytes
```

**Leaf codec 15: native plane-context leaf (container version 9).** Written only on request (`--native-leaf`,
`KOLMO_T_NATIVE_LEAF`, balanced), only for the integer residual stream of an `INT` node, and only when smaller than
the other leaf codings. A reader rejects codec 15 in a file of version below 9, and as a block codec. Payload, for raw
length `rl`:

```
payload := fmt u8 (= 1) | np u8 (1-8, rl % np == 0, L = rl / np) | plane * np        ; plane k = bytes k*L .. k*L+L-1
plane   := 'c' value u8 | 's' L bytes | 'h' len uv | Huffman stream | 'r' len uv | rANS stream
         | 'l' len uv | context stream (lanes, L >= 8) | 'p' len uv | context stream (cross-plane, plane k >= 1)
context := mask u16 (buckets 0-8, nonzero, bits 9-15 zero) | table * popcount(mask) | z uv | state u64 * 4 | word u32 *
table   := 256 frequencies in the rANS table format, summing to 65536 (a one-symbol table holds 65536)
```

The `h` and `r` streams are those of codecs 13 and 14. In a context stream the bucket of a context byte `c` is 0 when
`c` is 0, else its bit length (1-8); rANS uses four 64-bit states with lower bound 2^31 and 16-bit frequencies, words
are read forwards, `z` (a multiple of 4, at least 32) is the rest of the stream, and every state must end at 2^31.
Lanes (`l`): with `L4 = L / 4`, lane `j` is positions `j*L4 .. j*L4+L4-1`, decoded in lockstep by state `j`, then the
tail positions `4*L4 + j` by state `j`; the context is the previous byte of the plane, reset to bucket 0 at position 0
and at each lane start. Cross-plane (`p`): position `i` uses state `i mod 4` and the context is byte `i` of plane
`k - 1`. Any other tag, table, length or final state is corrupt.

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

## FASTQ family blocks (codec 16, version 10)

A block with codec 16 is FASTQ text coded by stream family (`src/codecs/fastq.c`; the standalone research tool
`src/experimental/kfam.c` writes the same frame). Its plan prefix is `0, 0, 0, 0` and its tflag 0; a decoder rejects
anything else, and rejects codec 16 in a file below version 10 or as a graph leaf. The block checksum covers the
decoded block as usual. `ultra` writes it, at levels 0 and 2 (not at level 1, which is context mixing only), for a
block that is complete FASTQ text (four-line records, `\n` line ends, the plus line bare or repeating the name, at
most 32 name tokens and 4096 name bytes, qualities 33 to 160) when it is smaller than what the block would be
otherwise. No other mode writes it.

```
payload := "KFM2" | family u8 (1 = FASTQ) | original size uv | streams uv (5)
           | stream * 5 : kind u8 | parameter length uv | parameter bytes
           | jobs uv | job * jobs : stream uv | raw size uv | coded size uv
           | the coded bytes of the jobs, in job order
```

`uv` is an unsigned LEB128 varint. Jobs are listed in stream order; a stream's bytes are its jobs' decoded bytes in
order. Each job is one binary arithmetic-coded slice (32-bit range, 16-bit probabilities, carry-less: the range is
renormalised a byte at a time when its top bytes agree; four flush bytes) coded with fresh models, so jobs decode
independently and in parallel, except that streams 0 and 3 are decoded first. Kinds: 0 bytes (an order-0 to 3 byte
model with one context byte from the family), 1 bases (2-bit symbols), 2 qualities. The streams:

| # | kind | content |
|----|----|----|
| 0 | 0 | layout: per record a flag byte, 0 or 1 (plus line bare, or repeating the name), 2 or 3 when the read length changes, then the length uv |
| 1 | 0 | names: per name token (maximal runs of digits or of other bytes) an operation: 0 same as the previous name's token, 1 numeric delta (zigzag uv), 2 literal (length uv, bytes); 3 ends the name. Context: token index and operation state |
| 2 | 1 | bases, one per base: A=0 C=1 G=2 T=3 (any other byte codes as 0) |
| 3 | 0 | exceptions: per base that is not A, C, G or T, the distance from the previous one (uv) and the byte |
| 4 | 2 | qualities, one per base: the quality byte minus 33. Parameter: a 16-byte presence mask of the values 0 to 127, then each present value's count (uv); the decoder builds from it the same order-preserving binary code tree as the encoder |

Names and qualities are cut into jobs at record boundaries every 4 Mi symbols, bases every 64 Mi bases; the cut
points are written in the job table, so a decoder needs no rule for them. The models (their contexts are listed in
`src/codecs/fastq.c`) are part of the format: bases use orders 6 and 12 and three match models; qualities use the
previous three qualities, the position, a running sum of quality changes, the mean so far and whether the base is
an exception, six context tables, two mixers, a final mixer and two adaptive probability maps. Like `CM`, the coder
is integer-only except for its logistic tables, built from IEEE `+ * /` only, so its output is bit-exact across
compilers and CPUs.
