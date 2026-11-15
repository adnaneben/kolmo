# Independent byte-context candidate (KBC1)

KBC is an opt-in research format, implemented in `kbc.c` and `kbc.h`. It neither replaces KRC1 nor changes normal Kolmo. It uses no third-party compression implementation. Integer interval arithmetic, context prediction and count rescaling are established techniques; independent source code is not a novelty claim. For background, see Witten, Neal and Cleary, [Arithmetic Coding for Data Compression](https://doi.org/10.1145/214762.214771) (1987). The source was written from the interval equations rather than adapted from that paper's program or an existing codec.

## Use

From the repository root:

```sh
make build/kbc byte-context-check
./build/kbc -T 10 input.bin output.kbc
./build/kbc -d -T 10 output.kbc restored.bin
```

The CLI defaults to an original/decoded size limit of 1024 MiB (`-M` changes it). `-T 0` detects online logical CPUs; the implementation caps at 64 concurrent workers and the number of available blocks. Inputs and outputs must not overlap in the C API. Compression needs `kbc_bound(input_bytes)` output capacity. The CLI verifies every compressed frame by decoding it before writing. It reads the entire input into memory; this is not a streaming interface. Timings exclude I/O, output allocation and verification, while process peak RSS includes them. Failed thread creation leaves the caller and started helpers to finish the shared queue. An allocation failure returns an error rather than silently changing compressed output.

GPU acceleration continues to be available through the preserved KRC1 tool. KBC's byte-context model is CPU-only; it depends on successive decoded bits. The existing Metal predictor cannot accelerate this model unchanged, and no KBC GPU throughput is claimed.

## Representation

Divide input into independent blocks of up to 262144 bytes. Each block resets its probability model and chooses the strictly smallest complete payload among raw bytes, a complete KRC1 frame, and a byte-context arithmetic stream. Ties prefer raw, then KRC1. KRC1's metadata and checksums are counted when it is embedded. Every candidate is actually encoded; this deliberately trades speed for a clear ratio experiment. Context coding stops if it cannot fit within the current best payload capacity.

Each bit is predicted using four binary count pairs: a tree for the current byte prefix, a tree indexed by the previous byte and prefix, and two separate hashed tables indexed by the previous two/three bytes and prefix. Counts start at (1,1), increment for the observed bit, and are rounded up and halved at a total of 64. The four zero/one count pairs are combined with integer weights 1, 2, 4 and 8; the zero probability is quantized to 1/4096. Each context table lives in the encoder/decoder workspace, not the file. There are no external model weights or dictionaries.

The arithmetic interval is an inclusive 32-bit [low,high] range. For zero probability P/4096, `split = low + floor((high-low+1)*P/4096) - 1`. Zero selects [low,split] and one selects [split+1,high]. Renormalization emits common leading bits and tracks middle-half underflow bits. Finalization emits the pending interval decision, pads the final byte, then appends four explicit zero lookahead bytes. Bits are stored most-significant first. Decoder lookahead is bounded by the declared payload; reading beyond it is an error.

## Frame

All multi-byte fields are little endian. The 32-byte header contains:

| Offset | Bytes | Field |
|---|---:|---|
| 0 | 4 | `KBC1` |
| 4 | 4 | version 1, block log2 bytes 18, two reserved zero bytes |
| 8 | 8 | Original bytes |
| 16 | 4 | Block count |
| 20 | 4 | Original-data CRC32C |
| 24 | 4 | CRC32C of directory and all payloads |
| 28 | 4 | Header CRC32C over the preceding 28 bytes |

A directory of 12 bytes per block follows: mode byte, three reserved zero bytes, original length u32, payload length u32. Modes are raw 0, complete KRC1 frame 1, arithmetic stream 2. Raw length must equal original length; coded length must be strictly smaller. Original block lengths are fixed by the global size and block index. Payloads follow contiguously in directory order. Trailing data, inconsistent lengths, reserved fields and unknown modes are rejected. Header and payload checksums are validated before scheduling output work; the original-data checksum is checked after decoding. CRC32C parameters match KRC1. Checksums detect corruption but do not authenticate files. This is an experimental format, not a compatibility promise.

## Memory and parallelism

Worst-case output bound is `input + 32 + 12*ceil(input/262144)`. Compression uses output capacity as temporary fixed block slots and compacts them after workers finish; this avoids another full-input candidate buffer. Each active compression worker needs approximately 4.46 MB of counts and 0.53 MB of candidate scratch, plus bounded KRC1 plans and thread stack. A context-decoding worker needs the count workspace; raw/KRC-only decoding avoids it. Decoder offsets cost one `size_t` per block. All model state is local to a worker, and every block starts from the same initial counts. Frames are independent of worker count.

Single-thread measurements and equal-block parallel measurements belong in separate tables. Increasing thread count cannot compensate for the model's serial dependency within a block, and mixing in a slower model has a substantial speed/decode cost. The earlier fast KRC1 and GPU paths remain separately available and protected.

## Validation and lzbench

`make byte-context-check` tests raw, KRC and arithmetic storage, empty/tiny inputs, block boundaries, multiple blocks, worker-count agreement, output guards, truncation and corruption with repaired outer checksums, normally and under ASan/UBSan. `benchmark/ratio/protect_experiments.py` enforces the earlier KRC/GPU source checkpoint and raw evidence. The existing default Kolmo source/performance gate remains separate.

`benchmark/ratio/prepare_lzbench.py` copies these standalone sources into the pinned local lzbench checkout. Both compression and decompression use the real lzbench callbacks, which include all per-call allocations, model resets, selection, metadata and checksums. The adapter has no input-name checks, corpus-specific models or references to source bytes in the compressed output. Reproduction and measured promotion decisions are recorded under `benchmark/ratio`.

The known `lowbias32` hash primitive and other established building blocks are credited in [PROVENANCE.md](PROVENANCE.md). Independent codec source does not imply original invention of those primitives.
