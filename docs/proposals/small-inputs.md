# Small inputs: format proposals (phase 2)

Status: **proposal for the owner. Nothing in this file is implemented.** Phase 1 (review 40, top-10 item 8) changed
only the encoder, kept the format, and writes frames that 0.0.1 and 0.0.2 read; its results are in
[benchmark/small/RESULTS.md](../../benchmark/small/RESULTS.md). This file covers the parts that need a new
identifier. Each one has its measured or modelled gain and a proposed identifier that FORMAT.md does not use today.
Claims: #90 (compact frame), #89 and #4 (small-block entropy), #173 (benchmark), then #172, #15/#56 and #91
(session, priming, dictionaries).

All figures come from `benchmark/small` (Apple M5, one thread) on six data kinds, with items of exactly 64 B to
64 KiB: JSON messages (GH Archive events), log lines (NASA HTTP), CSV rows (GHCN), KV values (osdb database rows),
code (samba), binary (ooffice). Each kind and size has up to 256 items, all coded independently. Kolmo is this
branch (phase 1). Rivals are zstd 1.5.7 (default frame), Brotli 1.2.1 and LZ4 1.11.1.

## 1. Where Kolmo stands after phase 1

Output over items, Kolmo against zstd at its comparable level (below 1.000 means Kolmo is smaller):

| kind | 64 B | 256 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|---|
| level 3 / zstd -3, json | 1.589 | 1.578 | 1.298 | 1.153 | 1.061 | 1.025 |
| level 3 / zstd -3, log | 1.589 | 1.541 | 1.382 | 1.184 | 1.060 | 1.010 |
| level 3 / zstd -3, csv | 1.590 | 1.677 | 1.643 | 1.493 | 1.235 | 0.790 |
| level 3 / zstd -3, kv | 1.589 | 1.183 | 1.160 | 1.069 | 1.041 | 1.056 |
| level 3 / zstd -3, code | 1.701 | 1.535 | 1.278 | 1.125 | 1.032 | 0.989 |
| level 3 / zstd -3, binary | 1.661 | 1.304 | 1.219 | 1.007 | 0.944 | 0.890 |
| level 9 / zstd -19, json | 1.587 | 1.285 | 1.117 | 1.043 | 0.999 | 1.001 |
| level 9 / zstd -19, log | 1.619 | 1.339 | 1.154 | 1.049 | 0.963 | 0.888 |
| level 9 / zstd -19, csv | 1.719 | 1.344 | 1.260 | 1.154 | 1.244 | 0.899 |
| level 9 / zstd -19, kv | 1.589 | 1.129 | 1.018 | 0.993 | 0.966 | 0.939 |
| level 9 / zstd -19, code | 1.688 | 1.316 | 1.132 | 1.042 | 0.983 | 0.941 |
| level 9 / zstd -19, binary | 1.604 | 1.184 | 1.051 | 0.922 | 0.878 | 0.847 |

There are two causes, measured separately.

1. **The container.** Each item carries 52 bytes: a 32-byte header, a 16-byte index entry and a 4-byte plan prefix.
   At level 3 that is 45% of the output at 64 B, 17-25% at 256 B, 5-19% at 1 KiB and 1.6-11% at 4 KiB
   (`frame_bytes` in the results). zstd needs 9-13 bytes, the LZ4 frame 11-19 and Brotli 1-6.
2. **The entropy stage of KLZH2 (levels 2-7).** Each stream sends its own Huffman table. Short code streams come out
   as stored bytes, at 8-9 bits per code (R21 F2: on NASA 1 KiB chunks, 12.6% of the output is khuf tables and 23.5%
   is stored chunks). This is why level 3 stays 12-40% above zstd -3 at 1-4 KiB even without the container.

Phase 1 already removed the encoder-side losses: the 32-byte empty frame, tables sized to the block, and KLZMA for
small max blocks. Level 8 got 1-33% smaller and level 9 up to 30% smaller below 64 KiB. Every other setting writes
the same bytes as before.

## 2. P1: compact frame `CZM1` (claim #90)

**Layout** (a one-block frame for inputs below 2^32 bytes):

    "CZM1" | flags u8 | orig uv | codec u8 | tflag u8 | [plan 4] | [dict id uv] | [check u32] | payload

- `flags`:
  - bit 0: check present;
  - bit 1: plan present (absent means plan 0,0,0,0);
  - bit 2: dict id present (reserved; refused until dictionaries exist);
  - bits 4-7: the mode (0-5, as CZN1 byte 5);
  - bit 3: reserved, must be 0.
- `check`: the low 32 bits of `kolmo_khash64(decoded, seed)`. It is the frame's only checksum, replacing CZN1's
  header CRC-32C and block checksum. The payload length is the rest of the buffer.
