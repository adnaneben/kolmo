# Actual inference KV pages on CPU

Stock Kolmo L3 produced the smallest indexed page frames at each tested token count for one real Devstral Small 2 24B inference. On six 16-token pages, Kolmo L3 used 157,130 bytes and zstd -3 used 171,722 bytes, so Kolmo used 8.5% fewer bytes. Combined file-command speeds were 3.35 and 3.79 MB/s respectively. The eight-token Kolmo L1 frames exceeded their raw page size by 0.4% after overhead. This screen supports measuring the existing L3 on more real caches. It does not qualify a specialized KV codec or a serving default.

| Tokens per frame | Raw bytes, six pages | L1 indexed | L3 indexed | zstd -1 indexed | zstd -3 indexed | L3 combined MB/s | zstd -3 combined MB/s | L3 storage bytes read / 8-token raw bytes |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 98,304 | 98,712 | 86,345 | 92,235 | 92,235 | 1.92 | 2.11 | 0.878 |
| 16 | 196,608 | 179,020 | 157,130 | 171,716 | 171,722 | 3.35 | 3.79 | 1.598 |
| 30 | 368,640 | 322,814 | 280,487 | 308,260 | 305,967 | 6.09 | 6.80 | 2.853 |

## Source of the bytes

A clean local llama.cpp server, commit 39febd2ad, ran the installed Devstral Small 2 24B GGUF on CPU with two threads, disabled device, operation and KV offload, float16 K and V, zero temperature, fixed seed 23, and one sampled token. The only user text was a public excerpt of Charles Dickens' *A Tale of Two Cities*, reproduced in inference.json. The independent slot started with zero cached tokens. Model prompt-evaluation time was 26.46 s for 30 tokens. The server saved 30 cache positions, 4,916,692 actual state bytes, SHA-256 40ed1d9d3dabf45df2206b6e1667ee863353b62895bcb488888dcc870635aa76. The model's installed content ID and size are recorded in inference.json. Model weights and raw activations are not included here.

The exact runtime serializer (src/llama-context.cpp and src/llama-kv-cache.cpp at 39febd2ad) writes the sequence context and unpadded K/V rows. The decoder checked 30 distinct positions, one sequence, 40 layers, FP16 type, all row lengths and exact end of file. The GGUF declares 32 query heads, 8 KV heads and a head dimension of 128, yielding 80 real tensors of [30, 8, 128] FP16 values. Each token row contains 2,048 bytes. All 80 tensor hashes are in shape.json. The fixed sample includes K and V separately from layers 0, 20 and 39. Page sizes of 8, 16 and 30 each start at token zero. They overlap and are not independent contexts.

## Measurement

Every compressed size includes an entire independent codec frame. The indexed columns add a modeled 16 bytes per page (64-bit offset and length). A lookup for the first eight tokens reads one such index record and one full stored frame. The rightmost column divides those read bytes by the requested raw bytes. The decoder expands 1, 2 or 3.75 times the requested number of token rows, respectively. Index caching, disk-sector rounding, allocator slack, host/device transfers and GPU attention kernels are outside this measurement.

All 288 scored and warmup file commands recovered the page's original SHA-256. results.csv retains both phase rates and combined speed, plus read amplification for every setting. Each repetition contains six pages per size and codec. Rates are median original bytes divided by total time over three scored repetitions following a warmup. The seeded order changes across codec arms. Launch and normal file I/O dominate the small pages. This measures one CPU model and one short English context, with no GPU or serving-latency claim. The matching SmolLM2 and Qwen2.5 safetensors files were present and opened, but the local review environment lacked their model-specific loader and tokenizer. The installed compatible GGUF enabled a genuine one-family measurement.

Use fresh_slot.py LLAMA_SERVER GGUF NEW_DIRECTORY. Use page_bench.py NEW_DIRECTORY/public-kv.bin PINNED_KOLMO ZSTD OUTPUT_DIRECTORY and summarize.py from the resulting evidence directory. Kolmo used pinned main a1d39559686a9ec17c8614b9154a64d7b5b98672, binary SHA-256 b67bbddb88d78d26313ae9ec11718e4cfb2e546f2aa0fd75faa36ee95ad9106f, -1/-3 with -T1. Zstandard 1.5.7 used --single-thread -1/-3, binary SHA-256 f59db1267be3158941b6a70902ddcea423a294271ded955f0afb8e889b587426. Both tested the original little-endian FP16 bytes directly.
