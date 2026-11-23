# Kolmo frame format, container version 12

Status: **normative** for the container layer and for the coders of sections 14-19, version 12, as written and read by
Kolmo 0.0.1 and 0.0.2.
Compatibility promise: [COMPATIBILITY.md](COMPATIBILITY.md). Detailed history of versions 1 to 11 and design notes:
[SPEC.md](SPEC.md).

This document has three parts.

- **Container layer** (sections 1 to 12): complete. A decoder can be written from this text alone up to the point
  where it hands a block payload to a coder. It covers the header, the block index, the checksums, the plan prefix,
  the transform flags, stored blocks, the nesting of video frames, container extensions and the stream wrapper.
  `tests/format_reference.py` is an independent Python implementation of this layer, written from this text. It
  checks every frame of the frozen corpus and the `kolmo_khash64` test vectors.
- **Coder and transform payloads** (section 8): a registry of every identifier with the status of its specification.
  Several payloads are specified in [SPEC.md](SPEC.md). For the others, the reference implementation at tag `v0.0.2`
  is normative until their sections are written (section 13 lists them).
- **Kolmo's own coders** (sections 14 to 19): khuf (27), krans, KLZ (28), KLZH (29), KLZH2 (32), and the byte-plane
  shuffle inverse. `tests/coder_reference.py` is an independent Python decoder written from these sections. It decodes
  every block and graph leaf of the frozen corpus that uses them.

Throughout, *must* and *must not* are requirements on a conforming decoder or encoder.

## 1. Conventions

- All multi-byte integers are unsigned and little-endian unless a section says otherwise.
- Offsets are in bytes from the start of the frame.
- `u8`, `u32` and `u64` are 1-, 4- and 8-byte unsigned integers.
- `low32(x)` is `x mod 2^32`.
- Arithmetic on 64-bit values is modulo 2^64.
- `a × b → 128` is the full 128-bit product of two 64-bit values.

## 2. Primitives

### 2.1 CRC-32C

The CRC-32C (Castagnoli) of RFC 3720. It uses:
- the reflected polynomial `0x82F63B78`;
- initial value `0xFFFFFFFF`;
- a final XOR with `0xFFFFFFFF`.

The check value of the ASCII bytes `123456789` is `0xE3069283`. `crc32c(A ‖ B)` is the CRC of A followed by B.

### 2.2 kolmo_khash64

`khash64(p, n, seed)` maps `n` bytes and a 64-bit seed to 64 bits. Words are read little-endian: `r64(i)` is the `u64`
at byte `i` of `p`, and `r32(i)` the `u32`.

Constants:

```
G  = 0x9E3779B97F4A7C15
P1 = 0xD6E8FEB86659FD93
fold(a, b) = low64(a × b → 128) XOR high64(a × b → 128)
aval(h)    = h ^= h >> 32; h *= P1; h ^= h >> 32; h *= P1; h ^= h >> 32; return h
mix64(z)   = z = (z ^ z >> 30) * 0xBF58476D1CE4E5B9; z = (z ^ z >> 27) * 0x94D049BB133111EB; return z ^ z >> 31
K0[i]      = mix64((i + 1) * G)            for i = 0 .. 15
```

Algorithm, with `L = n * G` and `S = seed * P1`:

1. **n ≤ 16.** Set `a` and `b` by length:

   | length | a | b |
   |---|---|---|
   | n ≥ 8 | `r64(0)` | `r64(n − 8)` |
   | 4 ≤ n < 8 | `r32(0)` | `r32(n − 4)` |
   | 1 ≤ n < 4 | `p[0] | p[n >> 1] << 8 | p[n − 1] << 16` | 0 |
   | n = 0 | 0 | 0 |

   Return `aval(fold(a ^ K0[0] ^ S, b ^ K0[1] ^ L) ^ seed ^ n)`.
2. **17 ≤ n ≤ 64.**
   - Start with `h = fold(r64(0) ^ K0[0] ^ S, r64(8) ^ K0[1]) + fold(r64(n−16) ^ K0[2], r64(n−8) ^ K0[3] ^ L)`.
   - If n > 32, add `fold(r64(16) ^ K0[4], r64(24) ^ K0[5] ^ S) + fold(r64(n−32) ^ K0[6], r64(n−24) ^ K0[7])`.
   - Return `aval(h ^ seed ^ n)`.
