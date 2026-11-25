# Kolmo frame format, container version 12

Status: **normative** for the container layer and for the coders and transforms of sections 14-24, version 12, as written and read by
Kolmo 0.0.1 and 0.0.2.
Compatibility promise: [COMPATIBILITY.md](COMPATIBILITY.md). Detailed history of versions 1 to 11 and design notes:
[SPEC.md](SPEC.md).

This document has four parts.

- **Container layer** (sections 1 to 12): complete. A decoder can be written from this text alone up to the point
  where it hands a block payload to a coder. It covers the header, the block index, the checksums, the plan prefix,
  the transform flags, stored blocks, the nesting of video frames, container extensions and the stream wrapper.
  `tests/format_reference.py` is an independent Python implementation of this layer, written from this text. It
  checks every frame of the frozen corpus and the `kolmo_khash64` test vectors.
- **Coder and transform payloads** (section 8): a registry of every identifier with the status of its specification.
  Payloads without a section here are specified in [SPEC.md](SPEC.md), with the reference implementation at tag
  `v0.0.2` deciding what SPEC.md leaves open (section 13).
- **Kolmo's own coders and the simpler transforms** (sections 14 to 21): khuf (27), krans, KLZ (28), KLZH (29),
  KLZH2 (32) and KLZMA (24); the byte-plane shuffle and x86 filter inverses; the line-class, word, log-template and
  minified-JSON inverses. `tests/coder_reference.py` is an independent Python decoder written from these sections. It decodes
  every block and graph leaf of the frozen corpus that uses them.
- **Block sorting, kimg and context mixing** (sections 22 to 24): codecs 18, 31, 7, 17 and 25, each with its own
  independent Python decoder (`tests/bwt_reference.py`, `tests/kimg_reference.py`, `tests/cm_reference.py`).

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
| 5 | 1 | mode | 0 to 5: the mode the encoder ran in (rt, fast, eco, hc, max, ultra). Its effects on decoding: the working-set bound of graph blocks (§11), and context-mixing graph leaves are valid only under mode 5 (§7.4) |
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
| 3 | table | 0, 3 (table v1), 4 (table v2), 13 (column-split, §20.10) | 4: transformed length | raw + 16384 (tflag 3); raw + ⌊raw/8⌋ + 16384 (tflag 4) |
| 4 | log templates (a: tokenizer, 0 words, 1 JSON, 2 minified JSON) | 0, 5, 10 | 4 (tflag 5); 8 (tflag 10) | raw + 16384 |
| 5 | 16-bit image | 0, 6 | 4 | raw + 16384 |
| 6 | fixed-size records | 0, 7 | 4 | raw + 16384 |
| 7 | x86 call/jump filter (a, b, c: ignored, §19.3) | 0, 8 | — | in place |
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
   - The ARM64 inverse is specified in SPEC.md, *ARM64 branch filter*, the byte-plane shuffle in §19.1, the x86
     filter in §19.3, and the inverses of tflags 1, 2, 4, 5, 6, 7, 9, 10 and 12 in §20.
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

Context-mixing leaves (codecs 7, 17 and 25, also as a sub-stream codec of a PLANES or ZRUN leaf) are written only by
ultra level 2, whose header mode is 5. In a frame whose header mode is not 5 a decoder must refuse them. Decoders
before the Unreleased fix accepted them under any mode, so a frame's mode said nothing about its decode cost; no
encoder wrote such a frame. With this rule a frame of modes 0 to 4 never runs a context-mixing coder.

## 8. Codec registry

### 8.1 Block and leaf codecs

These are the codec ids of version 12. *Written* means the 0.0.2 encoder writes the id with default options or a
documented CLI option. *Read only* means it decodes the id but does not write it. An id not in this table must be
refused (corrupt).

| Id | Name | Where | 0.0.2 | Payload specification |
|---|---|---|---|---|
| 0 | STORE | block, leaf | written | §7.1 |
| 5 | BSC (libbsc 3.4.0 block format) | block, leaf | read only | SPEC.md *Entropy coders*; `src/codecs/bwt.c` |
| 7 | CM (context mixing, models 1-4) | block, leaf | written (ultra) | **§24** |
| 12 | GRAPH | block | written | SPEC.md *Kolmo graph blocks* |
| 14 | RANS (order 0) | leaf | written | SPEC.md *Leaf codec 14*; `src/codecs/rans.c` |
| 15 | NPLANE | leaf | written (`--native-leaf`) | SPEC.md *Leaf codec 15* |
| 16 | FQFAM (FASTQ family) | block | written (ultra) | SPEC.md *FASTQ family blocks*; `src/codecs/fastq.c` |
| 17 | CMI (context mixing, image model) | block | written (ultra) | **§24** |
| 18 | BWT (Kolmo block sorting) | block, leaf | written | **§22** |
| 19 | AUD (kaud, 16-bit PCM) | block | written | SPEC.md *Audio blocks*; `src/codecs/audio.c` |
| 20 | VID (kvid layout + nested frame) | block | written | §7.3; SPEC.md *Video frames*; `src/codecs/video.c` |
| 21 | PLANES | leaf | written | SPEC.md *Format 2*; `src/codecs/planes_leaf.c` |
| 23 | ZRUN | leaf | written | SPEC.md *Format 2*; `src/codecs/zrun_leaf.c` |
| 24 | KLZMA | block, leaf | written | **§21** |
| 25 | CMP (context mixing over parts) | block | written (ultra) | **§24** |
| 26 | GR6 (GR6+ line-structured text) | block | written (ultra) | SPEC.md *GR6+ blocks*; `src/codecs/gr6.c` |
| 27 | KHUF (Kolmo Huffman) | leaf | written | **§15** |
| 28 | KLZ (byte-aligned LZ77) | block, leaf | written (level 1) | **§17** |
| 29 | KLZH (LZ77 + Huffman) | block, leaf | read only | **§18.5** |
| 31 | KIMG (8-bit images) | block | written (levels 8, 9, ultra) | **§23** |
| 32 | KLZH2 (LZ77 + Huffman or static rANS per stream) | block, leaf | written (levels 2-7) | **§18**, with krans §16 and khuf §15 |

**Retired ids, never valid in version 12:**
- 1-4, 6, 13 and 22 were library codecs of versions 1 to 11;
- 8-11 are experimental codecs of builds made with `-DKOLMO_FRONTIER`, outside this format;
- 30 is held for a later khuf.

Ids 14, 15, 21, 23 and 27 are valid only as graph leaves, and 20 only as in §7.3. (Decoders before the Unreleased fix
also accepted 14 and 27 as block codecs; no encoder writes them there.)

### 8.2 Other identifiers

The following identifiers are specified in SPEC.md and keep the meanings given there:
- graph node ids 1-7;
- the CM model nibbles and their registry;
- the BWT stream flags;
- the plan parameters of each transform.

A decoder must refuse a value that section does not assign.

### 8.3 Column-split table blocks

Plan type 3 with tflag 13 is written only on request (`KOLMO_T_COLUMNS`, used by indexed archives). Its payload is
specified in §20.10.

## 9. Container extensions

Bytes after the last payload must be either nothing or extension sections followed by a trailer:

```
section := tag[4] | length u64 | body[length] | crc32c(tag ‖ length ‖ body) u32
trailer := "KEXT" | 1 u8 | 0 u8 0 u8 0 u8 | count u32 | sections_length u64 | crc32c(first 20 trailer bytes) u32
```

- The trailer is the last 24 bytes of the frame (of the file, for a file of one frame; section 10.1).
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
- The version byte must be 1; a decoder refuses any other value.
- The three reserved bytes are written as 0 and **ignored by decoders**, as the reserved header and index bytes of
  frames are (COMPATIBILITY.md, "Rules for changing the format", rule 2). They can never carry something a decoder needs.
- `segment_bytes` is informational: the segment size the encoder used. Decoders do not read it (each frame carries its
  own sizes), so any value, 0 included, is valid.

### 10.1 Files of several members

A file (or a pipe) is one or more **members** one after another, decoded in order. The original is the members'
originals concatenated. This is the rule of gzip, xz and zstd (RFC 8878 section 3.1), so `cat a.kolmo b.kolmo` and
appended outputs (`kolmo -c x >> a.kolmo`, `kolmo --stream -c x >> a.kolmo`) decode to the inputs concatenated.

- A member is a `CZNS` stream (this section), or a `CZN1` frame with its container extensions, if any (section 9).
- A stream member ends at its zero length. A frame member ends after its last payload, unless the next bytes start an
  extension section: it then ends after the `KEXT` trailer that closes its sections. Its trailer is therefore the last
  24 bytes of that member, not necessarily of the file.
- After a member there is either nothing, or the next member, which starts with `CZN1` or `CZNS`.
- **Any other bytes after a member are an error** (corrupt): stray bytes, a zero length with no stream before it, a
  member cut short. A decoder never stops at the end of a member and ignores what follows.
- The library's one-frame calls (`kolmo_decompress`, `kolmo_check_frame`, `kolmo_read_range`) take exactly one frame
  and refuse anything after it. `kolmo_stream_decompress` and the CLI (`-d`, `-t`, `-l`) read every member.
- Before this rule was written down (0.0.2 and earlier), decoders stopped at a stream's terminator and returned
  success without reading the rest; a frame followed by a stream was refused. Files of several members written by any
  release decode in full with a decoder that follows this section.

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
- read a file of several members to the end, and refuse bytes after a member that start no member (section 10.1);
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
| `tests/v12/` | 84 frames written in the six modes of 0.0.1 (top level) and 274 frames written by 0.0.2 (`r0.0.2/`), each with its original. `r0.0.2/frames.json` gives each frame's options and SHA-256, the original's SHA-256 and a summary of its blocks |
| `tests/khash64_vectors.txt` | the khash64 vectors |
| `tests/format_reference.py` | the container layer in Python, checked against both of the above |
| `tests/coder_reference.py` | the coders and transforms of sections 14 to 21 in Python, decoding every corpus block and leaf that uses them |
| `tests/bwt_reference.py`, `tests/kimg_reference.py`, `tests/cm_reference.py` | the coders of sections 22, 23 and 24 in Python; `tests/vectors/` holds encoder-written streams for variants the corpus lacks |

## 13. What is not yet written out

Every payload that section 13 used to list now has its own section:
- sections 14 to 18: KLZH2 (32) with krans, KLZ (28), KLZH (29) and khuf (27);
- section 19: the byte-plane shuffle (plan 2, tflag 1) and x86 filter (plan 7, tflag 8) inverses;
- section 20: every transform inverse except the ARM64 filter (SPEC.md) and the shuffle and x86 filter (section 19):
  tflags 1-7, 9, 10, 12 and 13;
- section 21: KLZMA (24);
- section 22: the BWT stream (18): LZP, inverse BWT and QLFC with libbsc's tables as normative data;
- section 23: kimg (31);
- section 24: context mixing: CM (7), CMI (17) and CMP (25), with the mixer priors as normative data.

The other ids of the registry (section 8.1) are specified in SPEC.md, as their rows say. Where SPEC.md leaves a detail
open, the reference implementation at tag `v0.0.2` decides it, and a change to such a decoder that changes the decoded
bytes of any version-12 frame is a format change (COMPATIBILITY.md), whether or not the corpus catches it.

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

The output can only end in step 3, so the last sequence of a stream is one whose literals complete the output. When a
match completes the output, a last sequence with no literals follows: the single token `0x00`. When `n` = 0 the
stream is that token alone.

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

The corpus (`tests/v12`, both releases) has the following material for these coders, all of which
`tests/coder_reference.py` decodes:
- 132 KLZH2 blocks and 29 KLZ blocks:
  - 74 KLZH2 and 22 KLZ blocks have no transform and are compared with the original;
  - 35 KLZH2 blocks are under a transform of section 20 and are compared with the original after its inverse;
  - 23 KLZH2 and 7 KLZ blocks have an inverse in section 19 or SPEC.md and are checked through the block checksum
    after it: 20 + 7 under the byte-plane shuffle, 2 under the ARM64 filter, 1 under the x86 filter;
- 17 KLZH2 graph leaves and 47 khuf graph leaves, checked to decode to their raw length;
- more than 40 krans streams inside them.

The test also checks that one stream for each of several refusal rules above is refused. The decoders were also
compared with the 0.0.2 C decoders on about 26,000 mutated corpus streams. Both refused the same streams and decoded
the others to the same bytes. No frame uses KLZH (29).

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

