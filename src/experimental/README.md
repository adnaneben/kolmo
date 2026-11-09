# Independent codec and Metal experiment

KRC is an independently implemented research codec. It uses established integer prediction, bit packing, sparse exceptions and CRC32C; this is not a claim to have invented those techniques. It contains no third-party compression backend. Normal Kolmo does not select KRC, link its GPU bridge, or write its format.

## Build and use

Run these commands from the repository root:

```sh
make build/krc experimental-check
./build/krc -T 10 input.bin output.krc
./build/krc -d output.krc restored.bin

# macOS with an accessible Metal device
make build/krc-metal build/krcgputest
./build/krc-metal -b hybrid -T 10 -G input.bin output.krc
./build/krcgputest src/experimental/krc_simd.metal
```

`-T 0` detects online logical CPUs, capped at 256; small jobs start fewer helpers. `-b metal` plans on the GPU and encodes on the CPU. `-b hybrid` divides tiles equally between CPU and GPU planning, then encodes on the CPU. Its thread budget includes the GPU submission helper. This fixed split is experimental, not automatic hardware tuning. A failed GPU operation falls back to CPU unless `-G` requires GPU success. GPU-produced frames decode with the CPU-only tool. `-s path` selects a shader; the default path is relative to the repository root. `-M N` limits original/decoded size to N MiB (default 1024). The CLI reads the whole input into memory and verifies every encoded output before writing it.

CPU implementation requires C11 and POSIX threads. CPU tests currently run locally on Apple Silicon; CI includes macOS and Linux. GPU testing is limited to this Apple M5. Other Apple GPUs and non-Apple platforms are not validated. The optimized kernel requires 32-lane SIMD groups. CUDA, Vulkan and Windows backends are not implemented.

## Format version 1

All multi-byte fields are little endian. The 32-byte header is:

| Offset | Bytes | Meaning |
|---|---:|---|
| 0 | 4 | `KRC1` |
| 4 | 4 | Version 1, flags 0, tile log2 words 8, reserved 0 |
| 8 | 8 | Original byte count |
| 16 | 4 | Tile count |
| 20 | 4 | Original data CRC32C |
| 24 | 4 | Encoded payload CRC32C |
| 28 | 4 | CRC32C of preceding 28 header bytes |

Complete 32-bit words form independent tiles of up to 256 words; the final zero to three bytes are stored verbatim after all tiles. Each tile begins with six bytes: mode, word stride, bit width, reserved zero, and a 16-bit exception count. Mode 0 stores raw words with stride/width/exceptions zero. Mode 1 packs unsigned words directly (stride 1). Modes 2, 3 and 4 use modular delta with zigzag, second difference with zigzag, and XOR respectively, with strides 1, 2, 4, 8, 16 or 32. Missing predecessor words are zero, and arithmetic wraps modulo 2^32.

The planner chooses the cheapest payload among raw and 19 prediction candidates. Predicted payloads contain packed low bits followed by exceptions in ascending word-index order. An exception stores its one-byte index and full four-byte residual. Packed bits are least-significant first. Candidate cost is `ceil(words * width / 8) + 5 * exceptions`. Strictly smaller costs win; ties keep the first candidate/width, with raw considered first. A coded payload must be smaller than its raw equivalent. CPU and GPU use the same choices and produce identical frames.

CRC32C uses the reflected Castagnoli polynomial `0x82f63b78`, initial/final complement; `123456789` checks to `e3069283`. Decoding validates headers, bounds, exceptions and checksums. Checksums detect corruption; they do not authenticate data. Version 1 is experimental and is not a public compatibility commitment.

## Execution and memory

CPU planning uses an atomic queue of eight-tile batches. Thread creation failure leaves the caller and successfully started helpers to finish the queue. Encoding and decoding are currently serial. The original Metal kernel (`krc.metal`) is preserved for reproduction. The optimized kernel (`krc_simd.metal`) distributes each candidate across SIMD lanes and evaluates only widths that can change exception counts; a second dispatch reduces candidate costs with stable ties. One command buffer orders both dispatches.

The public planning API accepts caller-owned workspace. `krc_compress` and `krc_gpu_compress` allocate it themselves. A GPU context retains buffers and must have only one concurrent caller. CPU fallback is explicit in the CLI; the low-level GPU API returns errors.

For N input bytes and T tiles, output capacity is bounded by `N + 32 + 6*T`; CPU plans take `16*T` bytes. Decoding uses bounded tile workspace and no heap allocation internally. GPU-only SIMD planning additionally requests shared input N, shared plans `16*T`, and private candidate scratch `320*T` bytes. Hybrid planning allocates those GPU buffers only for its GPU partition. Contexts retain the largest buffers used until destruction. Driver/compiler allocations are additional. CLI process peak RSS and requested Metal buffer bytes overlap and must not be added together or treated as a complete GPU peak measurement.

## Reproducibility and promotion

The executive plan and measurements live in `benchmark/next`. Run `python3 benchmark/next/protect_gains.py --sources-only` for checkpoint integrity, and `make experimental-check` for CPU correctness plus address/undefined-behavior sanitizers. Metal tests compare both plans and complete frames with the CPU, including hybrid splits. Benchmark output always includes every metadata/checksum byte and verifies decoded contents.

This prototype is not promoted into normal Kolmo: its weak ratio on general files outweighs the currently observed planning speed benefits. New encoding ideas must preserve these experimental results and pass the protected production gates before promotion.