3. **n > 64.**
   - Key: `K[i] = K0[i] + S` for even `i` and `K0[i] − S` for odd `i` (i = 0..15), then `K[16 + i] = K[i]` for i = 0..7.
   - Lanes: `acc[i] = K[8 + i] ^ K[i]` for i = 0..7.
   - A *stripe* at offset `o` with rotation `t` does, for each lane i = 0..7: `v = r64(o + 8i)`, `d = v ^ K[t + i]`,
     `acc[i] += low32(d) * (d >> 32) + v`, where the product is a 32×32→64-bit product.
   - Let `ns = (n − 1) / 64` (integer division) and `s = 0`.
     - While `s + 16 ≤ ns`: run the stripes at `(s + t) * 64` with rotation `t`, for t = 0..15. Then scramble every
       lane, `acc[i] = (acc[i] ^ acc[i] >> 47 ^ K[8 + i]) * 0x9E3779B1`, and set `s += 16`.
     - While `s < ns`: run the stripe at `s * 64` with rotation `s mod 16`, then `s += 1`.
   - Tail: run the stripe at `n − 64` with rotation `ns mod 16`. It overlaps the previous stripe when n is not a
     multiple of 64.
   - Final: `h = L ^ seed`; for i = 0, 2, 4, 6: `h += fold(acc[i] ^ K[8 + i], acc[i+1] ^ K[9 + i])`; return `aval(h)`.

`tests/khash64_vectors.txt` lists `n seed khash64` for 31 lengths and 3 seeds. Every length class and the 1 KiB
scramble are covered. The input byte `i` is `(31 i + 7) mod 256`.

The format uses only the low 32 bits of khash64, with these seeds:

| Use | Seed |
|---|---|
| block checksums | `0x637a6e31` |
| block checksums in a frame with flag bit 0 | `0x637a6e31 ^ u32(plan bytes)` |
| codec 5's payload hash | `0x62736331` |
| column-split table segments | `0x74626c63` |
| DDP1 chunk checks | `0x64647031` |

## 3. Frame layout

```
+----------------+-------------------------+----------------------------+----------------------------+
| header (32 B)  | block index (16 B each) | block payloads, in order   | extensions (optional, §9)  |
+----------------+-------------------------+----------------------------+----------------------------+
```

The payload of block *i* starts at `32 + 16·nblocks + Σ_{j<i} comp[j]` and is `comp[i]` bytes long. Blocks cover the
original data in order: block *i* decodes to bytes `Σ_{j<i} raw[j]` up to `Σ_{j≤i} raw[j]` of the original. Blocks are
independent, so a decoder may decode them in any order or in parallel, and may decode only the blocks that overlap a
requested byte range.

## 4. Header (32 bytes)

| Off | Size | Field | Value in version 12 |
|---|---|---|---|
| 0 | 4 | magic | ASCII `CZN1` |
| 4 | 1 | version | 12 |
| 5 | 1 | mode | 0 to 5: the mode the encoder ran in (rt, fast, eco, hc, max, ultra). Not checked by decoders; its only effect on decoding is the working-set bound of graph blocks (§11) |
| 6 | 4 | reserved | written as 0; ignored by decoders |
| 10 | 1 | flags | bit 0: block checksums cover the stored payload (§5.1). Bits 1 to 7: 0 |
| 11 | 1 | memd | declared decode working set per block (§11); 0 = not declared |
| 12 | 8 | orig | original length in bytes; below 2^63 |
| 20 | 4 | nblocks | number of blocks (0 for empty input) |
| 24 | 4 | block | nominal block size; informational |
| 28 | 4 | crc | CRC-32C of header bytes 0..27 followed by the whole block index |

**How a decoder reads the version byte.**
- 12: read the frame.
- 1 to 11: refuse it as *legacy*. These are frames of earlier Kolmo builds, which used library codecs and other hashes.
- Anything else: refuse it as *corrupt*.
- A file that starts with `CZC1` is also legacy.

The version is checked before any checksum.

Reserved fields that decoders ignore can never carry information a decoder needs. A later feature that needs new
information signals it with an identifier that 0.0.2 decoders refuse (COMPATIBILITY.md).

## 5. Block index

`nblocks` entries of 16 bytes follow the header.

| Off | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | raw | decoded length of the block |
| 4 | 4 | comp | stored length of its payload |
| 8 | 4 | checksum | §5.1 |
| 12 | 1 | codec | §8 |
| 13 | 1 | tflag | the transform applied before the coder (§6.2) |
| 14 | 2 | reserved | written as 0; ignored by decoders |

A decoder must refuse the frame, before decoding any block, if any of these holds:
- the index does not fit in the input;
- the CRC does not match;
- `Σ raw ≠ orig`;
- a payload extends past the input;
- a stored block (codec 0) has `comp ≠ raw + 4`;
- `memd` is declared and some block's buffers (§11) exceed it.

### 5.1 Block checksum

- **Flag bit 0 clear:** `checksum = low32(khash64(decoded block, 0x637a6e31))`, checked after the block is decoded.
- **Flag bit 0 set** (written at level 1): `checksum = low32(khash64(payload[4..comp), 0x637a6e31 ^ u32(payload[0..4))))`,
  checked before the block is decoded. `u32(payload[0..4))` is the plan prefix read as a little-endian `u32`.

