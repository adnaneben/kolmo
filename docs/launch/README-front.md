> **DRAFT. A proposal for the top of README.md at launch. README.md itself is not changed.**
>
> It replaces everything above "## How it works" in the current README. The current "Results" table (21 held-out
> files) moves to the benchmark page or further down the README.
>
> Two things must happen before it goes in:
> 1. **The benchmark page goes live.** Replace `BENCHMARK_PAGE_URL`.
> 2. **The x86-64 run confirms the claims.** The numbers below are from the Apple M5 run of the 0.0.2 market report
>    (Silesia, lzbench 2.4.1, one thread). Where the x86 run differs, use its numbers, or state both machines.
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

Measured on the Silesia corpus, with every codec inside lzbench on one thread (method and raw results:
[benchmark page](BENCHMARK_PAGE_URL)):

| | Kolmo | Compared with |
|---|---|---|
| **Archives you read back** | `-6`: ratio 4.367, decodes at 1463 MiB/s | `xz -9`: 4.346, decodes at 169 MiB/s (**8.6x slower**) |
| **zstd -19's ratio, faster** | `-4`: 3.949, compresses at 79 MiB/s | `zstd -19`: 4.012, 6.0 MiB/s (**13x slower**) |
| **Highest ratio at practical speed** | `-9`: 5.232, decodes at 85 MiB/s | `zpaq -5`: 5.237, decodes at 0.7 MiB/s (**120x slower**) |
| **Structured data** | default level on CSV, logs, pickles, integer columns | **1.4-3.4x smaller** than `zstd -3`; up to 4x at higher levels |

Where the data has structure, the default level beats even the slowest general tools. Silesia's star catalog is
2.933 at `-3` against 1.643 for `xz -9`, and its x-ray image is 2.305 against 1.887.

**Where it is not the right tool:**

- If you need the fastest single-core decode, use zstd. It decodes 30-40% faster than Kolmo's levels 2-7 on one
  core. Kolmo's blocks decode in parallel and pull ahead on many cores.
- On plain text and small files, Kolmo is about as good as zstd at the low levels, not better.
- Model weights: no tool we measured, Kolmo included, does much better than about 1.2x (f32) or 1.5x (bf16).
- Memory: level 9 uses about 1.4 GB to compress.

## Levels

One scale: `-1` (fastest) to `-9` (max), with `-3` the default. Levels 1-7 all decode at LZ speed, so going up costs
compression time only. Levels 8 and 9 use block sorting: smaller, but about 10x slower to decode. `-U` (ultra) is
context mixing, for cold archives. The full contract is in [docs/LEVELS.md](docs/LEVELS.md).

## Your data stays readable

- The format (container version 12) is specified in [FORMAT.md](FORMAT.md).
- Every release reads every frame written since 0.0.1, to the same bytes ([COMPATIBILITY.md](COMPATIBILITY.md)). CI
  checks this against frames frozen from every release.
- Every block carries a checksum, so corrupt input is reported, never returned as data.
- x86-64 and ARM64 builds write identical files at any thread count.
- The decoders are fuzzed under ASan/UBSan in CI.

Version 0.0.x means the command line and the API may still change between releases. The compressed format will not.

## How it works

A short version is below. The long version is [How Kolmo finds the structure in your data](docs/launch/structure-detection.md).

<!-- the current README's "How it works" table continues here -->
