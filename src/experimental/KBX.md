# Branch-isolated independent codec experiment

KBX builds on this project's independently implemented KBC1/KRC1 sources. It is an opt-in experiment and is not selected by normal Kolmo. `make build/kbx kbx-check` builds the CLI and correctness/sanitizer tests. These additions do not change the earlier codec implementations or their formats.

## Profiles and compatibility

| CLI | Purpose | Frame | Size rule |
|---|---|---|---|
| `-l 0` | Compact-counter, buffered-I/O implementation of KBC1 | KBC1, version 1 | Byte-identical to the preserved KBC1 encoder |
| `-l 1` | Tagged longer contexts and byte-normalized interval coding | KBX3, version 3 | Smallest of raw, KRC1 and the new predictor; may lose to KBC1 |
| `-l 2` | Compare both predictors for every block | KBX3, version 3 | Complete frame never exceeds preserved KBC1 or profile 1 in byte count |

`./build/kbx -l 1 -T 10 input output.kbx` encodes and verifies a file. `./build/kbx -d -T 10 output.kbx restored` detects the supported frame automatically. The decoder reads KBC1 and KBX3. It rejects the archived, unpromoted KBX1/KBX2 trial formats instead of treating their different models as the current model. Earlier KBC1 decoders accept profile 0; they report an unknown format for KBX3. The API accepts an explicit profile argument. Output capacity is `kbx_bound(n)`. Inputs and outputs must not overlap.

The CLI reads the entire input, defaults to a 1024 MiB original/decoded limit (`-M` changes it), accepts 0..64 worker budgets (`0` detects online CPUs), and verifies every encoded output before writing. Workers are capped by the number of independent 256 KiB blocks. Failed thread creation leaves the caller and successfully started helpers to drain the queue. Allocation errors return an error; they do not silently change the selected representation. The implementation is CPU-only; the preserved KRC1/Metal tool remains separate.

## Exact-output optimization

Two adaptive counts that never exceed 63 share one `uint16_t`: zero in the low byte, one in the high byte. Updating a bit adds 1 or 256. At a count sum of 64, `((value + 0x0101) >> 1) & 0x7f7f` computes the same rounded half-counts as KBC1. Model arithmetic and tie-breaking remain unchanged in profile 0. The two/three-byte hash bases are cached once per byte. Bit I/O buffers complete bytes rather than storing/reloading an output byte for every bit. The legacy arithmetic decisions and payload bits are preserved.

Profile 0's counter workspace is 2,228,736 bytes instead of 4,457,472. This is a halving of the model counters, not a promise to halve whole-process memory. Compression additionally needs approximately 0.53 MB of candidate scratch per active worker, KRC1 plans, input/output buffers and thread stacks. Raw/KRC-only decoding does not allocate a probability model.

## Ratio model

The new predictor retains the prefix tree and previous-byte tree, and adds hashed contexts for two, three, four and six preceding bytes. Each hashed node has a 19-bit table index and a 13-bit tag derived from the same 32-bit hash. A tag mismatch resets that node's counts before using it. This reduces contamination by different contexts sharing an index. Hash collisions remain possible; they affect prediction quality, not exact decoding, because encoder and decoder update identical state.

The six count pairs use integer weights 1, 2, 4, 8, 16 and 32. Higher-order contexts contribute their observed counts after subtracting the initial (1,1) priors. An unseen higher-order node therefore does not dilute a trained lower-order predictor with a uniform prior. Counts are rescaled at 64 as in the baseline. No pretrained model, external dictionary or filename-specific rule is used.

A stronger-model worker needs 4,325,888 bytes of counters plus 4,194,304 bytes of tags, or 8,520,192 bytes before candidate scratch. Profile 2 pays for both candidate encodings to guarantee its size rule. That extra work can make encoding substantially slower even when the selected output is unchanged. Encoder workspace is reused within a call and reset at every block; it is not shared between callers.

Mode 3 uses an inclusive unsigned 32-bit interval. With zero probability P/4096, `split = low + floor((high-low)*P/4096)`. A zero selects `[low, split]`; a one selects `[split+1, high]`. Common leading bytes are emitted and the interval is shifted. A point interval emits four bytes and expands before another symbol is coded, so neither branch can create an empty interval. Four final bytes identify the final interval. The decoder uses the same equations and refuses to read beyond the payload. Byte normalization trades some finite precision for less per-bit I/O and normalization overhead. Mode 2 remains the original KBC1 bit coder.

## KBX3 frame

The 32-byte header and 12-byte-per-block directory retain KBC1's field layout and full CRC32C accounting. The magic is `KBX3` and version byte is 3. Block log2 is 18; reserved fields are zero. Modes 0, 1 and 2 mean raw, complete KRC1 frame and original KBC1 arithmetic payload. Mode 3 means the tagged six-context byte-normalized model. New model parameters are fixed by this version, so no external state or omitted weights are required.

The compressed bound remains `n + 32 + 12*ceil(n/262144)`. Profile 2 starts from the same raw/KRC/baseline candidates and only replaces a payload with a strictly smaller new one. The header/directory lengths are unchanged, which establishes its complete-frame size rule. Headers, directory, payload and restored data are validated before success. Checksums detect corruption; they do not authenticate a frame. Experimental versions are not a public compatibility commitment.

## Evidence and provenance

`kbx-check` checks 73 cases across all three profiles, exact old frames, old/new decoding, block boundaries, guarded output, thread agreement, truncation, and corruption with repaired outer checksums; it runs normally and under ASan/UBSan. Corpus timing lives under `benchmark/evolve`, using real lzbench callbacks with allocations, initialization and complete encoded payloads counted. Rejected trials have their own source archives and raw CSVs. Final validation uses full enwik8 frozen before new performance evaluation.

The mathematical primitives are established techniques, not inventions claimed here. The known lowbias32 hash is credited in `PROVENANCE.md`. The six-byte folding multiplier is a conventional integer mixing constant. Tag checks, weighted counts and interval coding were implemented in this experiment without importing competing compression implementations. The fact that the source is independent does not establish a novel algorithm or a public leaderboard position.