A mismatch is a checksum error. A decoder must never return the bytes of a block whose checksum fails.

## 6. Block payload

### 6.1 Plan prefix

Every payload starts with 4 bytes `t, a, b, c`, the **plan**. `t` is the plan type (below); `a`, `b` and `c` are its
parameters. Unless a codec in §7 says otherwise, the rest of the payload is:

```
meta (0, 4, 8 or 4·(c+1) bytes, by tflag) | coded stream
```

A decoder must refuse `t > 11`, and plan type 11 with `b > 15` or `c ≠ 0`.

| t | Plan | Allowed tflag | Meta | Transformed length bound |
|---|---|---|---|---|
| 0 | none | 0 | — | — |
| 1 | typed transpose of delimited text (a: delimiter, c + 1: columns) | 0, 1, 2 | `4·(c+1)`: column sizes, `u32` each (tflag 1 or 2) | raw |
| 2 | byte-plane shuffle (a: element width 2, 4 or 8; b: channels 1-8; c: delta) | 0, 1 | — | raw |
| 3 | table | 0, 3 (table v1), 4 (table v2), 13 (column-split, §8.3) | 4: transformed length | raw + 16384 (tflag 3); raw + ⌊raw/8⌋ + 16384 (tflag 4) |
| 4 | log templates (a: tokenizer, 0 words, 1 JSON, 2 minified JSON) | 0, 5, 10 | 4 (tflag 5); 8 (tflag 10) | raw + 16384 |
| 5 | 16-bit image | 0, 6 | 4 | raw + 16384 |
| 6 | fixed-size records | 0, 7 | 4 | raw + 16384 |
| 7 | x86 call/jump filter | 0, 8 | — | in place |
| 8 | word dictionary | 0, 9 | 4 | raw + 16384 |
| 9 | kolmo graph (codec 12 only, §7.4) | 0 | — | — |
| 10 | line classes | 0, 12 | 4 | raw + 16384 |
| 11 | ARM64 branch filter (a, b: page phase) | 0, 14 | — | in place |

**Rules for tflag.**
- tflag 0 means the plan was chosen but no transform was applied. The coded stream decodes to the block itself.
- Any other combination of plan type and tflag must be refused.
- The *transformed length* is the `u32` meta field of tflags 3-7, 9, 10 and 12. A value above the bound must be
  refused.
- tflag 10 also carries a second `u32`: the number of newlines the encoder inserted. They are removed after the
  inverse transform (SPEC.md, *JSON*).

### 6.2 Decoding a block of the general form

1. Read the plan and the meta.
2. Decode the coded stream with the block's codec (§8), to the transformed length or, with no transform, to `raw` bytes.
3. Undo the transform named by `tflag`, giving exactly `raw` bytes.
   - tflags 8 and 14 work in place on the decoded block.
   - The ARM64 inverse is specified in SPEC.md, *ARM64 branch filter*, and the byte-plane shuffle in §19.1.
   - The other inverses are in the reference implementation (§13).
4. Verify the checksum (§5.1).

## 7. Blocks with their own layout

### 7.1 Stored (codec 0)

The plan must be `0,0,0,0`, tflag 0 and `comp = raw + 4`. The payload after the plan is the block's bytes.

### 7.2 Self-contained coders (codecs 16, 19, 26, 31)

- The plan must be `0,0,0,0` and tflag 0. One exception: codec 19 also allows plan `0,1,0,0`.
- The payload after the plan is the coder's stream, which decodes to exactly `raw` bytes.
- For codec 19 with plan `0,1,0,0` (AIFF), the decoder then byte-swaps the 16-bit samples back (SPEC.md, *Audio
  blocks of AIFF files*).

### 7.3 Video frame (codec 20)

A frame whose single block has codec 20 is a video frame. It is subject to these rules:
- `nblocks = 1`, `raw = orig`, flag bit 0 clear and tflag 0;
- the plan is `0,0,0,0`;
- the rest of the payload is a complete nested version-12 frame;
- that nested frame must not itself be a video frame.

The nested frame decodes to the kvid layout, which the kvid inverse turns back into the original Y4M bytes (SPEC.md,
*Video frames*). The outer block's checksum covers the original. Codec 20 anywhere else is corrupt.

### 7.4 Graph block (codec 12)

Codec 12 must have plan type 9 and tflag 0, and plan type 9 must have codec 12. The payload after the plan is a graph
program: nodes, and leaves coded by leaf codecs. Its layout is in SPEC.md, *Kolmo graph blocks*. A decoder must refuse
a node or leaf codec it does not know.

## 8. Codec registry

### 8.1 Block and leaf codecs

These are the codec ids of version 12. *Written* means the 0.0.2 encoder writes the id with default options or a
documented CLI option. *Read only* means it decodes the id but does not write it. An id not in this table must be
refused (corrupt).