- `memd`: not stored. A one-block frame's decode working set follows from `codec` and `orig`
  (`blk_dec_floor`/`kmem_declared` today).
- The container is 4 + 1 + 1-5 + 2 + 4 = **12-16 bytes**, without a plan or dictionary id. It is 13 bytes for an
  item under 16 KiB, against 52 today.

**When it is written.** At first only on request: `--small-frame`, a `kolmo_params` flag, and the headerless API
below. Only for one-block outputs, and only when `orig` is at most 64 KiB (or the block size). CZN1 stays the
default until 0.0.x decoders that read CZM1 are common. A second step, for the owner to decide, would make it the
default below 64 KiB.

**Headerless API** (zstd's magicless frames serve the same users: KV stores and message buses that keep the length
themselves):

    int64_t kolmo_compress_headerless(src, n, dst, cap, params);   /* codec | tflag | [plan] | payload */
    int64_t kolmo_decompress_headerless(src, n, dst, orig, threads);

**Measured gain.** Every item in the benchmark is a one-block frame. The change in output when the 52-byte container
of each item is replaced by 13 bytes (this branch's frames):

| setting | 64 B | 256 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|---|
| level 3, json | -33.6% | -13.7% | -6.6% | -2.9% | -1.1% | -0.4% |
| level 3, log | -33.6% | -15.8% | -8.3% | -3.8% | -1.5% | -0.5% |
| level 3, csv | -33.6% | -18.6% | -14.1% | -8.5% | -3.8% | -1.7% |
| level 3, kv | -33.6% | -12.7% | -3.8% | -1.2% | -0.4% | -0.1% |
| level 3, code | -34.1% | -15.3% | -6.6% | -2.4% | -0.8% | -0.2% |
| level 3, binary | -33.9% | -13.5% | -4.5% | -1.6% | -0.5% | -0.1% |
| level 9, json | -33.9% | -17.2% | -7.7% | -3.3% | -1.2% | -0.4% |
| level 9, log | -33.6% | -18.7% | -10.2% | -4.7% | -1.9% | -0.7% |
| level 9, csv | -34.9% | -23.9% | -19.5% | -12.9% | -5.2% | -2.3% |
| level 9, kv | -33.6% | -13.5% | -4.5% | -1.4% | -0.5% | -0.2% |
| level 9, code | -35.6% | -18.4% | -7.7% | -2.7% | -0.9% | -0.3% |
| level 9, binary | -35.3% | -15.6% | -5.6% | -1.8% | -0.5% | -0.1% |

The same frames against zstd (below 1.000 means Kolmo with CZM1 would be smaller):

| setting | 64 B | 256 B | 1 KiB | 4 KiB | 16 KiB | 64 KiB |
|---|---|---|---|---|---|---|
| level 3 + CZM1 / zstd -3, json | 1.055 | 1.363 | 1.212 | 1.120 | 1.050 | 1.021 |
| level 3 + CZM1 / zstd -3, log | 1.055 | 1.297 | 1.268 | 1.139 | 1.044 | 1.005 |
| level 3 + CZM1 / zstd -3, csv | 1.055 | 1.365 | 1.412 | 1.366 | 1.187 | 0.776 |
| level 3 + CZM1 / zstd -3, kv | 1.055 | 1.033 | 1.115 | 1.056 | 1.037 | 1.055 |
| level 3 + CZM1 / zstd -3, code | 1.121 | 1.301 | 1.194 | 1.098 | 1.024 | 0.987 |
| level 3 + CZM1 / zstd -3, binary | 1.098 | 1.128 | 1.164 | 0.991 | 0.939 | 0.889 |
| level 9 + CZM1 / zstd -19, json | 1.048 | 1.064 | 1.031 | 1.009 | 0.987 | 0.997 |
| level 9 + CZM1 / zstd -19, log | 1.074 | 1.089 | 1.036 | 1.000 | 0.945 | 0.882 |
| level 9 + CZM1 / zstd -19, csv | 1.118 | 1.023 | 1.014 | 1.005 | 1.180 | 0.878 |
| level 9 + CZM1 / zstd -19, kv | 1.055 | 0.976 | 0.973 | 0.980 | 0.962 | 0.938 |
| level 9 + CZM1 / zstd -19, code | 1.087 | 1.075 | 1.044 | 1.013 | 0.974 | 0.938 |
| level 9 + CZM1 / zstd -19, binary | 1.038 | 0.999 | 0.993 | 0.905 | 0.873 | 0.846 |

What the tables show:
- At 64 B every item is stored, so the frame is the whole difference: 116 bytes (52 + 64) become 77, and zstd
  writes 73.
- With CZM1 alone, level 9 is within 4-12% of zstd -19 at 64 B, within 9% at 256 B and 1 KiB, and within 1.3% from
  4 KiB, CSV at 16 KiB (+18%) aside.
- Level 3 stays 12-41% above zstd -3 at 256 B-4 KiB on every kind but KV and binary. That is section 3's gap.

**Proposed identifiers:**
- frame magic `CZM1`. It is not in FORMAT.md, and no claim uses it. 0.0.1 and 0.0.2 refuse it ("corrupt or
  unsupported input", exit 1, no output written; checked on this branch).
- the headerless format: no identifier; the caller holds the context.

**Cost and risk:**
- a second frame kind for the detection paths: `-l`, `-t`, `-dcf`, `--range`, the stream wrapper (a CZNS segment
  may hold either kind) and the archive readers;
- FORMAT.md §4b, `tests/format_reference.py`, and a fuzz target with seeds;
- effort about S-M (R21's estimate).

## 3. P2: a small-block entropy stage (claims #89, #4)

**Evidence:**
- R21 (`review40b/reviews/R21.md` F2, model over the real streams extracted with the reference decoder): an adaptive
  order-0 coder with no table sent codes KLZH2's four streams in 15.6% (GH events) to 36.6% (GHCN 4 KiB rows) fewer
  bytes than today. Static default tables do better in their own domain, but they are **+28.7%** across domains
  (GHCN tables on NASA logs).
- R01's tANS prototype with predefined small-chunk tables (assess R01) measured +0.79% against zstd -1 at 4 KiB on
  xml, dickens and osdb. Main measures +5.02% there.
- Phase 1 (this branch, section 1): level 3 is 12-40% above zstd -3 at 1-4 KiB on text kinds, with 4-19% of that
  from the container.

**Projected** (R21's model of the adaptive streams plus a compact frame, with a 4-byte coder flush; main 52df50cf,
R21's held-out items). R21 measured level 3 today at 1.38x zstd -3 on NASA 1 KiB chunks and 1.49x on GHCN 4 KiB rows
(18,175 against 12,159 B):

| set | projected | zstd -3, same items |
|---|---|---|
| NASA 1 KiB | about 17,200 B (parity) | 17,026 B |
| GH events | about 31,700 B (parity) | 31,472 B |
| GHCN 4 KiB | about 11,570 B (5% smaller) | 12,159 B |

These are projections from a model, not a built coder. The first step of #89 is a prototype measured on
`benchmark/small`.

**Proposed identifiers, in order of preference** (SYNTHESIS §5.3: prefer a stream mode if the two tie):
1. **KLZH2 stream mode 2**: an adaptive order-0 multi-symbol range coder, with no table sent, for a stream of
   `count` symbols. Codec 32's mode byte exists already, and decoders refuse `mode > 1` (FORMAT.md §18: "the mode
   byte is missing or above 1"), so 0.0.2 refuses such frames cleanly. The encoder would pick it per stream when it
   is smaller, so large blocks are unchanged. This is the smallest port: the parse and stream layout stay as they are.
2. **Codec 42 (KLZS)**: the KLZH2 parse with one adaptive coder over the four interleaved streams and the parser
   priced from it (R21's design). It is larger, and it is only worth it if mode 2 cannot reach the gates below.
   Codec 42 is the id R21 registered for #89 (SYNTHESIS §5.9).

**Acceptance gates** (SYNTHESIS §5.3):
- decode at least 300 MB/s on 4 KiB items;
- no regression across domains on the six kinds of `benchmark/small`;
- level order kept (level 3 not larger than level 1 at any size; today GHCN rows invert, R21 bug 2).

## 4. Later steps, for ordering only

R35's measured order (SYNTHESIS §5.4) is the compact frame and the entropy stage, then:
- a session mode with history (#172, magic `CZMS` proposed by R35);
- prefix and CM priming (#15/#56);
- trained dictionaries (#91; a CZN1/CZM1 flags bit that 0.0.2 refuses, since bits 1-2 are refused today).

Each step is judged on `benchmark/small` (extended with #173's held-out train/test splits) against raw history, not
against "no dictionary". None of these steps is proposed for implementation here.

## 5. Decisions for the owner

1. Accept `CZM1` as the compact frame magic, and decide whether it stays opt-in or becomes the default below 64 KiB
   once readers exist.
2. Accept the headerless API (`kolmo_compress_headerless` / `kolmo_decompress_headerless`).
3. Allocate KLZH2 stream mode 2 for the small-block entropy stage, with codec 42 held as the fallback, and approve a
   prototype against the gates in section 3.
