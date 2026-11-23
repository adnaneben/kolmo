# Kolmo frame format, container version 12

Status: **normative** for the container layer, version 12, as written and read by Kolmo 0.0.1 and 0.0.2.
Compatibility promise: [COMPATIBILITY.md](COMPATIBILITY.md). Detailed history of versions 1 to 11 and design notes:
[SPEC.md](SPEC.md).

This document has two parts.

- **Container layer** (sections 1 to 12): complete. A decoder can be written from this text alone up to the point
  where it hands a block payload to a coder. It covers the header, the block index, the checksums, the plan prefix,
  the transform flags, stored blocks, the nesting of video frames, container extensions and the stream wrapper.
  `tests/format_reference.py` is an independent Python implementation of this layer, written from this text. It
  checks every frame of the frozen corpus and the `kolmo_khash64` test vectors.
- **Coder and transform payloads** (section 8): a registry of every identifier with the status of its specification.
  Several payloads are specified in [SPEC.md](SPEC.md). For the others, the reference implementation at tag `v0.0.2`
  is normative until their sections are written (section 13 lists them).

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
   - The ARM64 inverse is specified in SPEC.md, *ARM64 branch filter*.
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
| 14 | RANS (order 0) | leaf | written | `src/codecs/rans.c` |
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
| 27 | KHUF (Kolmo Huffman) | leaf | written | `src/algo/khuf.h`, `src/algo/khuf.c` |
| 28 | KLZ (byte-aligned LZ77) | block, leaf | written (level 1) | `src/algo/klz.h`, `src/algo/klz.c` |
| 29 | KLZH (LZ77 + Huffman) | block, leaf | read only | `src/algo/klz.h`, `src/algo/klz.c` |
| 31 | KIMG (8-bit images) | block | written (levels 8, 9, ultra) | `src/codecs/kimg.c` |
| 32 | KLZH2 (LZ77 + Huffman or static rANS per stream) | block, leaf | written (levels 2-7) | `src/algo/klz.h`, `src/algo/krans.h`, `src/codecs/native.c` |

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

## 13. What is not yet written out

These payloads are specified here only by the reference implementation at tag `v0.0.2`. Its C sources decode them,
and the frozen corpus pins their behaviour on real data. Writing each one out as prose is the next step of the format
work, in this order (most frames first):

1. KLZH2 (32) and its static rANS tables (`krans`);
2. KLZ (28);
3. khuf (27);
4. the table, log, line-class and word transforms (tflags 4, 5, 9, 10, 12);
5. KLZMA (24);
6. the BWT stream's QLFC coder (18);
7. the transpose, shuffle, record, 16-bit image and x86 inverses (tflags 1, 2, 6, 7, 8);
8. kimg (31);
9. CMP (25).

Until a payload has its own section, a change to its decoder that changes the decoded bytes of any version-12 frame is
a format change (COMPATIBILITY.md), whether or not the corpus catches it.
