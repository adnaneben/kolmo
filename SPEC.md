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
| 4 | 1 | version | 1 to 11: the highest version any block needs (3 when none needs more; 4 or 5 only when a table column uses the delta-of-delta or third-order predictor; 6 only when a block uses the minified-JSON split, tflag 10; 7 when a block is a kolmo graph, codec 12; 8 when a block uses the line-class transform, plan type 10; 9 only when a graph leaf uses codec 15; 10 only when a block uses the FASTQ family coder, codec 16; 11 (format 2) when a block or graph leaf uses codecs 19-23, a block uses the ARM64 branch filter (plan type 11), a table block's transformed length exceeds the block length + 16384 (see below), the frame uses a trained profile (flag bit 2), or a CM stream (block or graph leaf) carries learning settings (model nibble 14 or 15)) |
| 5 | 1 | mode | the `kolmo_mode` used (rt=0 … ultra=5); informational |
| 6 | 1 | transform | file-level plan type for version 1; 0 in versions 2–5, unless a dictionary is used (then bytes 6–9 hold the dictionary id) |
| 7 | 1 | a | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 8 | 1 | b | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 9 | 1 | c | plan parameter (v1); part of the dictionary id when flag bit 1 is set |
| 10 | 1 | flags | bit 0 (v3+): block checksums cover the stored payload, not the decoded bytes (set in `rt`). bit 1 (v3+): the zstd-coded blocks were compressed with a dictionary, whose 32-bit id is in bytes 6–9; decoding requires the matching dictionary. bit 2 (v11+): the frame was written with a trained profile, whose 32-bit id is in bytes 6–9; decoding requires that profile (*Profile frames*). Bits 1 and 2 are never both set. Other bits 0 |
| 11 | 1 | memd | decode working set per block (rc3, `docs/MEMORY-MODEL.md`): 0 = not declared (frames written before rc3); otherwise the most one block needs to decode is at most 2^(memd/8) MiB, rounded up to the byte (`memd` = 1 is 1.09 MiB, 8 is 2 MiB, 80 is 1 GiB, 255 about 3.8 PiB) |
| 12 | 8 | orig | original (uncompressed) length in bytes |
| 20 | 4 | nblocks | number of blocks |
| 24 | 4 | block | nominal block size in bytes |
| 28 | 4 | crc32c | CRC-32C (Castagnoli) of header bytes 0..27 followed by the whole block index |

A reader rejects the file if the magic is wrong, the version is not one it knows (1–11 for this implementation; older
releases stop earlier, see *Identifiers and versions*), the index does not fit in the file, the
header+index CRC does not match, the raw sizes of the index do not add up to `orig`, a stored block (codec 0) is not
exactly its raw size plus the 4 plan bytes, or (*Decode working set*, below) a block needs more than `memd` declares. If flag bit 1 is set, the reader must be given a dictionary whose id (the low 32 bits
of XXH3 of the dictionary bytes, seed `0x637a6e44`) equals bytes 6–9, or decoding fails with `KOLMO_ERR_DICT`; an older
reader that predates the flag refuses the newer version rather than returning wrong data.

## Block index — one 16-byte entry per block

| off | size | field | meaning |
|----|----|----|----|
| 0 | 4 | raw | decoded length of this block |
| 4 | 4 | comp | stored length of this block's payload |
| 8 | 4 | checksum | low 32 bits of a block checksum (see below) |
| 12 | 1 | codec | entropy coder: STORE=0, LZ4=1, LZ4HC=2, ZSTD=3, BROTLI=4, BSC=5, LZMA2=6, CM=7, GRAPH=12 (kolmo graph block), FQFAM=16 (FASTQ family coder), CMI=17 (context mixing with the image model), GR6=26 (GR6+ line-structured text); the full list is in *Identifiers and versions* |
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
KG=9 (kolmo graph)  LINES=10 (line classes)  A64=11 (ARM64 branch filter, version 11)
```

`tflag` in the index records which transform actually produced the stored bytes (0 = none; the transform-specific
values are defined in the source). When a transform is present, a small transform header may precede the coded data
inside the payload (for example a 4-byte transformed-length for the table, log, record, image and word transforms, or
k×4 column sizes for the transpose). The decoder reads the plan prefix, undoes the coder to recover the transformed
bytes, then undoes the transform named by `tflag` to recover the block.

The transformed length may exceed the block length by at most 16384 bytes. Exception (version 11): for the table
transform (tflag 4) the bound is the block length + ⌊block length / 8⌋ + 16384, because its typed columns carry a kind
and a scale byte per value and a length byte per text field (short numbers grow a few percent before coding). Encoders
write version 11 only when some table block uses that extra room.

In version 1 there is no plan prefix; the file-level `transform`/`a`/`b`/`c` in the header apply to every block.

**JSON.** The LOG plan carries the tokenizer in `a`: 0 = space-separated words (logs), 1 = JSON (keys and punctuation
form the template, every scalar value is a placeholder), 2 = minified JSON. The output of modes 0 and 1 is the same
log-template stream (tflag 5). Mode 2 inserts a newline after every `,` that follows `}` or `]` outside strings and
codes the result as JSON lines; such blocks have tflag 10 and an 8-byte prefix (log-stream length u32, number of
inserted newlines u32), and the decoder removes every newline from the decoded text (the encoder only uses mode 2 on
blocks that contain no newline). Files with a tflag 10 block are version 6.

## Entropy coders

`STORE` is the raw block. `LZ4`/`LZ4HC`, `ZSTD`, `BROTLI` and `LZMA2` are the standard formats from the vendored
libraries. `BSC` is libbsc 3.4.0's block-sorting stream (written and read by Kolmo's C port in `src/codecs/bwt.c`), prefixed with a 4-byte XXH32 of the stream that is checked before
decoding; from rc3 it is read only. `BWT` (codec 18, container version 11) is Kolmo's own block-sorting stream
(`src/codecs/bwt_stream.c`): flags u8 (bits 0-1 QLFC model, 1 static or 2 adaptive; bit 2 LZP) | LZP table bits u8 and
minimum match u8 when bit 2 is set | primary index u32 | log2 of the index sampling interval u8 | k u8 | k sampled
BWT indexes u32 | the QLFC stream (block count u8, per block raw and coded sizes u32, the blocks). All little-endian;
no header, length or checksum of its own: the raw size and the XXH3 block checksum come from the container. `CM` is this project's integer context-mixing coder; its stream starts with a 32-bit size word whose low 28
bits are the block length and whose top 4 bits `f` select the model: `f = 0` is model 1 with the legacy default table
size; otherwise `f & 7` is the log2 of the hash-table size minus 15, and `f & 8` selects model 2 (two more sparse
contexts, three record contexts, four mixers combined by a learned final mixer, and an extra order-2 APM). The arithmetic-coded bytes follow.
`f = 8` exactly is the extended header: one more byte follows the size word, `model << 4 | (log2 of the table size − 15)`;
it carries model 3 (orders 12 and 20, the byte the match model expects as a context, finer classes for long matches).
Encoders write model 3 for text, model 2 for other data, and model 1 at level 1 and for blocks of 2^28 bytes or more;
decoders accept all three, so files written with an older model still decode. **Learning settings (format 2, rc3):** a
model nibble of 14 or 15 (models 2 and 3, exact logistic tables) is followed by three more bytes before the coded bytes:
`b0 b1 b2`. Each 2-bit field of `b0` and `b1` selects one learning constant (the first value is the constant of every
other stream): `b0` bits 0-1 mixer rate shift 12, 11, 13, 14; bits 2-3 counter limit 2, 1, 3, 4; bits 4-5 APM rate shift
13, 12, 14, 15; bits 6-7 match-model rate shift 6, 5, 7, 8; `b1` bits 0-1 floor of the decaying mixer rate (of 64) 20,
12, 28, 40; bits 2-3 its decay period 2^13, 2^12, 2^14, 2^16 bytes; bits 4-5 state-map count limit 255, 127, 511, 1023;
bits 6-7 weights (of 8) of the mixer and the three APMs in the final probability 2,2,2,2 / 4,1,1,2 / 1,2,2,3 /
3,1,2,2. `b2` is the initial mixer weights: 0 = every weight 1/4, 1 = the shared prior of the model (the table
`cm_prior` in `src/codecs/cm_prior.inc`: one row per mixer, copied to every row of that mixer); other values are
refused. `-x` writes these streams (the settings are chosen by a race of presets on the stream's first 256 KiB,
`docs/TWOPASS.md`); a frame that holds one, as a block or as a graph leaf, is version 11. `CMI` (codec 17) is the same coder with
an image model for 8-bit samples: `iw u32 | ih u32 | ich u8` (row width in bytes, bytes before the first pixel,
interleaved channels), then a size word as above and the coded bytes. CM output is bit-exact across compilers and
CPUs (integer-only, `-ffp-contract=off`). `FQFAM` (codec 16) is described in *FASTQ family blocks*, `GR6` (codec 26) in
*GR6+ blocks*.

## Decode working set (header byte 11)

Every frame rc3 writes declares in `memd` what decoding one block needs, so that a reader knows the figure before it
allocates anything (rule 1 of `docs/MEMORY-MODEL.md`). The figure of block *i* is a function of its index entry and
the head of its payload, the same on both sides (`blk_dec_need`, `src/core/memneed.c`):

- the buffers every block uses: `2 × raw + raw / 8 + 16384 + 1 MiB` (the decoded block, the transform buffer, small state);
- plus the codec's working set: 0 for stored, LZ4, Huffman and rANS blocks; 2 MiB for zstd; `min(t, 16 MiB) + 2 MiB`
  for Brotli; the LZMA2 dictionary for `t` bytes (the power of two at or above `t`, 4 KiB to 64 MiB) + 2 MiB; `5 t + 4 MiB`
  for block sorting (codecs 5 and 18); for context mixing (codecs 7, 17 and each part of 25) exactly the tables its
  stream header selects plus model 3's match chain (`4 × (t + 8)`), the largest part for codec 25 (parts are decoded as
  separate units of work); measured bounds for the FASTQ family coder (`4 raw + 32 MiB`), GR6+ (`6 raw + 64 MiB`), audio
  (`3 raw + 16 MiB`) and graph blocks (`6 raw + 8 MiB`, plus the context-mixing bound of `2 raw + 64 KiB` bytes when the
  frame's mode is ultra, whose level 2 may code leaves with context mixing);

where `t` is the codec's output (the block, or the transformed block up to `raw + raw / 8 + 16384`). `memd` codes the
largest figure of the frame, rounded up. A video frame (codec 20) declares the nested frame's figure plus `2 × orig`
and the layout.

A reader:
- rejects a frame whose largest block's buffers exceed `memd` (when `memd` is not 0);
- before decoding, compares `memd` (or, when it is 0, the largest block's buffers) with its memory limit and stops with
  `KOLMO_ERR_MEMORY` if it does not fit;
- rejects a block whose figure exceeds `memd`, and a context-mixing stream whose tables exceed it, before allocating
  them; in a frame without a declaration it holds them to its memory limit instead.

The field needs no version of its own: a reader that ignores it decodes correctly (it only removes checks), so rc3
writes it in frames of every version it writes, and format 2 (version 11) readers enforce it. The decoded bytes, and
every other byte of the frame, do not depend on the memory free when it was written (*Memory changes speed, never
bytes*, `docs/MEMORY-MODEL.md`): `memd` itself is computed from the payloads.

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
| 11 | format 2: a block uses codec 19 (audio) or 20 (video), a graph leaf uses codec 21, 22 or 23, a table block uses the larger transform slack, the frame was written with a trained profile (flag bit 2; see *Profile frames*), or a block is a GR6+ block (codec 26; `ultra` only) | rc3 |

Highest version read: 5 by 1.0.0 and 1.0.1; 8 by 1.1.0; 9 by 1.2.0-rc1 and rc2; 10 by the FASTQ branch; 11 by rc3. (1.1.0 is the state of `main` under that
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
| 18 | BWT (Kolmo block sorting: LZP, BWT, adaptive QLFC) | blocks and graph leaves, version 11 or later; written instead of BSC (5) by `ratio`, `max -l 1` and `ultra`; BSC stays readable | rc3 (bwt lane) |
| 19 | AUD (kaud: 16-bit PCM audio) | blocks, version 11; written for 16-bit PCM WAV files by every mode but `rt` when smaller | rc3 |
| 20 | VID (kvid layout + nested frame) | the only block of a version 11 frame; written for 8-bit Y4M files by every mode but `rt` | rc3 |
| 21 | PLANES (per-plane leaf) | graph leaves, version 11 | rc3 |
| 22 | HZ (Huffman, then zstd) | graph leaves, version 11; also inside codecs 21 and 23 | rc3 |
| 23 | ZRUN (zero runs and literals) | graph leaves, version 11; also inside codec 21 | rc3 |
| 24 | KLZMA (Kolmo's LZMA-class coder) | blocks and graph leaves, version 11 or later; written only with the opt-in `KOLMO_NATIVE=lzma` (or `strong`); payload in `src/algo/klzma.h` | rc3 wave 2 (native lane) |
| 25 | reserved for the rc3 lanes (`research/RC3-PLAN.md`) | — | — |
| 26 | GR6 (GR6+ line-structured text) | blocks, version 11 or later; written by `ultra` (not level 1) | rc3 |
| 27 | KHUF (Kolmo Huffman; codec 13's chunk framing) | graph leaves, version 11 or later; opt-in `KOLMO_NATIVE=huf`; format in `src/algo/khuf.h` | rc3 wave 2 |
| 28 | KLZ (byte-aligned LZ77) | blocks and graph leaves, version 11 or later; opt-in `KOLMO_NATIVE=lz`; format in `src/algo/klz.h` | rc3 wave 2 |
| 29 | KLZH (LZ77 with Huffman-coded streams) | blocks and graph leaves, version 11 or later; opt-in `KOLMO_NATIVE=lz`; format in `src/algo/klz.h` | rc3 wave 2 |

**ARM64 branch filter (plan type 11, tflag 14, version 11; rc3 wave 2)**: the plan prefix is `11, a, b, 0`, where
`a | (b & 15) << 8` is the block's *page phase* P (0-4095). The payload is the coded stream of the filtered block,
with no transform header (the filtered block has the block's length). To decode, undo the coder, then for every
4-byte little-endian word w at offsets i = 0, 4, 8, ... of the block (a tail of 1-3 bytes is left as it is), with
pc = P + i (32-bit arithmetic):
- BL (`w >> 26 == 0x25`): w = 0x94000000 | ((w - (pc >> 2)) & 0x03FFFFFF);
- ADRP (`(w & 0x9F000000) == 0x90000000`): s = ((w >> 29) & 3) | ((w >> 3) & 0x001FFFFC); if
  `(s + 0x00020000) & 0x001C0000` is 0, d = s - (pc >> 12) and w = (w & 0x9000001F) | (d & 3) << 29 |
  (d & 0x0003FFFC) << 3 | ((0 - (d & 0x00020000)) & 0x00E00000); otherwise w is unchanged.
The encoder applies the same with + in place of - (the XZ Utils ARM64 BCJ mapping, a bijection on 32-bit words). It
writes this plan for the code sections of ARM64 executables (ELF `EM_AARCH64`, Mach-O `CPU_TYPE_ARM64`, PE `0xAA64`),
which it reads from their section tables (`docs/METADATA.md`); P is the block's offset in the executable modulo 4096.
A decoder rejects tflag 14 under any other plan type, plan type 11 with another non-zero tflag, and plan type 11 in a
frame below version 11.

**Audio blocks of AIFF files (codec 19, plan prefix `0, 1, 0, 0`; rc3 wave 2)**: as any codec 19 block, except that
the file's samples are big-endian. The KAU1 stream codes the block with the bytes of every sample in the sample range
swapped (16-bit little-endian, as for WAV); after decoding it, the decoder swaps back the bytes of the 2 x channels x
frames bytes that follow the H head bytes (channels at byte 6 of the KAU1 header, frames u64 at bytes 22-29, H u32 at
bytes 37-40). A plan prefix byte `a` other than 0 or 1 is rejected.

**Block flags added in rc3**: flag 13 under plan type 3 (column-split table, *Container extensions*), written only with
`KOLMO_T_COLUMNS`; the container version is not raised (identifiers rule above), readers before rc3 refuse the block.
**Extension tags**: `IDX1` (column and row index), `DDP1` (chunk store); reserved for the features-b lane in
`research/RC3-PLAN.md`.

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
| 2, 3 with learning settings and the shared prior | `f` = 8, then one byte whose high nibble is 14 or 15 (model 2, 3), then three bytes | `ultra` streams of codec 7 and 25, and codec 7 graph leaves (`-x -l 2`), from rc3; the frame is version 11 | rc3 |

The exact logistic tables use the logistic curve round(4096 / (1 + e^(-x/256))) at every x in [-2048, 2047] instead of the
33 interpolated knots of the earlier models; stretch is its inverse (the smallest x whose squash reaches p). Readers before
1.2.0-rc2 refuse these streams (an unknown model nibble or channel count) with `KOLMO_ERR_CORRUPT`; 1.2.0-rc2 reads both kinds.

**What an older reader does.** Only versions 4 to 11 are announced in the header. Codec 18 is announced like codec 16: a frame with a codec 18 block, or a graph block whose leaves may use it, has version 11, which every rc2 reader refuses at the header. Codec 16 is announced: a frame
with a codec 16 block has version 10, so every earlier reader (1.2.0-rc2 and before) refuses it at the header with
`KOLMO_ERR_CORRUPT` ("corrupt or unsupported input") before decoding any block, and returns no bytes; frames without
codec 16 keep the version they had (the writer's output for them is unchanged). A version 10 reader rejects codec 16
in a frame of version 9 or lower, and as a graph leaf. Codec 26 is announced the same way, by version 11: a reader
rejects codec 26 in a frame of version 10 or lower, and as a graph leaf. The rest of this paragraph is about the identifiers that are
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
leaf    := stream index uv | codec u8 (0-7 as for blocks, 13, 14, 15 from version 9, 21-23 from version 11) | raw length uv | coded length uv | coded bytes
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

## Format 2 (container version 11)

rc3 writes version 11 for every frame that uses one of the codecs below; frames without them keep the version they
had. A reader rejects codecs 19-23 in a frame of version 10 or lower, codecs 21-23 as block codecs, and codecs 19 and
20 as graph leaves. Backward compatibility is not a goal of format 2: rc2 and earlier refuse version 11 at the header.

**Leaf codec 21: per-plane leaf.** For a leaf of `rl` bytes:

```
payload := np u8 (2-8; rl is a multiple of np) | plane * np
plane   := codec u8 | coded length uv | coded bytes      (plane b is raw bytes [b L, (b+1) L), L = rl / np)
```

A plane's codec is one of 0-7, 13, 14, 22, 23 (not 12, 15-21). The encoder writes it only for the residual stream of
an `INT` node with byte planes (flag bit 3), choosing per plane the smallest of the block codec, Huffman (13), rANS
(14; not at the fast levels; at `hc` only when 3% smaller than a non-rANS choice), Huffman then zstd (22), zero runs
(23, when at least half the plane's bytes are 0) and stored (0), and keeps the leaf only when it is smaller than the
leaf as a whole.

**Leaf codec 22: Huffman, then zstd.**

```
payload := h uv (length of the Huffman stream; h <= rl + rl/1024 + 64) | zstd frame of h bytes
```

The zstd frame (no dictionary) decodes to a codec 13 stream of `h` bytes, which decodes to the `rl` leaf bytes.

**Leaf codec 23: zero runs and literals.**

```
payload  := k uv (non-zero bytes, k <= rl)
            | runs: codec u8 | raw length r uv (r <= 10 (k + 1)) | coded length uv | coded bytes
            | literals: codec u8 | coded length uv | coded bytes      (raw length k; the coded bytes end the payload)
