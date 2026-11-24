> **DRAFT. A proposal for the top of README.md at launch. README.md itself is not changed.**
>
> It replaces everything above "## How it works" in the current README. The current "Results" table (21 held-out
> files) moves to the benchmark page or further down the README.
>
> Before it goes in:
> 1. **The benchmark page goes live** (PR #29; GitHub Pages is not enabled). Replace `BENCHMARK_PAGE_URL`.
> 2. **PR #33 is merged.** The claims below come from the 0.0.2 market report (Apple M5) and the x86-64 check in #33
>    (Intel Xeon 6975P-C), both Silesia, lzbench 2.4.1, one thread. Kolmo's sizes are identical on the two machines;
>    the speed factors are smaller on the Xeon, so both are stated.
> 3. **The release contains the long-field table fix** (PR #22, merged, and PR #30), and `[RELEASE]` below names it.
>
> The version, the install lines and the format link must match the release the launch goes with.

---

# Kolmo

**Smaller than xz, decodes like zstd, and detects the structure of your data by itself.**

Kolmo is a lossless compressor that looks at what your data is (tables, logs, arrays, images, executables,
archives) and rewrites it so its structure is explicit before coding it. There is no schema, profile or training
step: `kolmo FILE` decides block by block.

```sh
brew install adnaneben/kolmo/kolmo     # command line
pip install kolmoc                     # Python (import kolmo)
cargo add kolmo                        # Rust
```

```sh
kolmo data.csv              # writes data.csv.kolmo (level 3, the default)
kolmo -6 logs.tar           # smaller; still decodes at LZ speed
kolmo -d data.csv.kolmo     # restores data.csv
kolmo -l data.csv.kolmo     # what was detected, and the coders used
```

## Why use it

Measured on the Silesia corpus, with every codec inside lzbench on one thread, on an x86-64 Xeon and an Apple M5
(method and raw results: [benchmark page](BENCHMARK_PAGE_URL)):

| | Kolmo | Compared with | Kolmo is faster by (Xeon / M5) |
|---|---|---|---|
| **Archives you read back** | `-6`: ratio 4.367 | `xz -9`: 4.346 | **decode 6.5x / 8.6x** |
| **zstd -19's ratio, faster** | `-4`: 3.949 | `zstd -19`: 4.012 | **compress 11x / 13x** |
| **Highest ratio at practical speed** | `-9`: 5.232 | `zpaq -5`: 5.237 | **decode 87x / 120x** |
| **Structured data** | default level on CSV, logs, pickles, integer columns | `zstd -3` | **1.4-3.4x smaller**; up to 4x at higher levels |

These claims are about Silesia. On plain English text (enwik8) and on small files (Canterbury) levels 4 and 6 do not
reach `zstd -19` or `xz -9`.

Where the data has structure, the default level beats even the slowest general tools. Silesia's star catalog is
2.933 at `-3` against 1.643 for `xz -9`, and its x-ray image is 2.305 against 1.887.

**Where it is not the right tool:**

- If you need the fastest single-core decode, use zstd. On one core Kolmo's levels 2-7 decode at 60-75% of zstd's
  speed. Kolmo's blocks decode in parallel and pull ahead on many cores.
- On plain text and small files, Kolmo is about as good as zstd at the low levels, not better.
- On new CSV, JSON and log data, the default level's ratio is 9-35% below `zstd -19`'s, and on CSV a *trained*
  OpenZL profile beats every Kolmo level from 3 to 9 ([benchmark page](BENCHMARK_PAGE_URL)).
- Model weights: no tool we measured, Kolmo included, does much better than about 1.2x (f32) or 1.5x (bf16).
- Memory: level 9 uses about 1.4 GB to compress.

## Levels

One scale: `-1` (fastest) to `-9` (max), with `-3` the default. Levels 1-7 all decode at LZ speed, so going up costs
compression time only. Levels 8 and 9 use block sorting: smaller, but about 10x slower to decode. `-U` (ultra) is
context mixing, for cold archives. The full contract is in [docs/LEVELS.md](docs/LEVELS.md).

## Your data stays readable

- The format (container version 12) is specified in [FORMAT.md](FORMAT.md).
- [RELEASE] reads every frame written since 0.0.1, to the same bytes, and so will every later release
  ([COMPATIBILITY.md](COMPATIBILITY.md)). CI checks this against frames frozen from every release.
- **If you used 0.0.1 or 0.0.2:** they wrote table blocks with a field over 64 KiB (CSV, or a log line over 64 KiB)
  that they cannot read back. The data is intact: [RELEASE] reads those files, and nothing needs recompressing.
  Check older files with `kolmo -t` before deleting their originals.
- Every block carries a checksum, so corrupt input is reported, never returned as data.
- x86-64 and ARM64 builds write identical files at any thread count.
- The decoders are fuzzed under ASan/UBSan in CI.

Version 0.0.x means the command line and the API may still change between releases. The compressed format will not.

## How it works

A short version is below. The long version is [How Kolmo finds the structure in your data](docs/launch/structure-detection.md).

<!-- the current README's "How it works" table continues here -->
