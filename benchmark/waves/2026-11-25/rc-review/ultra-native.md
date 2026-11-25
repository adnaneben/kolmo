# Ultra physical-block experiment

Ordinary native Ultra already used one stateful codec-7/model-4 block on each tested 256 KiB file. Increasing its 128 MiB target to 256 MiB changed no output size. A single CM4 stream saved bytes versus specifically forcing 64 KiB blocks. The four smaller blocks exposed an independent parallel user option with a different ratio and peak RSS. The capped and metadata workloads below show why the standalone savings cannot be credited to ordinary Ultra+.

## Capped 8.5 MiB real text spans

Both arms use the same 256 MiB encoder admission cap. The automatic target in the CZN1 header is 8 MiB under this cap. The larger-arm target is explicitly 16 MiB. Tests verify source and stock-v12 decode SHA. Each timing is a single full run for that arm; no stable speed or memory win is claimed.

| Source | Requested T | Physical blocks | Frame B, default → larger | Size change | Total s, default → larger | Combined speed change | Peak encoder / decoder MiB, default → larger |
| --- | ---: | --- | ---: | ---: | ---: | ---: | --- |
| enwik | 1 | 8388608 + 524288 → 8912896 | 1,859,362 → 1,834,062 | -1.36% | 22.42 → 21.15 | +5.99% | 215.1 / 195.1 → 239.3 / 186.2 |
| enwik | 4 | 8388608 + 524288 → 8912896 | 1,859,362 → 1,834,062 | -1.36% | 19.84 → 21.39 | -7.26% | 237.8 / 209.7 → 254.8 / 186.2 |
| dickens | 1 | 8388608 + 524288 → 8912896 | 1,867,643 → 1,843,150 | -1.31% | 22.65 → 21.29 | +6.40% | 224.2 / 185.6 → 236.0 / 181.0 |
| dickens | 4 | 8388608 + 524288 → 8912896 | 1,867,643 → 1,843,150 | -1.31% | 21.69 → 23.06 | -5.96% | 240.1 / 199.2 → 254.2 / 181.0 |

## Independent member and region boundaries

| Producer | Requested T | Default/256 MiB bytes | Default/256 MiB blocks | Native codecs |
| --- | ---: | ---: | ---: | --- |
| two-real-members-ustar | 1 | 288,464 / 288,464 | 5 / 5 | 7 7 24 7 7 |
| two-real-members-ustar | 4 | 288,464 / 288,464 | 5 / 5 | 7 7 24 7 7 |
| stock-build-MachO | 1 | 518,466 / 518,466 | 2 / 2 | 7 7 |
| stock-build-MachO | 4 | 518,466 / 518,466 | 2 / 2 | 7 7 |

## Repeated short-block throughput tradeoff

For each source, compare ordinary T4 with four forced 64 KiB blocks at T4. Each independently repeated trial measures a full raw-file encode and decode on the same machine; requested threads remain fixed. Ratios below use paired per-repetition sums of native encode and decode wall time. Byte counts include the complete CZN1 frame. A speed increase with larger output is a point on a tradeoff curve.

| 256 KiB source span | Trials per arm | Stored bytes, ordinary → four | Extra output | Paired combined speed | Ratio × combined speed | Faster trials | Peak RSS encode/decode MiB, ordinary → four |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| enwik-offset-256k | 7 | 64,824 → 73,915 | +14.02% | 2.90× | 2.55× | 7/7 | 56.2 / 26.1 → 104.9 / 78.4 |
| jxl-offset-256k | 7 | 39,176 → 47,068 | +20.14% | 1.81× | 1.51× | 7/7 | 84.5 / 21.3 → 110.5 / 61.9 |
| reymont-offset-256k | 7 | 42,148 → 48,541 | +15.17% | 2.92× | 2.54× | 7/7 | 50.8 / 20.5 → 85.0 / 58.7 |

No default change or cross-architecture claim follows. When a whole file already has one physical block, a different advertised target cannot remove another reset. In a capped text stream the benefit depends on capacity-dependent table size, history and the second block’s fraction of input, and T4 versus a virtually serial code path. Separately evaluate the existing explicit small-block option for a latency-prioritized multi-core policy on a sealed population; report its storage and memory costs and compare the neighboring native levels and rival settings at equal bytes and equal speed.

See PROTOCOL.md, summary.csv for all phase metrics, the per-study JSONL receipts with frame and model headers, and replay.py. None of these tests compares full-corpus PAQ8PX or CMIX.