```

Sub-codecs are 0-7, 13, 14, 22 (not 21 or 23). `runs` holds k + 1 LEB128 varints: the number of zero bytes before
each non-zero byte, then the number after the last one; `literals` holds the k non-zero bytes. Decoding interleaves
them; a zero literal, a run past `rl` or bytes left over in either stream is an error (`src/algo/zerorun.c`).

**Plane-aligned Huffman and rANS chunks** (codecs 13 and 14 cut at plane boundaries, one-symbol rANS chunks) are
ordinary codec 13/14 leaves and need no version; from rc3 the encoder tries them by default at `hc`.

## Audio blocks (codec 19, version 11)

A block with codec 19 has plan prefix `0, 0, 0, 0` and tflag 0; its payload is one **KAU1** stream (the format of the
kaud research tool; `src/codecs/audio.c` is the library port, `src/experimental/KAUD.md` describes the stream in full):

```
"KAU1" | version u8 (1) | profile u8 (0, 1) | channels u8 (1, 2) | log2 chunk u8 (8-30) | frame u16 (16-32768)
| effort u8 | lpc order u8 | mu u16 * 3 | sign-sign step u16 | adaptation cap u8 (must be 7) | split depth u8 (0-5)
| sample frames u64 | stage-0 taps u16 | stage-1, stage-2 taps u8 * 2 | cross taps u8 * 2 | combiner step u8
| H u32 | H bytes (block bytes before the first sample) | T u32 | T bytes (after the last sample frame)
| CRC-32C of the block u32 | per chunk: payload length u32, CRC-32C of its PCM u32 | chunk payloads
```

The samples are 16-bit little-endian, interleaved; H + 2 x channels x frames + T must equal the block's raw length.
The encoder writes codec 19 for every block of a 16-bit PCM WAV file (mono or stereo, `WAVE_FORMAT_PCM` or
extensible PCM) that holds more than 1024 whole sample frames, when it is smaller than the block coded otherwise;
profile 0 up to `hc`, profile 1 at `max` and `ultra`. Block boundaries need not fall on sample frames: the bytes
before the first whole frame and after the last are H and T.

## Video frames (codec 20, version 11)

A frame whose only block has codec 20 is an 8-bit YUV4MPEG2 file (C420, C420jpeg, C420mpeg2, C420paldv, C422,
C444, Cmono) coded by the kvid front end (`src/codecs/video.c`). The block's raw length is the file size (so the file
is under 4 GiB), its plan prefix `0, 0, 0, 0`, tflag 0, and its checksum is over the file as usual. The payload after
the plan prefix is a complete Kolmo frame (any version, any number of blocks, not itself codec 20) of the **layout**:

```
layout := header length u32 | header bytes (stream header line and every FRAME line, in order)
          | predictor u8 per strip (16 rows of a plane), frames in order, planes in order within a frame
          | residuals of plane 0 of every frame, then plane 1, then plane 2