| Id | Name | Where | 0.0.2 | Payload specification |
|---|---|---|---|---|
| 0 | STORE | block, leaf | written | §7.1 |
| 5 | BSC (libbsc 3.4.0 block format) | block, leaf | read only | SPEC.md *Entropy coders*; `src/codecs/bwt.c` |
| 7 | CM (context mixing, models 1-4) | block, leaf | written (ultra) | SPEC.md *Entropy coders*, *CM models*; `src/codecs/cm.c` |
| 12 | GRAPH | block | written | SPEC.md *Kolmo graph blocks* |
| 14 | RANS (order 0) | leaf | written | SPEC.md *Leaf codec 14*; `src/codecs/rans.c` |
| 15 | NPLANE | leaf | written (`--native-leaf`) | SPEC.md *Leaf codec 15* |
| 16 | FQFAM (FASTQ family) | block | written (ultra) | SPEC.md *FASTQ family blocks*; `src/codecs/fastq.c` |
| 17 | CMI (context mixing, image model) | block | written (ultra) | SPEC.md *Entropy coders*; `src/codecs/cm_image.c` |
| 18 | BWT (Kolmo block sorting) | block, leaf | written | SPEC.md *Entropy coders* (stream header); `src/codecs/bwt_stream.c`, `src/algo/qlfc.c`, `src/algo/lzp.c` |
| 19 | AUD (kaud, 16-bit PCM) | block | written | SPEC.md *Audio blocks*; `src/codecs/audio.c` |
| 20 | VID (kvid layout + nested frame) | block | written | §7.3; SPEC.md *Video frames*; `src/codecs/video.c` |
| 21 | PLANES | leaf | written | SPEC.md *Format 2*; `src/codecs/planes_leaf.c` |
| 23 | ZRUN | leaf | written | SPEC.md *Format 2*; `src/codecs/zrun_leaf.c` |
| 24 | KLZMA | block, leaf | written | `src/algo/klzma.h`, `src/algo/klzma.c` |
| 25 | CMP (context mixing over parts) | block | written (ultra) | `src/codecs/cm_par.c` |
| 26 | GR6 (GR6+ line-structured text) | block | written (ultra) | SPEC.md *GR6+ blocks*; `src/codecs/gr6.c` |
| 27 | KHUF (Kolmo Huffman) | leaf | written | **§15** |
| 28 | KLZ (byte-aligned LZ77) | block, leaf | written (level 1) | **§17** |
| 29 | KLZH (LZ77 + Huffman) | block, leaf | read only | **§18.5** |
| 31 | KIMG (8-bit images) | block | written (levels 8, 9, ultra) | `src/codecs/kimg.c` |
| 32 | KLZH2 (LZ77 + Huffman or static rANS per stream) | block, leaf | written (levels 2-7) | **§18**, with krans §16 and khuf §15 |

**Retired ids, never valid in version 12:**
- 1-4, 6, 13 and 22 were library codecs of versions 1 to 11;
- 8-11 are experimental codecs of builds made with `-DKOLMO_FRONTIER`, outside this format;
- 30 is held for a later khuf.

Ids 15, 21 and 23 are valid only as graph leaves, and 20 only as in §7.3.

### 8.2 Other identifiers

The following identifiers are specified in SPEC.md and keep the meanings given there:
- graph node ids 1-7;
- the CM model nibbles and their registry;
- the BWT stream flags;
- the plan parameters of each transform.

A decoder must refuse a value that section does not assign.

### 8.3 Column-split table blocks

Plan type 3 with tflag 13 is written only on request (`KOLMO_T_COLUMNS`, used by indexed archives). Its payload is
specified in SPEC.md, *Column-split table blocks*.

## 9. Container extensions

Bytes after the last payload must be either nothing or extension sections followed by a trailer:

```
section := tag[4] | length u64 | body[length] | crc32c(tag ‖ length ‖ body) u32
trailer := "KEXT" | 1 u8 | 0 u8 0 u8 0 u8 | count u32 | sections_length u64 | crc32c(first 20 trailer bytes) u32
```

- The trailer is the last 24 bytes of the file.
- `sections_length` must equal the number of bytes between the last payload and the trailer, and the sections' lengths
  must add up to it exactly. A decoder checks this, and the trailer's CRC, before decoding the frame.
- A section's CRC is checked by the feature that reads that section.
- Block decoding never depends on extensions, and a damaged extension is an error only for the feature that reads it.
- Tags in use: `IDX1` (column and row index) and `DDP1` (chunk store). Both are specified in SPEC.md, *Container
  extensions*.

## 10. Stream wrapper (`CZNS`)

For bounded memory and pipes (`--stream`), a stream wraps complete frames:

```
"CZNS" | 1 u8 | 0 u8 0 u8 0 u8 | segment_bytes u32 | ( length u64 | frame[length] )* | 0 u64
```

- Each frame is an independent version-12 frame of one segment of the input. The original is the frames' originals
  concatenated.
- A zero length ends the stream.
- A decoder tells a stream from a frame by the first four bytes.

## 11. Decode working set (header byte 11)

When `memd` is not zero, the declared bytes are `steps[memd mod 8] << (memd >> 3)`, where:

```
steps = 1048576, 1143481, 1246975, 1359835, 1482911, 1617126, 1763488, 1923097
```

This gives about 2^(memd/8) MiB.

- **Buffer floor.** Every block needs at least `2·raw + ⌊raw/8⌋ + 16384 + 1 MiB` for its buffers. A frame where that
  figure exceeds the declared bytes for some block must be refused.
- **Codec working set.** A decoder may also refuse a block whose codec working set exceeds the declaration (SPEC.md,
  *Decode working set*), and should refuse before allocating it.
- **Mode byte.** The declaration covers the context-mixing graph leaves only in frames whose mode byte is 5 (ultra).
- **Declared memory is never needed for correctness.** A decoder that ignores `memd` decodes correctly.
- **Encoders.** The encoder writes the smallest `memd` whose declared bytes are at least the largest per-block figure.

## 12. Conformance

**A conforming decoder must:**
- decode every frame of `tests/v12` to its original;
- refuse every frame that breaks a rule of sections 4 to 11, with an error;
- never return bytes from a block that fails its checksum, or from a frame whose header or index fails.

Errors fall into five classes:
- corrupt;
- checksum;
- legacy;
- a caller's limit;
- memory.

**A conforming encoder** writes only identifiers this document assigns, and frames that pass `tests/format_reference.py`.

**Test material:**

| File | Contents |
|---|---|
| `tests/v12/` | 84 frames written in the six modes of 0.0.1 (top level) and 246 frames written by 0.0.2 (`r0.0.2/`), each with its original. `r0.0.2/frames.json` gives each frame's options and SHA-256, the original's SHA-256 and a summary of its blocks |
| `tests/khash64_vectors.txt` | the khash64 vectors |
| `tests/format_reference.py` | the container layer in Python, checked against both of the above |
| `tests/coder_reference.py` | the coders of sections 14 to 19 in Python, decoding every corpus block and leaf that uses them |

## 13. What is not yet written out

These payloads are specified here only by the reference implementation at tag `v0.0.2`. Its C sources decode them,
and the frozen corpus pins their behaviour on real data. Writing each one out as prose is the next step of the format
work, in this order (most frames first):

1. the table, log, line-class and word transforms (tflags 4, 5, 9, 10, 12);
2. KLZMA (24);
3. the BWT stream's QLFC coder (18);
4. the typed and quote-aware transposes (plan 1, tflags 1 and 2), and the record, 16-bit image and x86 inverses
   (tflags 6, 7, 8);
5. kimg (31);
6. CMP (25).

Written out so far: KLZH2 (32) with krans, KLZ (28), KLZH (29) and khuf (27) in sections 14 to 18, and the byte-plane
shuffle (plan 2, tflag 1) in section 19.

Until a payload has its own section, a change to its decoder that changes the decoded bytes of any version-12 frame is
a format change (COMPATIBILITY.md), whether or not the corpus catches it.

## 14. Native coders: common rules

Sections 14 to 18 specify Kolmo's own coders: khuf (27), krans (inside 32), KLZ (28), KLZH (29) and KLZH2 (32). A
coder is called with a byte string `in` (its *stream*) and the number `n` of bytes it must produce. For a block, the
stream is the payload after the plan and the meta (§6.1), and `n` is the transformed length, or `raw` when there is no
transform. For a graph leaf, the stream is the leaf's coded bytes and `n` its raw length (SPEC.md, *Kolmo graph
blocks*). A coder either produces exactly `n` bytes or refuses the stream as corrupt. It never reads outside `in`.

`tests/coder_reference.py` is an independent Python decoder written from sections 14 to 18. It decodes every block
and graph leaf of the frozen corpus that uses these coders (§18.4).

### 14.1 Varints

- **uv64** is unsigned LEB128: bytes are read in order, byte `k` (from 0) contributes `(byte & 127) << 7k`, and the
  first byte below 128 ends the varint. At most 10 bytes are read. The value is taken modulo 2^64, so the tenth byte
  contributes only its lowest bit.
  - Refuse when the stream ends inside the varint.
  - Refuse when the tenth byte still has bit 7 set.
- **uv32** is the same with at most 5 bytes and the value taken modulo 2^32.
- Overlong encodings, such as `0x80 0x00` for 0, are valid and decode to their value. Encoders write the shortest form.

### 14.2 Match copy