### 19.3 x86 call/jump filter (plan type 7, tflag 8)

Encoders write the plan as `7, 0, 0, 0`. A decoder ignores `a`, `b` and `c`: the 0.0.1 and 0.0.2 planners could
leave there the parameters of a byte-plane shuffle plan that lost its trial (`src/plan/planner.c`, fixed in the
Unreleased encoder), so refusing them could refuse frames those releases wrote. A later x86 filter therefore needs
a new plan type, not new values of plan 7's parameters. There is no meta, and the coded stream decodes to `raw` bytes, which the inverse changes in
place. For each position `i` from 0 to `raw − 5`, in increasing order, look at the block's bytes as they are at that
moment, including changes made at earlier positions. If both of these hold:
- `b[i]` is `0xE8` or `0xE9`;
- `b[i+4]` is `0x00` or `0xFF`;

then replace the 24-bit little-endian value in `b[i+1..i+3]` by that value minus `i`, modulo 2^24. Then continue at
`i + 1`, not after the four bytes. A block shorter than 5 bytes is unchanged. `i` is the offset in the block, not in the
file.

The encoder applies the same rule with `+ i`, from the last position to the first.

## 20. Text transforms

These inverses take the coder's output `t` (the *transformed bytes*, of the transformed length in the block's meta,
§6.1) and produce the block's `raw` bytes, called `n` here. Each one either produces exactly `n` bytes or refuses
the block as corrupt. `u32` fields are little-endian.

### 20.1 Line classes (plan type 10, tflag 12)

Fixed-width lines stored column by column.

```
t := 1 u8 | K u8 | L u32 | tail u32 | other u32 | K × (len u32, count u32) | class u8 × L | other lines | tail | columns
```

- `L` is the number of lines and `K` (1-8) the number of classes. Class `k` (1-based) holds `count` lines of `len`
  bytes, not counting their newline.
- `class` gives each line in order its class: 0 for an *other* line, else 1 to K.
- *Other lines* is `other` bytes: the other lines in order, each with its newline.
- *Tail* is the `tail` bytes after the last newline of the block.
- *Columns* holds the classes in order 1 to K. Class `k` is `len` columns of `count` bytes: column `j` holds byte `j`
  of each of the class's lines, in line order.

Let `covered = Σ len·count` and `lines = Σ count`, computed exactly. Refuse when:
- `t` is shorter than 14 bytes, or its first byte is not 1;
- K is 0 or above 8, or `t` is shorter than `14 + 8K`;
- a `len` is 0 or above 4096;
- `L > n` or `lines > L`;
- `14 + 8K + L + other + tail + covered ≠ |t|`, or `covered + lines + other + tail ≠ n`.

Decode the lines in order:
- A class-`k` line is `len` bytes from the class's columns, then a newline.
- An *other* line is the next bytes of the other-lines area, up to and including the next newline.

Refuse when:
- a class id is above K;
- a class has more lines than its `count`, or fewer at the end;
- no newline is left in the other-lines area;
- the output would pass `n`;
- the other-lines area is not used up exactly;
- the bytes left to produce are not exactly `tail`.

Then append the tail. Class column bytes are copied as they are and are not checked, so they may hold newlines.

### 20.2 Word dictionary (plan type 8, tflag 9)

Frequent words are replaced by two-byte codes whose first byte is a *prefix* byte.

```
t := np u8 | prefix u8 × np | cap u8 | nw u32 | word × nw | text
word := 3 to 48 bytes | '\n'
```

Refuse when:
- `t` is shorter than 6 bytes;
- `np` is 0 or above 64, or `t` is shorter than `np + 6`;
- two prefixes are equal, or `cap` equals a prefix;
- `nw > 256·np` or `nw > 16384`;
- a word is missing its newline, or is shorter than 3 or longer than 48 bytes. A word may hold any byte except
  newline.

The **text** is the rest of `t`. Read it from the start:
- A byte that is neither a prefix nor `cap` is copied to the output.
- A prefix byte `P` and the next byte `x` are the code `256·index(P) + x`, where `index(P)` is P's position in the
  prefix list (0-based). They are replaced by word number `code`.
- `cap` must be followed by a prefix byte. The code after it is replaced by its word with bit 5 of the word's first
  byte cleared (`& 0xDF`).

Refuse when:
- `cap` is not followed by a prefix byte;
- a prefix byte is the last byte of the text;
- `code ≥ nw`;
- the output would pass `n`, or ends shorter than `n`.

### 20.3 Log templates (plan type 4, tflag 5)

Lines that share a *template* are stored once as the template plus one column per variable token.

```
t := 1 u8 | L u32 | T u32 | tail u32 | dict u32 | rare u32 | ids | dict bytes | rare bytes | tail bytes | columns
```

**Integer arrays.** An integer array of `m` values is `mode u8 | w u8 | w planes of m bytes`.
- Plane `b` holds byte `b` (from the least significant) of each value's unsigned `w`-byte code `z`.
- The value is `zigzag⁻¹(z) = (z >> 1) XOR −(z & 1)`.
- With `mode` 1, values are deltas: each value is the running sum of the decoded deltas, modulo 2^64, starting from 0.
- Refuse `mode > 1`, `w` = 0 or above 8, or fewer than `m·w` bytes left.
- Values are then read as unsigned 64-bit numbers.

**Header and ids.** Refuse when:
- `t` is shorter than 21 bytes, or its first byte is not 1;
- `L > n`, `T > L` or `tail > n`.

`ids` is an integer array of `L` values, the template id of each line. Refuse an id above T, where 0 means a
*rare* line. Then `dict`, `rare` and `tail` bytes follow; refuse when they do not fit. `m_t` is the number of lines with
id `t`.

**Templates.** The dictionary holds templates 1 to T, each ending in a newline.
- Byte `0x01` in a template is a placeholder.
- A template with `p` placeholders is `p + 1` literal pieces separated by them.
- Placeholders are numbered as *columns* in order: template 1's first, and so on.
- Refuse when a template has no newline, or bytes are left after template T.

**Columns.** The total number of columns times 5 must not exceed the bytes left; refuse otherwise. Then each column of
template `t` follows, in column order, and has `m_t` values:
- **Type 0, text:** `size u32 | size bytes`. The values in order, each ending in a newline.
- **Type 1, skeleton:** `g u8 (1-12) | g + 1 pieces (len u8 | len bytes) | g runs`, where a run is
  `pad u8 (0-18) | integer array of m_t values`. Every value of a run must be below `10^pad` when `1 ≤ pad ≤ 17`,
  and below `10^18` when `pad` is 0 or 18.
- Any other type, a value outside those ranges, or bytes left after the last column are refused.

**Output.** For each line, in order:
- **Id 0:** copy the next line of the rare area, up to and including its newline. Refuse when the rare area has no
  newline left.
- **Id `t`:** write the template's pieces. In place of each placeholder, write the next value of its column:
  - for a **text** column, the bytes up to the next newline of its area, without the newline. Refuse when there is
    none.
  - for a **skeleton** column, piece 0, run 0's value, piece 1, … , run `g−1`'s value, piece `g`. A run value is
    written in decimal. When `pad` > 0 it is left-padded with `0` to `pad` digits.

  Then write a newline.

Refuse when:
- the output would pass `n`;
- the rare area is not used up;
- a text column's area is not used up;
- the bytes left to produce are not exactly `tail`.

Then append the tail.

### 20.4 Minified JSON (plan type 4, tflag 10)

The meta is 8 bytes: the transformed length, then `k u32`, the number of newlines the encoder inserted. Refuse
`k > n`. The transformed bytes decode by §20.3 to `n + k` bytes, not `n`. Remove every newline from them, and refuse
unless exactly `n` bytes remain. The encoder uses this only on blocks without newlines.

### 20.5 Table (plan type 3, tflag 4)

A delimited table stored column by column, with typed numeric columns. The plan is `3, delim, …`. Only `a`, the
delimiter byte, is used by the decoder. Integers are little-endian. `zigzag⁻¹` is as in §20.3, and "wrapping"
arithmetic is modulo 2^64 on signed 64-bit values.

```
t := k u32 | nr u32 | nexc u32 | noNL u32 | gcol i32 | nform u32 | formula × nform | exception × nexc | column × k
formula   := y u16 | a u16 | b u16 | form u8 | 1 byte (ignored) | factor f64      (16 bytes)
exception := row u32 | fields u32                                                 (8 bytes)
column    := type u8 | size u32 | body (size bytes)
```

**Header.**
- `k` is the number of columns and `nr` the number of rows.
- `noNL` non-zero means the last row has no newline.
- `gcol` is the group column, or −1 for none.
- Refuse when:
  - `t` is shorter than 24 bytes;
  - `k` is 0 or above 256, `nform > 256`, or `gcol` is below −1 or not below `k`;
  - the formulas and exceptions do not fit in `t`;
  - `nr > n + 1`.
- A formula's `form` is 1 (ratio), 2 (difference) or 3 (percent change); refuse any other value. Refuse `y ≥ k` or
  `a ≥ k`, and for a ratio `b ≥ k`. `factor` is an IEEE 754 double.

**Exceptions** mark short rows. Row `row` has only `fields` fields, the first ones. Refuse when:
- `fields` is 0 or above `k`;
- `row ≥ nr`;
- rows are not strictly increasing.

Column `j` therefore has `nr` minus the number of exceptions with `fields ≤ j` entries. Refuse a column whose count
differs.

**Columns.** Refuse a `type` other than 0 or 2, or a `size` larger than what is left. Bytes left at the end of a body
are ignored.
- **Text (type 0):** `cnt u32 | nlong u32 | lens u8 × cnt | long u32 × nlong | data`.
  - A length byte of 255 takes the next `long` value instead.
  - The values are consecutive slices of `data`.
  - Refuse when the body is shorter than 8 bytes, or `cnt` or `nlong` exceeds the body size.
  - Refuse when the arrays do not fit, the number of 255 bytes is not `nlong`, or the lengths add up to more than
    `data`.
- **Numeric (type 2):** `cnt u32 | nnum u32 | nx u32 | S u8 | w u8 | mode u8 | fid u8 | kind u8 × cnt | sd u8 × nnum | planes | xlen u32 × nx | xdata`.
  - `kind` gives each entry: 0 empty, 1 a number, 2 a text exception.
  - `S` is the column's scale: values are integers in units of `10^−S`.
  - `sd[i]` is how many trailing digits number `i` drops when it is printed.
  - `planes` is `w` byte planes of `nnum` bytes each. Plane `b` holds byte `b` of each number's code `z`, and the
    residual is `zigzag⁻¹(z)`.
  - Text exceptions are consecutive slices of `xdata`, with lengths from `xlen`.
  - Refuse when:
    - the body is shorter than 16 bytes;
    - `S > 18`, `w` is 0 or above 8, `mode > 5`, `cnt` exceeds the body size, or `nnum` or `nx` exceeds `cnt`;
    - the arrays do not fit;
    - a kind is above 2, or the counts of kinds 1 and 2 are not `nnum` and `nx`;
    - an `sd` is above `S`;
    - the `xlen` add up to more than `xdata`.

The table is **aligned** when `nexc` is 0 and every column has `nr` entries.

**Group ids.** With `gcol ≥ 0`, refuse unless column `gcol` is text and the table is aligned. Row 0 has group 0, and
each later row has the previous row's group, plus 1 when its `gcol` value differs from the previous row's. Without a
group column every row is in group 0.