```

Predictor 0 is the LOCO-I median of the left, upper and upper-left samples in the same frame (the first row from the
left sample, the first column from the upper one, the very first sample from 0); 1 is the same sample of the previous
frame; 2 is the previous frame's sample plus the median of the differences (current minus previous, as int8) of the
left, upper and upper-left samples. Predictors 1 and 2 are invalid in the first frame. A residual is
zigzag((sample - prediction) mod 256 as int8). The writer takes this route for a whole Y4M file in every mode but
`rt`, coding the layout with the caller's mode and threads, when the result is smaller than the file. A reader decodes
the nested frame, inverts the layout and checks the block checksum; a range read decodes the whole file first.

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

## GR6+ blocks (codec 26, version 11)

A block with codec 26 is line-structured text (tables, logs, delimited records) coded field by field
(`src/codecs/gr6.c`; ported from branch `claude/dazzling-maxwell-p8y4u0`, commit `12f0c0e`, where it was codec 15 in
version 9). Its plan prefix is `0, 0, 0, 0` and its tflag 0; a decoder rejects anything else, and rejects codec 26 in a
file below version 11 or as a graph leaf. The block checksum covers the decoded block as usual. Its payload (after the
4-byte plan) is

```
payload  := version u8 (1) | separator u8 | final newline u8 (0/1) | formulas u8 | lines u64 le | table[62] |
            formula * formulas | arithmetic-coded bytes