A match `(offset, length)` appends `length` bytes to the output. Each one is the byte `offset` positions before the
output's current end, read after the previous byte of the same match was appended. Matches may overlap their own
output: offset 1 repeats the last byte `length` times.

- Refuse an offset of 0, or an offset larger than the number of bytes already produced by this coder call.
- Refuse a length that would take the output past `n`.
- The window is the coder call's own output. Nothing from earlier blocks or leaves is visible.

## 15. khuf (codec 27): Huffman chunks

Codec 27 is a sequence of **chunks**. Each chunk is decoded in turn and appended to the output, until the output has
`n` bytes:

```
chunk := mode u8 | m uv64 | body
  mode 0 (stored): m bytes
  mode 1 (run):    one byte, repeated m times
  mode 2 (coded):  z uv64 | a khuf block of z bytes, which decodes to m bytes (15.1)
```

Refuse when:
- `mode` > 2;
- `m` is 0, larger than 131072 (128 KiB), or larger than the bytes still missing from the output;
- a stored body, run byte or coded block reaches past the end of the stream (`z` larger than what is left);
- the khuf block is refused;
- bytes are left in the stream after the output reaches `n` bytes. With `n = 0` the stream must therefore be empty.

The encoder writes chunks of 128 KiB except the last one. Inside KLZH2 it also cuts them at other points (§18.3), so
a decoder must accept any `m` within the rules above.

### 15.1 khuf block

A khuf block codes `m` bytes, 2 ≤ m ≤ 131072, with a canonical prefix code of at most 11 bits per symbol, in four
interleaved bit streams:

```
block := table | L0 uv64 | L1 uv64 | L2 uv64 | stream 0 (L0 bytes) | stream 1 (L1) | stream 2 (L2) | stream 3 (the rest)
```

Refuse `m` < 2 or `m` > 131072.

**Table.** The first byte is `k`; the code has `c = k + 1` symbols.
- **Sparse** (`k` < 48): `c` pairs `symbol u8, length u8` follow, in any order. Refuse when:
  - the pairs reach past the end;
  - a symbol appears twice;
  - a length is 0 or above 11;
  - `c` = 1.
- **Dense** (`k` ≥ 48): a byte `last` follows, then `⌊(last + 2) / 2⌋` bytes of 4-bit lengths for symbols 0 to
  `last`, two per byte, the even symbol in the low nibble. When `last` is even, the high nibble of the final byte is
  ignored. Symbols above `last` have length 0, and length 0 means absent. Refuse when:
  - the bytes reach past the end;
  - a length is above 11;
  - the number of non-zero lengths is not `c`.
- In both forms, refuse unless the code is complete: the sum of `2^(11 − length)` over present symbols must be exactly
  `2^11`.

**Canonical codes.** Assign codes as in DEFLATE (RFC 1951, 3.2.2):
1. Order the present symbols by length, then by symbol value.
2. Number them with consecutive integers. Before each increase in length, shift the counter left by one bit for every
   length step.

Formally, with `count[l]` the number of symbols of length `l`:
- `next[1] = 0`;
- `next[l] = (next[l−1] + count[l−1]) << 1`;
- each symbol, in increasing symbol order, takes `next[length]++`.

**Bit order.** Each stream is read as a sequence of bits: byte 0 first and, within a byte, the least significant bit
first. A code is matched most significant bit first: the first bit read is the top bit of the `length`-bit code.

**Streams.** Let `seg = ⌈m / 4⌉`. Stream `q` (0-3) decodes output bytes `[min(m, q·seg), min(m, (q+1)·seg))`. A stream
may therefore code no symbols. Read the three lengths as uv64, then refuse when:
- one of them is larger than the block;
- `L0 + L1 + L2` is larger than what follows them.

`L3` is the rest of the block. Then refuse when any `Lq` is greater than `⌊seg · 11 / 8⌋ + 2`.

To decode stream `q`, decode its symbols one by one from its bits. A code may need bits past the stream's end. Those
bits never change a decoded symbol, because the code is complete, but they do count as consumed. Let `b` be the bits
the stream's symbols take in total. Refuse when:
- `b` > `8 · Lq`;
- `Lq` > 0 and `b` ≤ `8 · (Lq − 1)`, so a whole byte is unused;
- `Lq` = 0 and `b` > 0.

Bits after the last symbol in the stream's final byte are ignored.

## 16. krans: static order-0 rANS

krans codes `n` ≥ 1 symbols (bytes) with eight interleaved rANS states, 12-bit probabilities and 16-bit
renormalisation. It appears only inside KLZH2 (§18.3).

```
krans := top u8 | f[0] uv32 … f[top] uv32 | x[0] u32 … x[7] u32 | W0 uv32 … W6 uv32 | word stream 0 … word stream 7
```