**Numbers.** Modes 2-5 need an aligned table; refuse them otherwise. Decode the columns of modes 0, 1, 2, 4 and 5
first, then those of mode 3. For the `i`-th number of a column (in row order), let `r` be its residual. "Same group"
means `i > 0` and its row is in the same group as the previous number's row.
- **Mode 0:** `v = r`.
- **Mode 1:** `v` = the running sum of the residuals, wrapping.
- **Mode 2:** `v = previous v + r` in the same group, else `r`.
- **Mode 4:** in the same group, `d = r + previous d` (just `r` for the group's second number), and `v = previous v + d`.
  Otherwise `v = r`.
- **Mode 5:** in the same group, for the group's second number `d = r`. After that, `dd = r + previous dd` (just `r` for
  the group's third number) and `d = dd + previous d`. Then `v = previous v + d`. Otherwise `v = r`.
- **Mode 3 (formula):**
  1. **Checks.** Refuse when `fid ≥ nform`, or formula `fid`'s `y` is not this column. Refuse when its source column
     `a` (and `b` for a ratio) is not numeric or is itself of mode 3.
  2. **Source values.** A source column gives each row the double `exact(v, sd) / 10^(S − sd)`, or NaN for a row
     without a number.
     - `exact(v, s)` is `(v >> s) · inv(5^s) mod 2^64`, read as signed. `>>` is an arithmetic shift, and `inv(5^s)` is
       the inverse of `5^s` modulo 2^64. It equals `v / 10^s` when `10^s` divides `v`.
     - `10^x` is the exactly representable double.
  3. **Prediction**, with doubles in IEEE 754 round-to-nearest, evaluated left to right with no fused multiply-add.
     Division by zero gives ±infinity or NaN, as IEEE 754 defines. For row `t`:
     - ratio: `A[t] / B[t] · factor`;
     - difference: `(A[t] − L) · factor`;
     - percent: `(A[t] − L) / L · 100 · factor`.

     `L` is `A[t−1]` when row `t − 1` exists and is in the same group, else NaN.
  4. **Value.** Let `p = prediction · 10^S`. If `|p| < 2^62`, which excludes NaN and infinities,
     `v = r + roundeven(p)`. Otherwise `v = r`.

**Printing a number.**
1. Write `v` as a fixed-point decimal with exactly `S` digits after the point and at least one before it, with `-`
   for negative values. When `S` is 0 there is no point.
2. Drop its last `sd` characters, plus the point when `sd = S > 0`. The dropped digits are not checked.

**Rows.** For row 0 to `nr − 1`:
- write its fields, `k` of them or `fields` for an exception row;
- separate the fields by the delimiter;
- end the row with a newline, except the last row when `noNL` is non-zero.

A field is:
- for a text column, its next value;
- for a numeric column, nothing (kind 0), the next printed number (kind 1) or the next text exception (kind 2).

Refuse when the output would pass `n`, or ends shorter.

**Fields over 65535 bytes.** A conforming decoder must decode a table whose text values or text exceptions are longer
than 65535 bytes. The 0.0.2 encoder writes such blocks: any CSV whose rows all have the same field count, with one
field longer than 64 KiB, at levels 3-9. The 0.0.2 decoder refused them in aligned tables, so 0.0.2 could not read
those files. Its fast path kept field lengths in 16 bits and never fell back to the row path. Releases after 0.0.2
decode them by the rules above, to the original. `tests/v12/r0.0.2/longfield.csv.*` are two such frames written by
0.0.2.

Table v1 (tflag 3) is in §20.9 and the column-split table (tflag 13) in §20.10.

### 20.6 Typed transpose (plan type 1, tflags 1 and 2)

Delimited text stored field column by field column. The plan is `1, delim, quotes, c`, with `k = c + 1` columns.
- The meta is `k` sizes, `u32` each.
- The coded stream decodes to `raw` bytes: column 0's bytes, then column 1's, and so on, each of its size.
- Refuse when the sizes do not add up to exactly `raw`.

A byte is a **terminator** when it is a newline or `delim`. With tflag 1 and `quotes` non-zero, `"`, `[` and `]` are
terminators too. With tflag 2 the quote rule is off whatever `quotes` holds.

Rebuild the text with a current column `j` (starting at 0) and the flags `inq` and `inbr` (starting clear). Until the
output has `raw` bytes:
1. Copy bytes from column `j`'s next unread position up to and including its next terminator, or to the end of the
   column when it has none left. Refuse when this copies nothing or would pass `raw`.
2. If the last byte copied is a terminator:
   - with quotes on, `"` toggles `inq` when `inbr` is clear; `[` sets `inbr` when `inq` is clear; `]` clears `inbr`
     when `inq` is clear;
   - a newline sets `j = 0` and clears both flags;
   - `delim` with both flags clear moves to the next column, `j = j + 1`, unless `j` is already `k − 1`.

### 20.7 16-bit image (plan type 5, tflag 6)

```
t := be u8 | skip u8 | mode u8 | 1 byte (ignored) | W u32 | N u32 | skip bytes | lo × N | hi × N | tail
```

**Header.** Refuse when:
- `t` is shorter than 12 bytes;
- `be > 1`, `skip > 1` or `mode > 2`;
- `W` is 0 or above 4096;
- `skip + 2N > raw`;
- `|t|` is not exactly `12 + skip + 2N + tail`, where `tail = raw − skip − 2N`.

**Samples.** Sample `i` (0 to N−1) is predicted from the samples already decoded. Let `L = x[i−1]`, `U = x[i−W]` and
`UL = x[i−W−1]`, each 0 when its index is negative. The prediction is:
- mode 0: `L`;
- mode 1: `⌊(L + U) / 2⌋`;
- mode 2: `min(L, U)` if `UL ≥ max(L, U)`; `max(L, U)` if `UL ≤ min(L, U)`; otherwise `L + U − UL`.

The residual is `zigzag⁻¹(lo[i] + 256·hi[i])` (§20.3) and `x[i] = (prediction + residual) mod 2^16`.

**Output.** The `skip` bytes, then the N samples, each written as 2 bytes (big-endian when `be` is 1, else
little-endian), then the tail.

### 20.8 Fixed-size records (plan type 6, tflag 7)

```
t := P u16 | nf u8 | 1 byte (ignored) | nrec u32 | pre u32 | tail u32 | field × nf | pre bytes | tail bytes | bodies
field := o u8 | w u8 | kind u8 | 1 byte (ignored) | size u32
```

The block is `pre` bytes, `nrec` records of `P` bytes, then `tail` bytes. Each field covers bytes `o .. o + w − 1` of
every record, and its body (`size` bytes, in field order) holds those bytes for all records. Field values are
little-endian unsigned `w`-byte integers.

**Header.** Refuse when:
- `t` is shorter than 16 bytes;
- `P` is below 2 or above 64, or `nf` is 0 or above 64;
- `pre > raw`, `tail > raw`, or `nrec·P + pre + tail ≠ raw`;
- `16 + 8·nf + pre + tail > |t|`.

**Fields.** Refuse when:
- `w` is not 1, 2, 4 or 8;
- `o + w > P`;
- `size` is larger than what is left;
- a record byte is covered by two fields, or any byte `0 .. P−1` is covered by none;
- bytes are left after the last body.

**Bodies.** *Planes* of `m` values of `w` bytes are `w` arrays of `m` bytes, array `b` holding byte `b` of each value.
- **kind 0, raw:** planes of the `nrec` values. Refuse unless `size = nrec·w`.
- **kind 1, delta:** the same, then each value is the running sum modulo `2^(8w)`.
- **kind 2, dictionary:** `count u32 | iw u8 | planes of count values (w bytes) | planes of nrec indices (iw bytes)`.
  - The dictionary values are running sums modulo `2^(8w)` of the stored ones.
  - Each record's value is the dictionary entry at its index.
  - Refuse when `iw` is not 1, 2 or 4, `count` is 0, `size ≠ 5 + count·w + nrec·iw`, or an index is `≥ count`.
- **kind 3, quantised doubles** (only with `w = 8`): `c f64 | delta u8 | pfor (nrec values) | pfor (nrec values)`.
  - A **pfor** is `pw u8 (1-8) | ne u32 | planes of nrec values (pw bytes) | ne × (index u32, value u64)`. Each pair
    replaces value `index`. Refuse a `pw` outside 1-8, an `index ≥ nrec`, or a pfor that does not fit.
  - The first pfor holds zigzag codes of integers `q`. With `delta` 1, `q` is their running sum, wrapping modulo 2^64.
  - The second pfor holds zigzag codes of corrections `e`, and must end exactly at the end of the body.
  - The value is the bit pattern of the double `q · c`, computed in IEEE 754 round-to-nearest from `q` converted to a
    double, plus `e`, modulo 2^64.
  - Refuse when `size < 19`, `delta > 1`, or `c` is not a finite number above 0.
- Any other kind, or kind 3 with `w ≠ 8`, is refused.

Copy the `pre` bytes to the start and the `tail` bytes to the end of the block. Each field writes its values into its
bytes of each record.

### 20.9 Table v1 (plan type 3, tflag 3)

The table layout of earlier releases. No 0.0.1 or 0.0.2 encoder writes it, but decoders accept it. Its checks are
looser than table v2's, and a decoder must reproduce them exactly. In particular, exception entries that do not match
are skipped, unused numbers are allowed, and bytes after the last column are ignored.

```
t := k u32 | nr u32 | nexc u32 | noNL u32 | exception × nexc | column × k
exception := row u32 | fields u32
column    := type u8 | size u32 | body
```

**Header.** Refuse when:
- `t` is shorter than 16 bytes;
- `k` is 0 or above 256;
- `16 + 8·nexc > |t|`;
- `nr > n + 1`, or `nexc > nr`.

**Columns.** Refuse a `type` above 1, or a body that does not fit. Bytes after the last column are ignored.
- **Type 0, text:** `cnt u32 | nlong u32 | lens u8 × cnt | long u32 × nlong | data`, as in §20.5.
  - Refuse when the body is shorter than 8 bytes, or the arrays do not fit.
  - Refuse when a length byte 255 finds no `long` left, or the lengths add up to more than `data`.
  - Unused `long` values are allowed.
- **Type 1, numeric:** `cnt u32 | nnum u32 | nx u32 | S u8 | w u8 | kind u8 × cnt | sd u8 × nnum | planes | xlen u32 × nx | xdata`.
  - Refuse when the body is shorter than 14 bytes, `S > 18`, `w` is 0 or above 8, or the arrays do not fit.
  - The `nnum` numbers are decoded from the planes as residuals `zigzag⁻¹(z)`. Number `i` is the running sum
    (wrapping) of residuals 0 to `i`. Refuse when any `sd[i] > S`, even for a number that no entry uses.
  - Number `i` is printed as `exact(m, sd[i])` (§20.5) in fixed point with `S − sd[i]` decimals (§20.5, step 1,
    with no characters dropped).
  - The entries in order:
    - kind 0 is empty;
    - kind 1 takes the next number;
    - kind 2 takes the next text exception, with its length from `xlen`.

    Refuse a kind above 2, a kind 1 with no number left, a kind 2 with no `xlen` left, or exception text past the
    body. Unused numbers and exceptions are allowed.

**Rows.** Refuse when `nr` exceeds column 0's entry count.
- Walk the rows 0 to `nr − 1` with an exception cursor `e` starting at 0. Row `r` has `k` fields, unless `e < nexc`
  and exception `e`'s row is `r`: then the row has that exception's `fields` fields, and `e` advances. Refuse a
  `fields` of 0 or above `k`.
- Exceptions that are never matched this way are ignored.
- Each column `j` must have exactly as many entries as there are rows with more than `j` fields; refuse otherwise.

Write the rows as in §20.5. Fields are separated by the delimiter, every row ends with a newline except the last one
when `noNL` is non-zero, and the output must be exactly `n` bytes.

`tests/vectors/table_v1/` holds transforms made by the 0.0.2 sources' table-v1 encoder, with their originals.

### 20.10 Column-split table (plan type 3, tflag 13)

The table-v2 stream of §20.5, cut into a header segment and one segment per column, each coded on its own, so a
reader can decode one column without the others. `kolmo --index` writes it. The payload after the plan prefix is:

```
payload := tn u32 | nseg u16 | entry × nseg | coded segments, in order
entry   := codec u8 | raw u32 | coded u32 | check u32
```

**Directory.** Refuse when:
- the payload is shorter than 6 bytes;
- `tn > raw + 16384`, where `raw` is the block's length;
- `nseg` is below 2 or above 257;
- the directory does not fit;
- an entry's codec is neither 0 (stored) nor the block's codec;
- a stored segment's `coded ≠ raw`;
- a segment's coded bytes run past the payload;
- the running sum of `raw` exceeds `tn`;
- the coded lengths do not add up to exactly the bytes after the directory, or the raw lengths to exactly `tn`.

**Segments.** Decode each segment to its `raw` bytes. Refuse unless `check` equals the low 32 bits of
`khash64(segment, 0x74626c63)`. Concatenate the segments into the stream `t`.

**Split.** Refuse unless `t` splits exactly as follows:
- `tn ≥ 24`, `k = u32 t[0..4)` is 1 to 256, `nform = u32 t[20..24)` is at most 256, and `nexc = u32 t[8..12)` is at
  most `tn`;
- the header (24 bytes, the formulas and the exceptions) fits;
- each of the `k` columns' `type u8 | size u32` prefixes and bodies fits, and the last column ends exactly at `tn`;
- `nseg = k + 1`, segment 0 is the header, and segment `j + 1` is column `j`'s section with its 5-byte prefix.

Then decode `t` by §20.5.

## 21. KLZMA (codec 24)

KLZMA is an LZ77 coder with an adaptive binary range coder, in the family of LZMA. It is **not** bit-compatible with
LZMA: the probability layout of the distance and matched-literal trees, the range decoder's start and its end rule
differ. It decodes as below, with every value an unsigned 32-bit integer unless stated otherwise.

```
klzma := props u8 | range-coded bytes
```

**Properties.** `lc = props mod 9`, `lp = ⌊props / 9⌋ mod 5`, `pb = ⌊props / 45⌋`. Refuse when:
- the stream is shorter than 6 bytes;
- `pb > 4` or `lc + lp > 4`.

### 21.1 Range decoder

- **State.** `range = 0xFFFFFFFF`, and `code` is the five bytes after `props`, shifted in one at a time:
  `code = (code << 8 | byte) mod 2^32`. The first of the five bytes is therefore shifted out and has no effect.
- **Reading bytes.** Reading continues from byte 6 of the stream. A byte past the end reads as 0, but still counts as
  read.
- **Normalisation**, after every decoded bit: if `range < 2^24`, set `range = (range << 8) mod 2^32` and
  `code = (code << 8 | next byte) mod 2^32`. This is done once per bit, not in a loop.
- **Probabilities** are 11-bit, start at 1024 and are kept per context. Decoding a bit with probability cell `p`:
  - let `bound = (range >> 11) · p`;
  - if `code < bound`: the bit is 0, `range = bound` and `p += (2048 − p) >> 5`;
  - otherwise: the bit is 1, `code −= bound`, `range −= bound` and `p −= p >> 5`;
  - then normalise.
- **Direct bit** (no probability): `range >>= 1`. The bit is 1 when `code ≥ range`, and then `code −= range`. Then
  normalise.
- **Bit tree** of `k` bits over cells `c[1 .. 2^k − 1]`: start with `m = 1`, and `k` times decode bit `b` with cell
  `c[m]` and set `m = 2m + b`. The value is `m − 2^k`, most significant bit first.
- **Reverse tree**: the same walk, but bit `i` of the walk (from 0) is bit `i` of the value, least significant bit
  first.

### 21.2 Contexts

- `state` is 0 to 11 and starts at 0. After a literal it becomes 0 if it was below 4, `state − 3` if below 10, and
  `state − 6` otherwise. After a match it becomes 7 if it was below 7, else 10. After a long repeat match it becomes
  8 or 11, and after a short repeat 9 or 11, by the same rule.
- `pos` is the number of bytes produced, and `ps = pos mod 2^pb`.
- Four recent distances `rep0`-`rep3` start at 0. A stored distance `d` means "`d + 1` bytes back".
- Cells, each starting at 1024:
  - `is_match[state][ps]`, `is_rep[state]`, `is_rep0[state]`, `is_rep1[state]`, `is_rep2[state]`,
    `is_rep0_long[state][ps]`;
  - `slot[4]` (6-bit trees);
  - `spec[slot]` for slots 4-13, a separate reverse tree for each slot;
  - `align` (a 4-bit reverse tree);
  - two length coders, one for matches and one for repeat matches. Each has `choice`, `choice2`, `low[ps]` and
    `mid[ps]` (3-bit trees) and `high` (an 8-bit tree);
  - literal coders: `0x300` cells for each of the `2^(lc+lp)` literal contexts.

**Length.** If `choice` gives 0, the value is `low[ps]`'s 3-bit tree. Otherwise, if `choice2` gives 0, it is 8 plus
`mid[ps]`'s tree. Otherwise it is 16 plus `high`'s 8-bit tree. The length is that value plus 2, so 2 to 273.

### 21.3 Decoding

Repeat until `pos = n`. Before each symbol, refuse when more than 4 bytes past the end have been read.

1. **Literal** (`is_match[state][ps]` gives 0). The literal context is `((pos mod 2^lp) << lc) + (prev >> (8 − lc))`,
   where `prev` is the last byte produced (0 at `pos` 0), and its cells are `L[0 .. 0x2FF]`.
   - When `state < 7`, the byte is an 8-bit tree over `L[1 .. 255]`.
   - When `state ≥ 7`, let `mb` be the byte `rep0 + 1` back. For bits 7 down to 0, with `sym` the tree index so far:
     while every bit so far equals `mb`'s bit, decode with cell `L[0x100 + 0x100 · mbit + sym]`, where `mbit` is
     `mb`'s current bit. After the first bit that differs, decode the remaining bits with `L[sym]`.
   - Append the byte and update `state`.
2. **Match** (`is_match` 1, `is_rep[state]` 0).
   - Decode the length with the match length coder.
   - Decode `slot` with the 6-bit tree `slot[min(length − 2, 3)]`.
     - If `slot < 4`, `d = slot`.
     - Otherwise let `nb = ⌊slot / 2⌋ − 1` and `d = (2 + (slot mod 2)) << nb`. For slots 4-13, add the `nb`-bit
       reverse tree `spec[slot]`. For slots 14 and up, add `nb − 4` direct bits (most significant first) shifted left
       by 4, then the 4-bit reverse tree `align`. The sum is taken modulo 2^32.
   - The recent distances become `d, rep0, rep1, rep2`. Set `state` to 7 or 10.
3. **Repeat match** (`is_rep[state]` 1).
   - **rep0** (`is_rep0[state]` 0): if `is_rep0_long[state][ps]` gives 0, this is a **short repeat**. Refuse when
     `rep0 ≥ pos`. Otherwise append the byte `rep0 + 1` back, set `state` to 9 or 11 and go on to the next symbol.
     If it gives 1, the distance is `rep0` and the list is unchanged.
   - **rep1, rep2 or rep3**: `is_rep1[state]` 0 selects rep1. Otherwise `is_rep2[state]` 0 selects rep2, and 1
     selects rep3. The selected distance moves to the front, and the ones before it move down one place.
   - Decode the length with the repeat length coder and set `state` to 8 or 11.
4. **Copy.** For a match or long repeat, refuse when `rep0 ≥ pos` or `length > n − pos`. Then copy `length` bytes
   from `rep0 + 1` back (§14.2).

**End.** Refuse when more than 4 bytes past the end have been read. Unread bytes at the end of the stream are ignored.
The encoder ends with 5 flush bytes.

### 21.4 Test coverage of sections 20 and 21

`tests/coder_reference.py` decodes all of the following:
- **Section 20**, compared with the original: 46 corpus blocks coded by KLZH2 or KLZMA under its transforms. These
  are 6 column-split table, 2 transpose, 1 16-bit image, 1 record, 15 table, 11 log-template, 5 minified-JSON, 4 word and 1 line-class block.
  - The table blocks reach numeric modes 0-5, ratio and difference formulas, a group column, short rows, text
    exceptions, empty fields, long-text lengths, fields over 65535 bytes and dropped trailing digits.
  - No corpus block uses the percent-change formula.
  - Blocks under these transforms coded by CM or BWT are not decoded, because those coders are not written out yet.
- **Table v1** (§20.9): the five vectors of `tests/vectors/table_v1/`, compared with their originals.
- **Section 21**: 15 KLZMA blocks and 8 KLZMA graph leaves.
  - 6 blocks have no transform and 5 are under the table or log transform; all 11 are compared with the original;
  - 4 are checked through the shuffle or x86 inverse and their block checksum.

The decoders of sections 20 and 21 were also compared with the 0.0.2 C decoders on mutated corpus streams: about
6,000 transform streams and 8,700 KLZMA streams. Both refused the same streams and decoded the others to the same
bytes.

## 22. BWT stream (codec 18)

Kolmo's block-sorting coder: optional LZP, the Burrows-Wheeler transform and QLFC entropy coding. The LZP, BWT and
QLFC stages are those of libbsc 3.4.0, and the QLFC models use libbsc's tables and constants. Those are normative data
here, in `tests/format_tables/qlfc.json`:
- `bsc_stretch_table` and `bsc_squash_table`, 4097 entries each;
- `model_rank_state_table` (32768) and `model_run_state_table` (8192);
- the counter and mixer constants. A constant is named `{M|F}_{RANK|RUN}_{XY}_{TH0|AR0|TH1|AR1|LR0|LR1|LR2}`, where
  `M` is the adaptive model and `F` the static one.

`stretch(p)` is `bsc_stretch_table[p]` and `squash(s)` is `bsc_squash_table[2048 + s]`. Arithmetic is on 32-bit
signed integers with `>>` arithmetic. Where a value is stored as 16 bits it wraps to a signed 16-bit value, and the
mixer products wrap modulo 2^32. `tests/bwt_reference.py` is the independent decoder.

### 22.1 Stream

```
stream := flags u8 | [lzp_bits u8 | lzp_min u8] | index u32 | rlog u8 | k u8 | sample u32 × k | qlfc
```

- `flags` bits 0-1 select the QLFC model: 1 static, 2 adaptive. Bit 2 means LZP was applied. Refuse other bits, and a
  model other than 1 or 2.
- With LZP, refuse `lzp_bits` outside 10-28 and `lzp_min` below 16.
- Refuse when `n < 2`, the stream is shorter than 7 bytes, the header does not fit, `rlog` is outside 1-30, or fewer
  than `4k + 1` bytes follow the header.

**Decoding.**
1. Decode the QLFC part (§22.4) to `m` bytes, at most `n`. Refuse unless `m ≥ 2`, `m = n` when there is no LZP, and
   `⌊(m − 1) / 2^rlog⌋ = k`.
2. Refuse unless `1 ≤ index ≤ m` and every sample is in `1..m`.
3. Invert the BWT (§22.6).
4. With LZP, undo it (§22.7) to exactly `n` bytes.

### 22.2 Range decoder

- Input is read as 16-bit little-endian words. When one byte is left, the word is that byte, and past the end words
  are 0.
- **Start:** `range = 2^32 − 1`, and `code` is three words shifted in 16 bits at a time, modulo 2^32.
- **Decoding a bit** with `p`, the 12-bit probability of a 0:
  1. If `range < 2^16`, set `range <<= 16` and shift the next word into `code`.
  2. Let `r = (range >> 12) · p`.
  3. If `code ≥ r`, the bit is 1, `range −= r` and `code −= r`. Otherwise the bit is 0 and `range = r`.
- A **word** is 32 bits decoded with `p = 2048`, most significant first.

### 22.3 Counters and mixers

**Counters** are signed 16-bit cells starting at 2048. With constants `TH0, AR0, TH1, AR1`:
- after a 0: `c += ((4096 − TH0 − c) · AR0) >> 12`;
- after a 1: `c −= ((c − TH1) · AR1) >> 12`.

Two places of the static model (the mantissa and escape bits, §22.5) use the *rounded* form instead:
- after a 0: `c −= (c·AR0 − ((4096 − TH0)·AR0 − 4095)) >> 12`;
- after a 1: `c −= (c·AR1 − TH1·AR1) >> 12`.

**Static model.** The probability is `(c·LR0 + s·LR1 + p·LR2) >> 5` for the char, state and static counters `c, s, p`.

**Adaptive model.** The probability comes from a **mixer**, which has:
- weights `w0 = w1 = 65536` and `w2 = 0`;
- inputs `s0, s1, s2`;
- an index;
- 17 map cells, cell `i` starting at `squash((i − 8)·256)`.

To mix `p0, p1, p2`:
1. `sj = stretch(pj)`.
2. `x = (s0·w0 + s1·w1 + s2·w2) >> 17`, wrapped to 16 bits and clamped to −2047..2047.
3. `index = (x + 2048) >> 8` and `mapped = map[index] + ((map[index+1] − map[index]) · (x & 255) >> 8)`.
4. The probability is `mixed = (3·squash(x) + mapped) >> 2`.

After the bit is decoded:
- Update `map[index]` and `map[index+1]` as counters, with the mixer's `TH`/`AR` constants for that bit value.
- Then `wj −= (LRj · e · sj mod 2^32) >> 16` for j = 0, 1, 2, where `e = mixed − 4095` after a 0 and `mixed − 1` after
  a 1.

The counters of the same step are updated too, with the plain form.

### 22.4 QLFC framing

`qlfc := nblocks u8 | …`
- **One block:** a coded block follows (§22.5).
- **Several blocks:** `nblocks` pairs `(out i32, in i32)` follow, then the blocks' bytes in order.
  - Refuse 0 blocks, a table that does not fit, or a negative size.
  - Refuse a block whose input runs past the stream, or a running output total above the limit.
  - A block with `in = out` is stored. Any other block is coded and must decode to exactly `out` bytes.
  - Bytes after the last block are ignored.
- A coded block decodes on its own: every model starts fresh.

### 22.5 QLFC coded block

A coded block starts with a word, the output length `n'`. Refuse it when it is negative or above the limit.

**MTF header.** Build the MTF table `T[0..255]` rank by rank. Each rank's byte is decoded bit by bit, from bit 7 down
to bit 0, keeping the bits found so far as a prefix:
- A bit is coded (`p = 2048`) only when the candidates under the current prefix include both a 0 and a 1 at that bit.
  A candidate is a byte not yet in the table, or the previous rank's byte.
- Otherwise the bit is the one the candidates allow, and nothing is read.

When a rank's byte equals the previous rank's byte, the table ends:
- `lastRank` is that rank;
- `maxRank` is `⌊log2(rank − 1)⌋` (0 when `rank ≤ 1`);
- the rest of `T` is filled with that byte.

Without such a repeat, `lastRank = 255` and `maxRank = 7`.

**Symbols.** Repeat until `n'` bytes are out. The state is:
- `ch = T[0]`;
- per-byte histories `rankHist[ch]` and `runHist[ch]`, starting at 0;
- the contexts `cR0`, `cR4`, `cRun` and `avg`, starting at 0.

Steps:
1. **Rank state:** `st = model_rank_state_table[cRun << 11 | cR4 << 3 | rankHist[ch]]`.
2. **Rank.**
   - If `avg < 32`, decode the *rank flag*.
     - If it is 0, `rank = 1` and `rankHist[ch] = 0`.
     - Otherwise:
       1. Decode exponent bits with `size` starting at 1. Stop when `size = maxRank` or a 0 is decoded. Each 1
          increments `size`; refuse `size > 7`.
       2. Set `rankHist[ch] = size`.
       3. Decode `size` mantissa bits, most significant first, into `rank`, which starts at 1: `rank = 2·rank + bit`.
   - If `avg ≥ 32`, decode the `maxRank + 1` *escape* bits directly into `rank`, which starts at 0. Refuse `rank = 0`,
     and set `rankHist[ch] = ⌊log2 rank⌋`.
   - Refuse `rank > lastRank`.
3. **MTF update.** Move `ch` to position `rank`, shifting `T[1..rank]` down one place. Then
   `avg = (avg·124 + rank·4) >> 7` and `r = rank − 1`.
4. **Run state:** `st = model_run_state_table[cR0 << 10 | cRun << 6 | min(r,7) << 3 | min(runHist[ch],7)]`.
5. **Run.** Decode the *run flag*.
   - **0:** output one `ch`, and set `runHist[ch] = (runHist[ch] + 2) >> 2` and `cRun = (cRun << 1 | 1) & 15`.
   - **1:**
     1. Decode exponent bits, `size` from 1 until a 0; refuse `size > 30`.
     2. Set `runHist[ch] = (runHist[ch] + 3·size + 3) >> 2`.
     3. Decode `size` mantissa bits into `run`, which starts at 1.
     4. Set `cRun = (cRun << 1 | (run < 3)) & 15`.
     5. Refuse when `run` passes `n'`; otherwise output `run` copies of `ch`.
6. In both cases `cR0 = (cR0 << 1 | (r = 0)) & 7` and `cR4 = (cR4 << 2 | min(r, 3)) & 255`.

**Cells of each bit.** Every bit takes three counters, in the order *char, state, static*: the cell of `ch`, the cell
of `st`, and the shared cell. The adaptive model also takes one mixer. Each counter set has its constant pair `XY`; a
mixer set's constants are those of the same name. `i` is the bit's index within its group.

| Bit | Counter cells (char, state, static) | Constants | Adaptive mixer |
|---|---|---|---|
| rank flag | `RankChar[ch]`, `RankState[st]`, `RankStatic` | `RANK_TC, TS, TP`; probability `TM` | `mixRank[ch]` |
| rank exponent, `size` | `RankExp*[·][size − 1]` | `RANK_EC, ES, EP`; `EM` | `mixRankExp[max(rankHist, size)][size]`, with the history before this symbol |
| rank mantissa, tree node `j` | `RankMant[size]*[·][j]` | `RANK_MC, MS, MP`; `MM`; static: rounded form | `mixRankMant[size]` |
| escape, tree node `j` | `RankEsc*[·][j]` | `RANK_PC, PS, PP`; `PM`; static: rounded form | `mixRankEsc[j]` |
| run flag | `RunChar[ch]`, `RunState[st]`, `RunStatic` | `RUN_TC, TS, TP`; `TM` | `mixRun[ch]` |
| run exponent, `size` | `RunExp*[·][size − 1]` | `RUN_EC, ES, EP`; `EM` | `mixRunExp[max(runHist, size)][size]`, with the history before this symbol |
| run mantissa | `RunMant[size]*[·][j]` | `RUN_MC, MS, MP`; `MM`; static: rounded form | `mixRunMant[size]` |

- The mantissa and escape tree node `j` starts at 1 and becomes `2j + bit`.
- For run mantissas with `size > 5`, `j` instead counts 1, 2, 3, … .
- The adaptive model updates every counter with the plain form.

### 22.6 Inverse BWT

`t` is the QLFC output of `m` bytes, and `index` is the row of the end-of-text marker in the full BWT.
1. Insert the marker at position `index` to get `m + 1` rows. The marker sorts before every byte.
2. With the usual LF mapping, the text's last byte is the byte of row 0. Each earlier byte is the byte of the row
   reached by LF from the previous one.

The samples are a decoding aid. Sample `t` is the row of the suffix that starts at text position `(t + 1)·2^rlog`,
which lets a decoder start `k + 1` chains in parallel. They are only range-checked, and a decoder may ignore them.
When they do not match the text, a decoder that ignores them produces the text, and one that uses them (as 0.0.2 does
on some inputs) produces other bytes, which fail the block checksum. Either way such a frame never decodes to wrong
bytes.

### 22.7 LZP

`lzp := nblocks u8 | …`. The block table works as in §22.4: one block follows directly, or `nblocks` `(out, in)`
pairs, with `in = out` meaning stored. Each coded block starts with an empty hash table of `2^lzp_bits` entries.
1. Copy 4 bytes. Refuse a block shorter than 4 bytes.
2. Let `ctx` be the last 4 output bytes, big-endian, and `h = ((ctx >> 15) ^ ctx ^ (ctx >> 3)) mod 2^lzp_bits`.
3. At each input byte: let `v = table[h]`, then set `table[h]` to the current output length.
   - If the byte is `0xF2` and `v > 0`, read the next byte (refuse when there is none).
     - If it is 255, output `0xF2`.
     - Otherwise it starts a length: `len = lzp_min + bytes`, adding bytes while each one is 254. Refuse when the length
       passes the output limit. Copy `len` bytes from output position `v`, byte by byte (overlap allowed), and recompute
       `ctx` from the last 4 output bytes.
   - Otherwise copy the byte.
4. Refuse when the output would pass its limit.

The LZP output must be exactly `n` bytes.

### 22.8 Test coverage

`tests/bwt_reference.py` decodes every codec-18 block and graph leaf of the corpus: 34 in all, with both QLFC models
and with and without LZP. Blocks without a transform, or under a section-20 transform, are compared with the original.
Blocks under the shuffle are checked against the block checksum.

On mutated corpus streams it agreed with the 0.0.2 C decoder on every accept, refusal and output byte. The exception is
streams whose sampled indexes no longer match the text, as §22.6 allows.

## 23. kimg (codec 31)

A lossless coder for 8-bit images: each sample is predicted from its neighbours and the residual is coded bit by bit
with adaptive binary contexts mixed by a small logistic mixer. All arithmetic is on integers. "Divide" means C
division, which truncates toward zero. `tests/kimg_reference.py` is the independent decoder.

### 23.1 Stream

```
kimg := version u8 | ich u8 | t u8 | 0 u8 | iw u32 | phase u32 | head u32 | npx u32 | 0 u32 | head bytes | tail bytes | coded bytes
```

- `version` is 1 or 2. 0.0.2 writes 2; 1 is read only.
- `ich` is the number of interleaved channels (1-4).
- `t` is the colour transform: 0 none, 1 subtract green, 2 YCoCg-R.
- `iw` is the row length in bytes, and `phase` is the byte column of the first pixel byte.
- The block is `head` stored bytes, `npx` coded pixel bytes, then `tail = n − head − npx` stored bytes.

Refuse when:
- the stream is shorter than 24 bytes;
- `version` is not 1 or 2, `t > 2`, or byte 3 or the last `u32` is not 0;
- `ich` is outside 1-4, `iw` is 0 or not a multiple of `ich`, or `phase ≥ iw`;
- `head > n` or `npx > n − head`;
- `t ≠ 0` and `ich < 3`, or `phase` or `npx` is not a multiple of `ich`;
- the stored bytes do not fit.

The coded bytes decode to `npx` samples `s`. With `t = 0` they are the pixel bytes. Otherwise channels 1 and 2 range
over 0-510, and every whole pixel `k` (a multiple of `ich`) is inverted:
- **t = 1:** `G = s0`, `R = s1 − 255 + G`, `B = s2 − 255 + G`.
- **t = 2:** `Co = s1 − 255`, `Cg = s2 − 255`, `tt = s0 − (Cg >> 1)`, `G = Cg + tt`, `B = tt − (Co >> 1)`, `R = B + Co`.

Refuse a pixel with `R`, `G` or `B` outside 0-255. Channels 3 and up are copied as they are. `V[c]`, the largest value
of channel `c`, is 510 for channels 1 and 2 under a transform, else 255.

### 23.2 Primitives

**Range decoder.** `x1 = 0`, `x2 = 2^32 − 1`, and `x` is the first 4 coded bytes, big-endian. A byte past the end reads
as 0 and increments an overrun count. To decode a bit with `p`, the 12-bit probability of a 1:
1. Let `xm = x1 + ((x2 − x1) >> 12)·p`.
2. The bit is 1 when `x ≤ xm`; then `x2 = xm`. Otherwise `x1 = xm + 1`.
3. While the top bytes of `x1` and `x2` are equal, shift all three left by 8: `x2` takes 255 as its new low byte and
   `x` takes the next byte.

All values are modulo 2^32.

**Squash and stretch.**
- `squash(d)` clamps `d` to ±2047, and for `i = d + 2048` interpolates 33 points:
  `t = 1, 2, 3, 6, 10, 16, 27, 45, 73, 120, 194, 310, 488, 747, 1101, 1546, 2047, 2549, 2994, 3348, 3607, 3785, 3901,
  3975, 4024, 4050, 4068, 4079, 4085, 4089, 4092, 4093, 4094`.
  With `w = (i − 2048) & 127` and `j = ((i − 2048) >> 7) + 16`, `squash = 4095` when `j ≥ 32`, else
  `(t[j]·(128 − w) + t[j+1]·w + 64) >> 7`.
- `stretch[p]`, for `p` in 0-4095, is the smallest `d` in −2047..2047 with `squash(d) ≥ p`, and 2047 when there is none.

**Counters** hold a 16-bit probability `q` (start 32768), a count `m` (start 0) and a limit: 255 for the low-mantissa
counters, 250 for all others. `dt[m] = ⌊131072 / (2m + 3)⌋`. After bit `b`:
- `q += ((65535·b − q) · dt[m]) >> 16`, kept to 16 bits;
- then `m += 1` if `m <` the limit.

**Mixed bit.** A *node* of a sample has four counters and a weight row `w[0..4]`.
1. `st[i] = stretch[q_i >> 4]` for i = 0..3, and `st[4] = 256`.
2. `p = squash((Σ w[i]·st[i]) >> 16)`, with a 64-bit sum and arithmetic shift, clamped to 1..4094.
3. Decode the bit with `p`.
4. Then `w[i] += (st[i] · ((b << 12) − p)) >> 12` for all five, and update the four counters.

Weights start at 19660 (`⌊0.3·65536⌋`) for i = 0..3 and 0 for i = 4.

A **direct bit** uses one counter: `p = q >> 4` clamped to 1..4094. Decode, then update the counter.

### 23.3 Sample loop

Samples are numbered `k = 0 … npx − 1`. Sample 0 sits at byte column `x = phase` and channel `c = phase mod ich`, and
row parity starts at 0. After each sample, `x` advances. At `x = iw`, `x` returns to 0, the parity flips and `c`
returns to 0; otherwise `c` advances modulo `ich`.

**Neighbours.** Each is a sample already decoded.
- `W = s[k−ich]` if `x ≥ ich` and `k ≥ ich`.
- `N = s[k−iw]` if `k ≥ iw`.
- `NW = s[k−iw−ich]` if N exists, `x ≥ ich` and `k ≥ iw+ich`.
- `NE = s[k−iw+ich]` if `k+ich ≥ iw`, `x+ich < iw` and `k ≥ iw−ich`.
- `WW = s[k−2ich]` if `x ≥ 2ich` and `k ≥ 2ich`.
- `NN = s[k−2iw]` if `k ≥ 2iw`.
- `NNE = s[k−2iw+ich]` if both NN and NE exist.

Missing neighbours default as follows:
- `N` is `W` (or 0 when W is missing too);
- `W` is `N`, and so are `NW` and `NE`;
- `WW` is `W`, `NN` is `N`, and `NNE` is `NE`.

**Predictors.**
- `P0 = W + N − NW`.
- `P1` is the median predictor of LOCO-I: `min(W,N)` if `NW ≥ max(W,N)`, `max(W,N)` if `NW ≤ min(W,N)`, else `P0`.
- `P2 = (3W + 3N − 2NW + NE − NN + WW + 2) >> 2`.
- `P3 = N + NE − NNE`, `P4 = W + NE − N`, `P5 = N`, `P6 = W`.
- `P7 = P0 + E0`, `P8 = P1 + E1`, `P9 = P2 + E2`. `E0..E2` are, for channel `c > 0` of the same pixel, the
  previous channel's `v − P0`, `v − P1` and `v − P2`, and 0 for the first channel of a pixel.

**Error memory.** For each byte column and row parity the decoder keeps 11 values: the 10 predictors' absolute errors
and the residual's magnitude, all 0 at the start.
- `eW` is the current parity's entry at `x − ich`, and `eWW` its entry at `x − 2ich`.
- `eN`, `eNW` and `eNE` are the other parity's entries at `x`, `x − ich` and `x + ich`.
- Each is all zeros when its neighbour is missing. `eWW` is all zeros in version 1.

**Prediction.**
1. For predictor `i`, `s_i = o + eW[i] + eN[i] + eNW[i] + eNE[i] + eWW[i]`, where `o` is 4 in version 2 and 1 in
   version 1. Its weight is `⌊2^44 / s²⌋` with `s = min(s_i, 16383)`; for `s = 0` it is the weight of 1.
2. `pr0 = (Σ weight·P_i + ⌊Σ weight / 2⌋) / Σ weight`.
3. **Expected error:** `ee = 2·eW[10] + 2·eN[10] + eNW[10] + eNE[10]`, plus `2·|r'|` when `c > 0`, where `r'` is the
   previous channel's residual. The bucket `a` is the number of thresholds `1, 3, 6, 10, 16, 24, 36, 54, 80, 120, 200`
   that are `≤ ⌊ee / 2⌋` (0-11).
4. **Bias.** Let `g1 = N − NW`, `g2 = W − NW` and `sc = sgn(g1) + 1 + 3·(sgn(g2) + 1)`. With the bias cell
   `(sum, count)` of `(c, a, sc)`, add `(sum ± ⌊count/2⌋) / count` to `pr` when `count > 0` (`+` when `sum ≥ 0`).
5. `P = clamp(pr, 0, V[c])`.

**Contexts.** `q8(v)` maps `|v|` to 0, 1, 2, 3, 4, 5, 6, 7 for 0, 1, 2, ≤4, ≤8, ≤16, ≤32 and above.
`tx = sc + 9·(sgn(NE − N) + 1)`. The four counter rows of the sample are:
- `T0[c][a]`;
- `T1[c][q8(eW[10])][q8(eN[10])]`;
- `T2[c][tx][a >> 1]`;
- `T3[c][q8(r') if c > 0 else q8(eNW[10])][q8(eNE[10])]`.

Each row has 27 nodes, and the weight row is `Wt[c][a][node]`.

**Residual.** Decode a folded value `u`:
1. Node 0 gives 1 for `u = 0`.
2. Otherwise decode the bit length `L` from 0, while `L < LM` and node `1 + L` gives 1. `LM` is 8 when `V[c] > 255`,
   else 7. Then `u = 2^L`.
3. If `L ≥ 1`, node `9 + L` gives bit `L − 1`. If `L ≥ 2`, node `18 + L` gives bit `L − 2`.
4. Bits `L − 3` down to 0 are direct bits with counter `Low[c][L][bit]`.
5. Refuse `u > V[c]`, or an overrun count above 8.

**Unfold.** Let `m = min(P, V − P)`.
- `u = 0` gives 0.
- `u ≤ 2m` gives `+(u+1)/2` for odd `u` and `−u/2` for even `u`.
- Larger `u` gives `u − m`, positive when `P < V − P`, else negative.

The sample is `v = P + unfold(u)`.

**Updates.**
- Bias cell: `sum += v − pr0` and `count += 1`. When `count` reaches 512 (64 in version 1), divide both by 2.
- Error memory at the current parity and `x`: `|v − clamp(P_i, 0, V[c])|` for i = 0..9, and `|v − P|` for entry 10.
- For the next channel of the same pixel, `E0..E2` and `r' = v − P`. After the last channel all four become 0.

### 23.4 Test coverage

`tests/kimg_reference.py` decodes the three kimg blocks of the corpus to their originals: one channel, three channels,
and three channels with transform 1. Against the 0.0.2 C decoder, mutated streams covering 1-4 channels, transforms
0-2 and version 1 were decoded identically.

## 24. Context mixing (codecs 7, 17 and 25)

Codec 7 (CM) codes each bit of the output with a probability that several context models predict and two to five
mixers combine. Codec 17 (CMI) is the same coder with contexts taken from an image's neighbouring samples, and codec 25
(CMP) cuts a block into parts and codes each with its own codec-7 stream. There are four models. Model 1 is the
smallest. Model 2 adds contexts, two mixers, a learned final mixer and a third APM. Model 3 adds more contexts and a
better match model. Model 4 keeps a chosen subset of model 3's contexts in smaller tables, with a 16-bit mixer.

All arithmetic is on integers. Hashes and the coder's registers are modulo 2^32. `>>` on a negative value is an
arithmetic shift (rounds toward −∞). "Divide" means integer division of non-negative values. `tests/cm_reference.py`
is the independent decoder. The mixer priors are normative data: `tests/format_tables/cm_prior.json`, copied verbatim
from `src/codecs/cm_prior.inc` at tag `v0.0.2`.

### 24.1 Streams

**Codec 7.** A `size` u32 (little-endian) comes first. Its low 28 bits must equal the block length `n`, and its top
4 bits `f` select the header. Blocks of 2^28 bytes or more always use the legacy model-1 header. This section covers
only shorter blocks.

| `f` | header | model | tables | coded bytes start at |
|---|---|---|---|---|
| 0 | `size` | 1 | legacy, `bb` = 20 | 4 |
| 1-7 | `size` | 1 | legacy, `bb = f + 15` | 4 |
| 9-15 | `size` | 2 | legacy, `bb = (f & 7) + 15` | 4 |
| 8 | `size`, `mb << 4 \| g` | see below | see below | 5 |

With `f = 8`, the fifth byte holds a model nibble `mb` and a low nibble `g`:
- `mb` 3, 5, 6, 7: model 3, 1, 2, 3, with `bb = g + 15`. Nibble 3 uses the legacy tables; 5, 6, 7 use the exact tables.
- `mb` 14, 15: models 2 and 3 with the exact tables and `bb = g + 15`. Three tune bytes `b0 b1 b2` follow, and the
  coded bytes start at 8.
- `mb` 9: model 4, with exact tables and size shift `s = g + 2`. `b0 b1 b2` follow, then a context mask u32, and the
  coded bytes start at 12. This header is read only when the stream has at least 16 bytes.

Refuse when:
- the stream is shorter than 8 bytes;
- `f = 8` and the stream is shorter than 9 bytes, or `mb` is none of 3, 5, 6, 7, 9, 14, 15;
- `mb` is 14 or 15 and the stream is shorter than 12 bytes or `b2 > 1`;
- `mb` is 9 and `b2 > 2`, or the mask has bit 0, 30 or 31 set;
- the low 28 bits of `size` differ from `n`;
- the model is not 4 and `bb` is outside 16-22.

A decoder may also refuse, before allocating, a stream whose tables exceed the frame's declared working set
(section 11). Streams without tune bytes use `b0 = b1 = b2 = 0`.

**Codec 17.**

```
cmi := iw u32 | ih u32 | ich u8 | size u32 | coded bytes
```

- `iw` is the row length in bytes, `ih` the number of bytes before the first pixel, and `ich & 127` the number of
  interleaved channels.
- Bit 7 of `ich` selects the exact tables; without it the legacy tables are used.
- `size` is read as for codec 7 with `f & 8` set and `bb = (f & 7) + 15`. The coded bytes start at 13.
- The image model has model 2's tables and mixers.

Refuse when:
- the stream is shorter than 17 bytes;
- the size does not match `n`, `f & 8` is clear, or `bb` is outside 16-22;
- `iw = 0`, the channel count is outside 1-4, `iw <` channels, or `ih > n`.

**Codec 25.**

```
cmp := 0 u8 | parts uv | parts × (raw uv | coded uv) | the parts' codec-7 streams, concatenated
```

`uv` is the LEB128 varint of section 14.1. Part `k` decodes its codec-7 stream to `raw_k` bytes, and the parts follow
one another in the block.

Refuse when:
- the stream is shorter than 2 bytes, or the version is not 0;
- `parts` is outside 1-256;
- a `raw_k` is 0 or more than the bytes still unassigned, or a `coded_k` exceeds the stream length;
- the `raw_k` do not add up to `n`;
- a part runs past the end of the stream, or bytes follow the last part;
- any part fails to decode.

Encoders write model 4 or model 1 parts. Columns of a transposed table (plan type 1) are one example.

### 24.2 Fixed tables

**Bit histories.**
- A state is a triple (`n0`, `n1`, last bit). State 0 is (0, 0, 0).
- After bit 1, `n1 = min(n1 + 1, 24)`, and if `n0 > 2` then `n0 = (n0 + 1) / 2`. After bit 0 it is the same with the
  roles swapped.
- States are numbered by breadth-first search from state 0, trying bit 0 before bit 1 and giving each new triple the
  next number. There are 215 states.
- `next[s][y]` is the transition. Numbers 215-255 have `n0 = n1 = 0` and `next = 0`.

**Squash, legacy.**
- The 33 knots are `K[k] = ⌊4096 / (1 + e^((16 − k)/2))⌋` for k < 16, `K[16] = 2047`, and `K[32 − k] = 4095 − K[k]`.
- `squash(x)` is 4095 for `x ≥ 2048` and 1 for `x ≤ −2048`.
- Otherwise, with `i = (x + 2048) >> 7` and `w = (x + 2048) & 127`, `squash(x) = (K[i]·(128 − w) + K[i+1]·w + 64) >> 7`.
- `SQ[x]` for `x` in −2048..2047 is `squash(x)` clamped to 1..4095.

**Squash, exact.**
- `SQ[x]` is `⌊4096 / (1 + e^(−x/256)) + 0.5⌋` clamped to 1..4095.
- No value lies within 0.00017 of a rounding boundary, so any correctly rounded `exp` gives the table.

**Stretch.** For each table, `ST[p]` (p = 0..4095) is the smallest `x` in −2047..2047 with `SQ[x] ≥ p`, else 2047.
`sq(x)` means `SQ[x]` after clamping `x` to −2047..2047.

**Other tables.**
- `rcp[i] = 655360 / (10i + 16)` for i < 64, and `srcp[i] = 655360 / (10i + 15)` for i < 1024.
- `pri[s] = n0 + n1`.
- `dx[s]`: let `c = min(n0 + n1, 7)`. It is 0 when both counts are non-zero or both are zero, `128(c + 1)` when only
  `n1` is non-zero, and `−128(c + 1)` when only `n0` is.

`hmix(a)`: `a ^= a >> 15; a *= 0x2C1B3C6D; a ^= a >> 12; a *= 0x297A2D39; a ^= a >> 15`, all modulo 2^32.

### 24.3 State

**Context slots.** Slot 0 is the order-1 direct counter. The other slots are hashed contexts, and slot `k` holds
context `mid[k]`:
- Model 1: contexts 1-15 (`nm = 16` slots).
- Model 2 and the image model: contexts 1-20 (`nm = 21`).
- Model 3: contexts 1-29 (`nm = 30`).
- Model 4: the contexts whose mask bit is set, in increasing order.

The mixers have `ni = 2·nm + 3` inputs.

**Hashed tables, models 1-3 and the image model.**
- Context `i` has `2^b` buckets of 16 words. `b = bb`, except for `i ≥ 16` with `bb > 18`, where
  `b = bb − 1 − (i ≥ 25)`.
- Word 0 of a bucket is `check << 16 | priority`. Words 1-15 are counters for the 15 nodes of a nibble's binary tree.
- A counter word is `p << 16 | state << 8 | count`: a 16-bit probability, a bit-history state and a count of 0-15.
- All words start at 0.

**Hashed tables, model 4.**
- Context `i` has `L_i` lines of 64 bytes. `L_i = n >> s`, then `>> 1` for contexts 16-24 or `>> 2` for 25-29,
  clamped to 2^12..2^22.
- Bytes 0-3 of a line are the check bytes of its four buckets. Bucket `j` holds 15 state bytes at `4 + 15j` to
  `18 + 15j`, one per node.
- All bytes start at 0.

**Direct counters.** 65536 counter words, each starting at `0x80000000`.

**State maps.**
- Each slot has 2048 entries indexed by `state | bitpos << 8`. An entry is `p << 10 | count`, with a 22-bit
  probability.
- Entry `j` of every slot starts with `p = ((2n1 + 1) << 22) / (2(n0 + n1) + 2)` for state `j & 255` and count 0.

**Mixers.** Each mixer has one weight row per selector value. Every row starts as a copy of that mixer's template:

| mixer | rows | selected by |
|---|---|---|
| `W1` | 768 | `c0 + 256·class` |
| `W2` | 128 | `(ml·8 + bitpos)·4 + sel2` |
| `W3` (models 2-4) | 2048 | `sel3 << 3 \| bitpos` |
| `W4` (models 2-4) | 65536 | `c0 \| sel4 << 8` |
| `WF` (models 2-4) | 1024 | `c0 \| ml << 8` |

`WF` is the final mixer, with five weights.

**Templates.** Each prior row has 63 inputs, and a value is used times 4.
- Models 1-3 and the image model: every weight is 2^14, and `WF` is `2^14, 2^14, 2^14, 2^14, 0`.
- With `b2 = 1`, models 2 and 3 take row `r` of `cm_prior[0]` (model 2) or `cm_prior[1]` (model 3) as `W1`-`W4`
  (inputs 0 to `ni − 1`), and the first five values of row 4 as `WF`.
- Model 4 weights are 16-bit, with 4096 = 1.0. Every weight is 1024, and `WF` is as above.
- With `b2 ≥ 1`, model 4 uses `P = cm_prior4[b2 − 1]`. Input `j` of `W1`-`W4` takes `clamp((4·P[r][q]) >> 4, −32768,
  32767)`, where `q` is the model-3 input it stands for:
  - `mid[k]` for input `k`;
  - `30 + k` for input `nm + k` (k < 3);
  - `33 + mid[k]` for input `nm + 3 + k`.
- With `b2 ≥ 1`, `WF` is four times the first five values of `P[4]`.

**APMs.**
- An APM has 33 cells of 16 bits per selector value: `A1` (256 values), `A2` (65536) and, for models 2-4, `A3`
  (65536).
- Every cell `j` starts at `16·sq(128(j − 16))`.

**Match counters.** 64 counters of 17 bits, each starting at 2^15.

**Learning settings** come from `b0` and `b1`. Each takes one value from a list of four, indexed by a 2-bit field:

| field | setting | values |
|---|---|---|
| `b0 & 3` | mixer shift `lr` | 12, 11, 13, 14 |
| `b0 >> 2 & 3` | counter limit `lim` | 2, 1, 3, 4 |
| `b0 >> 4 & 3` | APM shift `ash` | 13, 12, 14, 15 |
| `b0 >> 6` | match shift `msh` | 6, 5, 7, 8 |
| `b1 & 3` | rate floor `lrk` | 20, 12, 28, 40 |
| `b1 >> 2 & 3` | decay shift `lrd` | 13, 12, 14, 16 |
| `b1 >> 4 & 3` | state-map limit `smlim` | 255, 127, 511, 1023 |
| `b1 >> 6` | final weights `fw` | (2,2,2,2), (4,1,1,2), (1,2,2,3), (3,1,2,2) |

Counters use `rate[c] = rcp[c·lim]`.

**History.**
- `buf` holds the decoded bytes. `pos` is their count, `c0` the partial byte with a leading 1 (1 at a byte start), and
  `bitpos` 0-7.
- `c4` holds the last four bytes (the low byte is the latest), and `c8` the four before.
- Every other variable below starts at 0.

### 24.4 Coding a bit

**Bucket lookup.** At `bitpos` 0 and 4, every hashed slot `k ≥ 1` looks up its bucket with
`h = hmix(H[mid[k]] + nib)`. `nib` is 0 at `bitpos` 0 and `(c0 & 15)·0x5BD1E995` at `bitpos` 4.

*Models 1-3 and the image model:*
1. Let `i = h >> (32 − b)` and `chk = h & 0xFFFF`.
2. Take bucket `i` if its check equals `chk`, else bucket `i ^ 1` if its check equals `chk`.
3. Otherwise empty the bucket with the smaller priority (bucket `i` on a tie): set word 0 to `chk << 16` and words
   1-15 to `0x80000000`.

*Model 4:*
1. Take line `(h·L) >> 32` and `chk = h & 255`.
2. Take the first bucket whose check byte equals `chk`.
3. Otherwise empty the bucket `j` with the smallest `pri` of its first state byte (the first on a tie): zero its 15
   state bytes and set its check byte to `chk`.

**Node and selectors.**
- `node = (c0 & (2^bitpos − 1)) | 2^bitpos` for `bitpos < 4`. With `t = bitpos − 4` it is `(c0 & (2^t − 1)) | 2^t`.
- `c1` is the last byte (`c4 & 255`); the image model uses `i_pred` instead.
- `class` is 0 when `c1 | 32` is a letter, 1 for a digit and 2 otherwise; the image model uses `i_cls`.
- `ml` is 0 without a match, else 1, 2 or 3 for match lengths below 16, below 32, and longer.

**Inputs.** `x` has `ni` entries.
- Slot 0 is the direct counter `D[c0 | c1 << 8]`.
- Slot `k ≥ 1` is the bucket's node `node`: a counter word in models 1-3, a state byte in model 4.
- For each slot, let `st = state | bitpos << 8`, where `state` is bits 8-15 of a counter word or the state byte.
  `x[nm + 3 + k] = ST[map_k[st] >> 20]`, the top 12 bits of the map entry's probability.
- For a counter word, `x[k] = ST[word >> 20]`.
- For a state byte (model 4, slots `k ≥ 1`), `x[k] = dx[state]`.

**Match input.** The match model (24.6) holds `mlen` and `mptr`.
- With `mlen > 0` and the top `bitpos` bits of `buf[mptr]` equal to the decoded bits of this byte (`(buf[mptr] + 256)
  >> (8 − bitpos) = c0`), the expected bit is `e = (buf[mptr] >> (7 − bitpos)) & 1`.
- `l = min(mlen, 31)`. Models 3 and 4 refine `mlen ≥ 16` into the class 16 + the number of these bounds that `mlen`
  reaches: 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 4096.
- Counter index `mi = 2l + e`. `x[nm] = ST[clamp(M[mi] >> 4, 1, 4095)]`, and `x[nm + 1]` is `16l` if `e = 1`, else `−16l`.
- Without an expected bit, `x[nm] = x[nm + 1] = 0`.
- `x[nm + 2] = 64`.

**Mixers.**
- Selectors: `sel2` is `c1 >> 6` (image: `i_act`), `sel3` is byte 2 of `c4` (image: `i_n`), and `sel4` is `c1`
  (image: `i_med`).
- Model 1: `d_r = (Σ W_r[i]·x[i]) >> 16` for `W1` and `W2`. `pr_r = sq(d_r)`, and `t = (d_1 + d_2) >> 1`.
- Models 2 and 3, image: `d_r = (Σ W_r[i]·x[i]) >> 16` for the four mixers.
- Model 4: pad `x` with a zero to an even length. For each mixer, `d_r = (Σ_pairs ((x[2j]·W[2j] + x[2j+1]·W[2j+1]) >> 8))
  >> 4`.
- Models 2-4 then clamp each `d_r` to ±2047, and `pr_r = sq(d_r)`.
- They compute `t = (WF[4]·256 + Σ_r WF[r]·d_r) >> 16`, clamped to ±2047, and `prf = sq(t)`.

**APMs.**
1. Clamp `t` to ±2047. `mixed = sq(t)`, `lo = (t + 2048) >> 7`, and `wt = (t + 2048) & 127`.
2. `p1` interpolates `A1[c0]` and `p2` interpolates `A2[(c0 | c1 << 8) & 0xFFFF]`, each as
   `(a[lo]·(128 − wt) + a[lo+1]·wt) >> 11`.
3. Model 1: `p = (mixed + p1 + p2) / 3`.
4. Models 2-4: `p3` interpolates `A3[hmix(v·0x9E3779B1 + c0·0x85EBCA6B) >> 16]`, where `v` is `c4 & 0xFFFF`
   (image: `i_wn`). Then `p = (fw0·mixed + fw1·p1 + fw2·p2 + fw3·p3 + 4) >> 3`.
5. Clamp `p` to 1..4095.

**Arithmetic decoder.**
- `low = 0`, `high = 2^32 − 1`, and `code` is the first four coded bytes, big-endian.
- `mid = low + (((high − low)·p) >> 12)`. The bit `y` is 1 when `code ≤ mid`; then `high = mid`. Otherwise
  `low = mid + 1`.
- After the updates below, while `(low ^ high) < 2^24`:
  - `low <<= 8`;
  - `high = high << 8 | 255`;
  - `code = code << 8 | the next byte`.
- A byte past the end reads as 255 and counts as padding. Refuse when the padding exceeds 8 at the start of a byte or
  at the end.

### 24.5 Updates after a bit

1. **State maps.** For each slot's entry `(p, c)`: `p += (((y << 22) − p)·srcp[c]) >> 16`, capped at 2^22 − 1. Then
   `c += 1` if `c < smlim`.
2. **Counter words** (slot 0 always, slots `k ≥ 1` in models 1-3):
   - `p += ((65535y − p)·rate[c]) >> 16`, clamped to 0..65535;
   - `state = next[state][y]`;
   - `c = min(c + 1, 15)`.
3. **Model-4 state bytes** become `next[state][y]`.
4. **Bucket priorities** (models 1-3): each looked-up bucket's priority increases by 1, up to 65535.
5. **Match counter.** If an expected bit was used, `M[mi] += ((y << 16) − M[mi]) >> msh`.
6. **Mixers.** Let `e_r = (y << 12) − pr_r`.
   - Model 1: `W_r[i] += (x[i]·e_r) >> lr`.
   - Models 2-4: let `k = max(64 − (pos >> lrd), lrk)` and `e_r = (e_r·k) >> 4`.
   - Models 2 and 3, image: `W_r[i] += (x[i]·e_r) >> lr`.
   - Model 4: `W_r[i] = clamp(W_r[i] + ((x[i]·(e_r >> (lr − 11)) + 2^14) >> 15), −32768, 32767)`.
   - Then `WF[r] += (xf[r]·((y << 12) − prf)) >> 12` for r = 0..4, where `xf` is the clamped `d_1..d_4` and 256.
7. **APMs.** `A3` (models 2-4), then `A1` and `A2`:
   - `a[lo] += ((g − a[lo])·(128 − wt)) >> ash`;
   - `a[lo+1] += ((g − a[lo+1])·wt) >> ash`;
   - `g = 65535y`.
8. **Broken match.** If the expected bit differed from `y`, `mlen` becomes 0. In models 3 and 4, first set `mrec` to
   8 if `mlen ≥ 16`, else `max(mrec − 1, 0)`.
9. `c0 = c0 << 1 | y`. After the eighth bit, the byte `c = c0 & 255` is decoded:
   - `buf[pos] = c` and `pos += 1`;
   - `c8 = c8 << 8 | c4 >> 24` and `c4 = c4 << 8 | c`;
   - `c0 = 1`;
   - the contexts of 24.6 are computed.

### 24.6 Contexts after a byte

**Words.** If `c | 32` is a letter or `c ≥ 128`, `word = (word + (c | 32) + 1)·0x3D4D51CB`. Otherwise, when
`word ≠ 0`, it shifts out: `word2 = word1`, `word1 = word`, `word = 0`.

**Record length.**
1. Let `d = pos − last[c]`.
2. If `last[c] ≠ 0`, `d = dist[c]` and `2 ≤ d < 65536`:
   - if `d = cand`, increment `cnt` up to 255, and set `rlen = d` once `cnt ≥ 8`;
   - else if `cnt > 0`, decrement it;
   - else `cand = d` and `cnt = 1`.
3. Then `dist[c] = d` and `last[c] = pos`.
4. With `r = rlen`, `ab = buf[pos − r]` and `ab2 = buf[pos − 2r]` when those positions exist and `r ≠ 0`, else 0.
   `col = min(pos mod r, 4095)` when `r ≠ 0`, else 0.

**Indirect.** With `p1 = c4 >> 8 & 255` and `p2 = c4 >> 8 & 0xFFFF`:
- `I1[p1] = (I1[p1] << 8 | c) & 0xFFFF`;
- `I2[p2] = (I2[p2] << 8 | c) & 0xFFFF`.

**Lines.**
- At `c = 10`, `line1 = line0` and `line0 = pos`.
- Let `col = pos − line0` and `plen = line0 − line1`.
- `above = buf[line1 + col]` if `col < plen`, else 0, and `above2 = buf[line1 + col + 1]` if `col + 1 < plen`,
  else 0.

**Context hashes.** The variables above are updated first, then the hashes. All products are modulo 2^32, `A = 0x9E3779B1`, `B = 0x85EBCA6B`, `C = 0xC2B2AE35` and `E = 0x2545F491`.

| i | `H[i]` |
|---|---|
| 1 | `hmix((c4 & 0xFF) + 0x1000000)` |
| 2 | `hmix((c4 & 0xFFFF) + 0x2000000)` |
| 3 | `hmix((c4 & 0xFFFFFF)·A + 3)` |
| 4 | `hmix(c4·A + 4)` |
| 5 | `hmix(hmix(c4) + (c8 & 0xFFFF)·B + 5)` |
| 6 | `hmix(word·0x27D4EB2F + word1·0x165667B1 + 6)` |
| 7 | `hmix((c4 & 0xFF00FF00)·A + 7)` |
| 8 | `hmix(hmix(c4·A) + (c8 & 0xFF)·B + 8)` |
| 9 | `hmix(hmix(c4 + 0x51ED27) + (c8 & 0xFFFFFF)·C + 9)` |
| 10 | `hmix((above << 8 \| above2)·A + (min(col, 255) << 20) + 10)` |
| 11 | `hmix((above << 16 \| (c4 & 0xFFFF))·B + 11)` |
| 12 | `hmix(word·E + 12)` |
| 13 | `hmix(word·A + word1·B + word2·C + 13)` |
| 14 | `hmix((I1[c] << 8 \| c)·A + 14)` |
| 15 | `hmix((I2[c4 & 0xFFFF] << 16 \| (c4 & 0xFFFF))·B + 15)` |
| 16 | `hmix((c4 & 0xFFFF0000)·A + 16)` |
| 17 | `hmix((c4 & 0xFF00)·B + 17)` |
| 18 | `hmix((ab << 16 \| ab2 << 8 \| (r ≠ 0))·A + r·B + 18)` |
| 19 | `hmix((ab << 8 \| (c4 & 0xFF))·C + r·A + 19)` |
| 20 | `hmix((col << 8 \| (c4 & 0xFF))·E + r·B + 20)` |

Row 10 uses the line column, and row 20 the record column.

**Structure (models 3 and 4).**
1. A token byte is a digit, a letter, `_`, `-`, `.` or any byte ≥ 128. On a token byte, `tok = (tok + c + 1)·0x2F0B3A55`.
2. On any other byte:
   - if `tok ≠ 0`: `ltok = tok`; if `lt` is set, then `key = tok`; then `tok = 0`;
   - if `c` is `:` or `=` and `ltok ≠ 0`: `key = ltok`;
   - `lt` is set when `c` is `<`, or when `c` is `/` and `lt` was set.
3. At `c = 10`:
   - copy all 32 field starts `fs` into `pfs`;
   - `pfn = fidx + 1`, `pls = ls` and `ls = pos`;
   - `fidx = 0`, `fs[0] = 0`, `key = 0` and `ltok = 0`.
4. At a space, `,`, tab, `;` or `|` with `fidx < 31`: `fidx += 1` and `fs[fidx] = pos − ls`.
5. When `fidx < pfn`, let `a = pls + pfs[fidx] + (pos − ls) − fs[fidx]`. If `pls ≤ a < ls`, then `fa = buf[a]`, and
   `fa1 = buf[a + 1]` when `a + 1 < ls` (else 0). Otherwise `fa = fa1 = 0`.

| i | `H[i]` |
|---|---|
| 25 | `hmix(key·A + (c4 & 0xFF)·B + 25)` |
| 26 | `hmix(key·C + tok·A + 26)` |
| 27 | `hmix((fidx << 16 \| (c4 & 0xFFFF))·A + 27)` |
| 28 | `hmix((fidx + 1)·B + tok·C + 28)` |
| 29 | `hmix((fa << 16 \| fa1 << 8 \| fidx)·A + (c4 & 0xFF)·E + 29)` |

**Orders 12 and 20 (models 3 and 4).**
- `H[21] = 21` and `H[22] = 22`, then `h = 0`.
- For `k = 1 … min(pos, 20)`, `h = (h + buf[pos − k] + 1)·0x2F0B3A55`. At `k = 12`, `H[21] = hmix(h + 21)`. At
  `k = 20`, `H[22] = hmix(h + 22)`.

**Image model (codec 17).** After the hashes above, `H[1..20]` are replaced.
- `q = pos`. Let `ch` be the channel count and `w = iw`.
- `I(d) = buf[q − d]` if `q ≥ d + ih`, else 0. `cl()` clamps to 0..255.
- Neighbours: `W = I(ch)`, `N = I(w)`, `NW = I(w + ch)`, `WW = I(2ch)`, `NN = I(2w)`, `P1 = I(1)`, `P2 = I(2)`,
  `W1 = I(ch + 1)`, `N1 = I(w + 1)`.
- `NE = I(w − ch)` and `NNE = I(2w − ch)` when `w > ch`, else 0.
- `ci = (q − ih) mod ch` when `q ≥ ih`, else 0.
- `grad = cl(W + N − NW)`. `med` is `min(W, N)` if `NW ≥ max(W, N)`, `max(W, N)` if `NW ≤ min(W, N)`, else `grad`.
  `avg = (W + N + 1) >> 1`.
- `act` counts how many of 3, 8, 16, 32, 64, 128, 256 the sum `|W − NW| + |N − NW| + |N − NE|` reaches (0-7).
- `icp` is `cl(W + P1 − W1)` when `ci > 0`, else `grad`. `icn` is `cl(N + P1 − N1)` when `ci > 0`, else `avg`.
- `H[i] = hmix(v_i·A + ci·A + i·B)`, with:

| i | `v_i` |
|---|---|
| 1 | `W << 8 \| N` |
| 2 | `N << 8 \| NE` |
| 3 | `W << 8 \| NW` |
| 4 | `grad << 3 \| act` |
| 5 | `med << 3 \| act` |
| 6 | `W << 16 \| N << 8 \| NW` |
| 7 | `W << 16 \| N << 8 \| NE` |
| 8 | `N << 8 \| NN` |
| 9 | `W << 8 \| WW` |
| 10 | `(cl(2W − WW) >> 2) << 6 \| cl(2N − NN) >> 2` |
| 11 | `avg << 6 \| ((W − N + 256) & 511) >> 3` |
| 18 | `(grad >> 1) << 7 \| med >> 1` |
| 19 | `(W >> 3) << 20 \| (N >> 3) << 15 \| (NE >> 3) << 10 \| (NW >> 3) << 5 \| P1 >> 3` |
| 20 | `(icp + icn + 1) >> 1` |

Rows 12-17 depend on `ci`:

| i | `ci > 0` | `ci = 0` |
|---|---|---|
| 12 | `icp << 3 \| act` | `cl(N + NE − NNE) << 3 \| act` |
| 13 | `icp << 8 \| P1` | `cl(W + NE − N)` |
| 14 | `P1 << 8 \| W` | `NW << 8 \| NE` |
| 15 | `P1 << 8 \| N` | `((W + NE) >> 1) << 6 \| N >> 2` |
| 16 | `P1 << 8 \| P2` | `(W >> 2) << 18 \| (N >> 2) << 12 \| (NW >> 2) << 6 \| NE >> 2` |
| 17 | `icn << 8 \| W` | `WW << 8 \| NN` |

The selectors for the next byte are:
- `i_pred = icp` and `i_cls = min(ci, 2)`;
- `i_act = act >> 1`, `i_n = N` and `i_med = med`;
- `i_wn = W << 8 | N`.

### 24.7 Match model

The match model runs after the contexts, once `pos ≥ 6`.
1. Let `hh = hmix(c4 ^ ((c8 & 0xFFFF)·A)) & mask`. `mask` is 2^22 − 1 in models 1-3. In model 4 it is 2^m − 1 for the
   smallest `m` in 16-22 with `2^m ≥ n` (22 at most).
2. If `mlen > 0`: `mptr += 1`, and `mlen += 1` up to 65535.
3. Otherwise take `cand = T[hh]` (0 when empty). `len(a)` is the number of equal bytes going backwards from `buf[a − 1]`
   and `buf[pos − 1]`, at most `min(a, 400)`.
   - **Models 1 and 2, image:** if `cand > 0` and `len(cand) ≥ 6`, then `mlen = len(cand)` and `mptr = cand`.
   - **Models 3 and 4:** follow the chain from `cand` through `prev[cand]`, at most 16 positions while `cand > 0`. Keep
     the first position with the longest `len`, and stop early at 400. If the best length is ≥ 6, take it.
4. **Models 3 and 4:** if nothing was taken, `mrec > 0` and `mptr + 1 < pos`, then `mptr += 1` and `mlen = 1`. Set
   `prev[pos] = T[hh]`.
5. `T[hh] = pos`. If `mlen > 0` and `mptr ≥ pos`, `mlen = 0`.

**Expected-byte contexts (models 3 and 4).**
- `eb = buf[mptr]` when `mlen > 0`, else 256.
- `lq` counts how many of 1, 8, 12, 16, 24, 32, 64 `mlen` reaches (0-7).

| i | `H[i]` |
|---|---|
| 23 | `hmix((eb << 3 \| lq)·A + (c4 & 0xFF)·B + 23)` |
| 24 | `hmix((eb << 3 \| lq)·C + (c4 & 0xFFFF)·A + 24)` |

Before the first byte, every `H[i]` is 0.

### 24.8 Test coverage

`tests/cm_reference.py`, in `make check` (about 40 s):
- decodes the 21 vectors in `tests/vectors/cm/` whole. They were written by the 0.0.2 encoder functions and checked
  by its decoders, and cover:
  - models 1, 2 and 3 with legacy and exact tables, and with tune bytes, with and without the prior;
  - model 4 with all contexts, none, three, and the binary set, with priors 0-2;
  - the image model with 1-4 channels, a header, and legacy tables;
  - CMP with model 1 and model 4 parts.
- decodes the first 1024 bytes of every codec 7, 17 and 25 block of the corpus: 26 blocks of model 1, model 4, the
  image model and CMP. Blocks without a transform are compared with the original. The others are compared with
  the SHA-256 digests in `tests/vectors/cm/prefixes.json`.

`make check-cm-reference` decodes every corpus stream whole and checks it against the original through the transform
inverses (about 20 minutes). It passed at the commit that added this section.

The 0.0.2 encoder writes models 1 and 4 only. Models 2 and 3, the tuned headers and the legacy image tables come from
older encoders, so only the vectors cover them. Against the 0.0.2 C decoders, 6,000 mutated streams were decoded
identically, comparing acceptance, refusal and every output byte. The mutations included header fields, coded bytes,
truncation, extension and wrong lengths.