formula  := target column u16 le | a u16 le | b u16 le | k u8 (0-18) | type u8 (0-5)        (a < target, b < target)
```

The block is read as `lines` lines (`\n`-terminated, the last one only when `final newline` is 1), each split into
fields at `separator` (one of `,` tab `;` `|` space). Per line the coder codes the field count (0 = same as the line
above, else count + 1), then per field one op and that op's values:

| op | name | values | field |
|---|---|---|---|
| 0 | DITTO | — | the field at the same position in the line above |
| 1 | EMPTY | — | empty |
| 2 | NUM | zigzag residual | prediction + residual, in the column's current number format |
| 3 | RANK | rank | entry `rank` of the column's move-to-front list (256 entries) |
| 4 | RANK2 | rank | entry `rank` of the move-to-front list of fields that followed the previous field (256 entries) |
| 5 | CDICT | index from the end | entry of the column's dictionary (distinct values in order of first use) |
| 6 | GID | id from the end | the block's distinct fields in order of first use (id 0 is the empty field) |
| 7 | NUMNEW | decimals, zero-padding width (each ≤ 18), zigzag value | a number in a new format |
| 8 | NEW | shared-prefix length, then characters | prefix of the reference field (the first entry of the RANK2 list, else of the RANK list), then characters: 6-bit codes into `table`, 62 = end, 63 = escape followed by two codes holding one byte |

A number is `-?[0-9]+(\.[0-9]+)?` with at most 18 digits that prints back identically (no `-0`, leading zeros only as a
fixed width). The NUM prediction is the one of four with the lowest running cost in the column (cost ← cost − cost/16 +
16 · bit length of the zigzag residual, updated after every number of the column's format): the column's formula, the
previous value, twice the previous minus the one before, and the previous plus half the last difference. Formula
types: 0 round(a / b · 10^k), 1 a − lag(a), 2 round(100 · (a / lag(a) − 1)), 3 lag(target) + a, 4 a, 5 round(a · b / 10^k),
all computed exactly in integers at the target's decimals, rounding half away from zero; `lag` is the column's
previous number. A formula applies only where its inputs on the same line are numbers.

Values are written as 6-bit units. An op is 5 data bits per unit plus a continuation bit. Any other value is its bit
length (one unit; 63 = escape, then 11 units of 6 bits) followed by the bits below the top bit, most significant first,
6 per unit. Every unit is coded as 6 binary decisions with an integer context-mixing model (8 hashed context models
over the op, column, ops and field above, previous field, residual sizes and characters; two logistic mixers; an APM)
and the same binary arithmetic coder and logistic tables as `CM`. As for `CM`, the model is defined by the reference
implementation (`src/codecs/gr6.c`) and is bit-exact across compilers and CPUs. `ultra` (levels 0 and 2) tries GR6+ on
text blocks with at least 64 lines and keeps it only when it is smaller than the block's other candidates (context
mixing, LZMA2 or BWT, with or without the planned transform). No other mode writes it.

## Container extensions (rc3, features-b lane)

A frame may be followed by **extension sections**: data about the frame that block decoding does not need. Block
decoding stops at the last payload (`32 + 16·nblocks + Σ comp`), so every reader decodes a frame with extensions
exactly as without them; the extensions are found from a fixed trailer at the end of the file.

```
frame | section * count | trailer
section := tag[4] | length u64 | body[length] | crc32c u32 (of tag, length and body)
trailer := "KEXT" | version u8 (1) | 0 u8 * 3 | count u32 | sections length u64 | crc32c u32 (of the 20 bytes before)
```

The trailer is the last 24 bytes of the file; `sections length` is the number of bytes between the end of the frame and
the trailer and must equal it exactly. A reader that looks for a tag walks every section, checks each one's length, and
checks the CRC-32C of the section it uses. A damaged trailer or section is an error for the feature that needs it
(`KOLMO_ERR_CORRUPT`, `KOLMO_ERR_CHECKSUM`), never for block decoding. Tags in use: `IDX1`, `DDP1`. Written by
`src/core/extension.c`.

### Column-split table blocks (plan type 3 TBL, block flag 13)

Written only on request (`KOLMO_T_COLUMNS`, set by `kolmo_compress_indexed`), in every mode but `rt`. The table-v2
stream of flag 4 (`src/tbl2.inc`) is a header (`k`, rows, exception rows, group column, formulas, exception list)
followed by one section per column (`type u8 | size u32 | section`). Flag 13 codes the header and each column section as
separate segments, so a reader decodes one column without the others. After the 4-byte plan prefix:

```
payload := tn u32 | nseg u16 (= k + 1) | entry * nseg | coded segments in order
entry   := codec u8 (0 stored, or the block's codec) | raw length u32 | coded length u32 | check u32
```

Segment 0 is the header up to and including the exception list, segment `j + 1` is column `j`'s section with its
5-byte prefix. `check` is the low 32 bits of XXH3-64 of the raw segment with seed `0x74626c63`, verified on every decode
(a partial decode cannot verify the block checksum). The raw lengths add up to `tn`, and the reassembled stream must
split into exactly these segments; it then decodes as flag 4, and the block checksum is verified as usual. To read
column `j`, a reader decodes segment 0 and segment `j + 1`, and also the segments of the formula inputs `a`, `b` when
column `j` uses mode 3, and the group column when column `j` or an input uses modes 2, 4, 5 or a lag formula.
Readers before rc3 refuse flag 13 (`KOLMO_ERR_CORRUPT`). Measured cost against flag 4: +2.0% on a dense numeric CSV and
+9.5% on a wide sparse one at `balanced`, -0.1% to +8.7% at `hc` and `ratio` (`benchmark/rc3/features-b/RESULTS.md`).

### Column and row index (`IDX1`)

Written by `kolmo_compress_indexed` (`src/core/index.c`), which compresses with `KOLMO_T_COLUMNS` (blocks of text end at
line ends where a line end exists in the second half of the block; delimited tables are column-split) and appends:

```
body    := "IX" | 1 | kind u8 | delim u8 | filter u8 (0, 1) | 0 u8 * 2 | nblocks u32 | rows u64
           | names uv | (length uv | bytes) * names
           | (flags u8 | lines uv) * nblocks
           | [ 3 | k u8 | (m uv | bitmap[m / 8]) * nblocks ]          (when filter = 1)
```

`kind` is 1 lines, 2 delimited table (`delim` is `,`, tab, `;` or `|`; 0 otherwise), 3 JSON lines. A row is a physical
line: a line starts at offset 0 of a non-empty input and after every `\n` that is not the last byte. `lines` counts the
line starts inside the block, `rows` is their sum, and `nblocks` must equal the frame's. Block `flags`: bit 0, the block
starts at a line start; bit 1, it ends with `\n` or at the end of the input. `names` are the fields of the first line of
a table (without a final `\r`; a field in double quotes is stored without them), 0 for other kinds.

Fields of a table line: with `,` and `;`, double quotes and square brackets group fields (as the table transform splits
them); field `k - 1`, where `k` is the number of names, runs to the end of the line. A JSON column is the raw value of
a top-level key; a `#N` column of other text is the N-th run of characters other than space, tab and `\r`.

The filter of a block is a bitmap of `m` bits (a multiple of 64; 0 = no filter, the block is always a candidate) over
the keys of the lines that start in the block, including the rest of the last one when it continues into later blocks:
every trigram of three bytes none of which is `\n`, ASCII letters folded to lower case, with hash
`h = (t · 0x9E3779B97F4A7C15) >> 32` for `t = c0 | c1 << 8 | c2 << 16`; and every whole token (a maximal run of ASCII
letters and digits, at least 3 long, folded), with `h` = FNV-1a 64 of the folded bytes from offset basis
`0xcbf29ce484222325 ^ 0x746f6b31`, then `h ^= h >> 29; h *= 0xbf58476d1ce4e5b9; h ^= h >> 32`, low 32 bits. A trigram
sets bit `(h · m) >> 32`; a token sets the bits of `h + j · ((h · 0x9E3779B1) | 1)` (32-bit arithmetic) for `j < k`. Bit
`i` is bit `i mod 8` of byte `i / 8`. A query rules out a block when a key its literal must contain is missing: the
trigrams of the literal, the tokens bounded inside the literal on both sides, and with a fixed whole-word pattern every
token. The CRC detects accidental damage; a deliberately forged index with a valid CRC can make a query skip blocks or
number rows wrongly, but a reader checks every block it decodes against its `flags` and `lines` and reads nothing outside
the file.

### Chunk store (`DDP1`)

Written by `kolmo_archive_create` (`src/core/dedup.c`). The frame is the **chunk store**: the distinct chunks of all
files, in the order first seen, compressed as an ordinary frame. Chunks are cut with a Gear hash (`h = (h << 1) +
G[byte]`, `G` from splitmix64 seeded `0x6b6f6c6d6f646470`) and FastCDC normalized chunking: for a target `A` (a power of
two from 256 B to 4 MiB), no cut before `A/4`, then a cut where `h & mask(log2 A + 2) = 0` up to `A`, then where
`h & mask(log2 A − 2) = 0` up to `8A`; in text files a cut moves on to the next `\n` within 4 KiB. These rules decide
only how well a writer deduplicates; a reader needs none of them.

```
body  := "DD" | 1 | flags u8 (bit 0: line cuts) | target u32 | table length u64 | table frame (CZN1)
table := chunks uv | (length uv | check u32) * chunks | files uv
         | (name length uv | name | size uv | mode uv | zigzag mtime uv | refs uv | zigzag(id - previous id - 1) uv * refs) * files
```

Chunk `i` occupies the store bytes after chunks `0 .. i−1`; lengths are at least 1 and add up to the store size.
`check` is the low 32 bits of XXH3-64 of the chunk with seed `0x64647031`, verified when the chunk is extracted. A file is
the concatenation of its chunks; their lengths must add up to its size. Names are at most 4096 bytes without NUL; the
command line extracts only relative names without a `..` component.