**Frequencies.** `top` is the largest symbol that can appear. Each `f[s]` is the frequency of symbol `s`, where 0
means absent; `f[top]` may be 0. Refuse when:
- `n` = 0, or the stream is shorter than 2 bytes;
- an `f[s]` is above 4096;
- the frequencies do not sum to exactly 4096.

`start[s]` is the sum of `f[t]` for `t < s`, and slot `j` (0-4095) belongs to the symbol `s` with
`start[s] ≤ j < start[s] + f[s]`.

**One symbol.** If exactly one frequency is non-zero (it is then 4096), the stream must end right after the frequency
list, and the output is that symbol `n` times. Refuse any further byte.

**States and word streams.** Otherwise:
1. Eight little-endian `u32` initial states follow. Refuse when fewer than 32 bytes are left, or when a state is
   below 2^16.
2. Seven uv32 byte lengths `W0`-`W6` follow. Word stream `k` (0-6) is the next `Wk` bytes, and word stream 7 is the
   rest of the payload. Refuse when a `Wk` is larger than what is left.

**Decoding.** Output symbol `j` (0 to n−1) uses state `k = j mod 8`:

```
slot = x[k] mod 4096
s    = the symbol of slot                      → output byte j
x[k] = f[s] · (x[k] >> 12) + slot − start[s]
if x[k] < 2^16:                                → read one word
    refuse if fewer than 2 bytes are left in word stream k
    x[k] = (x[k] << 16) | (first byte << 8) | second byte     (the word is big-endian)
```

At most one word is read per symbol, and all arithmetic fits in 32 bits.

**End.** After `n` symbols, refuse unless:
- every state equals 2^16;
- every word stream is used up exactly.

## 17. KLZ (codec 28): byte-aligned LZ77

A KLZ stream is a sequence of *sequences*. Each one holds some literals and then, except in the last sequence, one
match:

```
sequence := token u8 | literal-length extension | literals | offset (2 or 3 bytes) | match-length extension
token    := bits 0-2: literal length (7: extended) | bits 3-6: match length − 4 (15: extended) | bit 7: wide offset
```

1. **Literal length.** `ll = token & 7`. If it is 7, read extension bytes and add each one to `ll`, stopping after the
   first byte that is not 255.
2. **Literals.** Copy the next `ll` bytes of the stream to the output. Refuse when the stream has fewer than `ll`
   bytes left, or when the output would pass `n`.
3. **End.** If the output now has exactly `n` bytes, the stream ends here. Refuse unless:
   - bits 3-7 of the token are 0;
   - no byte of the stream is left.
4. **Offset.** Otherwise the offset is a little-endian integer of 3 bytes when bit 7 is set, else 2 bytes. Refuse
   when the bytes run out. A 3-byte offset below 65536 is valid.
5. **Match length.** `ml = (token >> 3) & 15`. If it is 15, add extension bytes as in step 1. Then `ml += 4`.
6. **Match.** Copy the match (§14.2), then continue with the next sequence. Refuse when there is no byte left for the
   next token.

When `n` = 0 the stream is the single token `0x00`. The output can only end in step 3, so a stream must end with a
sequence whose literals complete the output. When a match completes it, that sequence is a token `0x00`.

## 18. KLZH (codec 29) and KLZH2 (codec 32): LZ77 with entropy-coded streams

KLZH2 is written at levels 2 to 7 and in graph leaves. KLZH is read only, with three differences (§18.5).

### 18.1 Layout

```
klzh2 := ns uv64 | nl uv64 | stream(literals, nl) | stream(ll codes, ns) | stream(ml codes, ns) | stream(offset codes, ns) | extra bits
stream(·, count) := mode u8 | z uv64 | z bytes
```

- `ns` is the number of sequences and `nl` the number of literal bytes. Refuse when `ns` > `⌊n / 3⌋ + 1` or `nl` > `n`.
- Each of the four streams decodes to `count` bytes:
  - **mode 0:** khuf chunks (§15) of `count` bytes;
  - **mode 1:** krans (§16) of `count` symbols. Only the three code streams may use it.
- Refuse when:
  - the mode byte is missing or above 1, or is 1 for the literal stream;
  - `z` is larger than what is left;
  - the stream's coder refuses its `z` bytes;
  - `count` is 0 and `z` is not 0. A stream of `count` 0 is `mode, 0`: its coder is not called, and any mode byte 0 or
    1 is accepted.
- The **extra bits** are the rest of the payload. They are read as one bit sequence: byte 0 first, least significant
  bit first. A value of `k` extra bits is the next `k` bits, the first one being bit 0 of the value.

### 18.2 Codes

All code streams are checked before any sequence is decoded. Refuse when:
- any literal-length or match-length code is 72 or more;
- any offset code is above 32.

**Length codes** (literal lengths, and match length − 3). Code `c` gives a base and a count of extra bits:

| c | base | extra bits |
|---|---|---|
| 0-15 | c | 0 |
| 16-71 | `2^h + (c mod 2) · 2^(h−1)`, with `h = 4 + ⌊(c − 16) / 2⌋` | h − 1 |

The value is the base plus the extra bits. Code 16 covers 16-23, code 17 covers 24-31, code 18 covers 32-47, and so
on. Code 71 covers `3·2^30` to `2^32 − 1`.

**Offset codes.** The decoder keeps three recent offsets `r0, r1, r2`, which start as `1, 4, 8` for each coder call.

| code | offset | recent offsets afterwards |
|---|---|---|
| 0 | r0 | unchanged |
| 1 | r1 | r1, r0, r2 |
| 2 | r2 | r2, r0, r1 |
| 3 + h (h = 0 to 29) | `2^h` + h extra bits | new, r0, r1 |

### 18.3 Decoding

For each sequence `k` = 0 to ns−1, take code `k` of each code stream and do the following:

1. **Literal length.** `ll` = value of the literal-length code. Read its extra bits.
2. **Match length.** `ml` = value of the match-length code, plus 3. Read its extra bits. **In 0.0.2 this sum is taken
   modulo 2^32.** For code 71 with extra bits of at least `2^30 − 3`, `ml` therefore becomes 0, 1 or 2. A decoder must
   reproduce this. No encoder writes such a sequence, and a later version of this document may forbid it.
3. **Offset.** Get the offset from the offset code, reading its extra bits when the code is 3 or more.
4. **Literals.** Copy the next `ll` bytes of the literal stream to the output. Refuse when fewer than `ll` literals
   are left, or when the output would pass `n`.
5. **Match.** Copy the match `(offset, ml)` (§14.2). A match of length 0 copies nothing, but its offset is still
   checked.

After the last sequence:
- Refuse unless the literals left equal the output bytes still missing. Then copy them.
- Refuse when the extra bits used reach past the end of the payload. Let `b` be the extra bits consumed by all the
  sequences: refuse when `⌈b / 8⌉` is greater than the length of the extra-bit area. Bits past the end were read as
  zeros, but such a stream is refused anyway.
- Bytes after the last extra bit are ignored. The encoder writes none.

The encoder cuts the khuf chunks of each stream where the input crosses a multiple of 64, 128, 256 or 512 KiB. This
is an encoder choice; §15 decodes any chunk sizes.

### 18.4 Test coverage

The 0.0.2 corpus (`tests/v12/r0.0.2`) has the following material for these coders, all of which
`tests/coder_reference.py` decodes:
- 93 KLZH2 blocks and 29 KLZ blocks:
  - 73 KLZH2 and 22 KLZ blocks have no transform and are compared with the original;
  - 20 KLZH2 and 7 KLZ blocks have the byte-plane shuffle (§19.1), and 2 KLZH2 blocks the ARM64 filter. These are
    checked through the block checksum after the inverse;
  - the remaining KLZH2 blocks, under the table, log, record, word and line transforms, are checked to decode to their
    transformed length;
- 17 KLZH2 graph leaves and 47 khuf graph leaves, checked to decode to their raw length;
- 29 krans streams inside them.

The test also checks that one stream for each refusal rule above is refused. No frame uses KLZH (29).

### 18.5 KLZH (codec 29, read only)

KLZH is KLZH2 with these differences:
- there is no mode byte: each stream is `z uv64 | khuf chunks`;
- `ns` ≤ `⌊n / 4⌋ + 1`;
- the match length is the code's value plus **4**;
- offset codes go up to 26, so offsets are below 2^24. Refuse an offset code above 26.

## 19. Simple inverse transforms

### 19.1 Byte-plane shuffle (plan type 2, tflag 1)

The plan is `2, w, c, d`, where `w` is the element width (2, 4 or 8), `c` the number of channels (1-8) and `d` the
delta flag. Refuse other `w` or `c`. The coded stream decodes to `raw` bytes. Let `u = w · c` and `rows = ⌊raw / u⌋`.
The first `rows · u` decoded bytes are `w · c` planes of `rows` bytes each. Plane `b · c + ch` holds byte `b` (from the
least significant) of channel `ch`'s values, in row order. For each channel, the value of row `r` is the
little-endian `w`-byte integer gathered from its planes:
- when `d` = 0, this is the value itself;
- otherwise it is the difference from the previous row's value of that channel, modulo `2^(8w)` (the value before row 0
  is 0), and the value is the running sum.

Value `(r, ch)` is written little-endian at offset `(r · c + ch) · w`. The last `raw − rows · u` decoded bytes are
copied unchanged.

### 19.2 ARM64 branch filter (plan type 11, tflag 14)

Specified in SPEC.md, *ARM64 branch filter*. `tests/coder_reference.py` implements it from that text.
