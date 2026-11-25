# Python and Rust native mirrors on PR 107

Mirror-only commit `d2881dba41b3dadc1a5fab8bb8e41750fad18471` follows `31fd3acd922490143059ac2438e988e38d89be5f`. Both official sync scripts changed only their copies of `src/core/blocks.c` and `src/core/compress.c`. Running both scripts again left the checkout clean. Each binding tracks 176 eligible native files. Every path and byte matched across all 352 comparisons. The actual Python source distribution and Rust crate also each contained all 176 correct native files. [Before](before.json) · [After](after.json) · [Checker](mirrors.py) · [Distribution and library SHA-256s](packages.json)

On Darwin ARM64, Apple Clang 21, Python 3.12.8 and Rust 1.99.1, the default and experimental builds of each binding passed their existing public suites. Python loaded an installed wheel in a fresh process. Rust ran the unpacked crate's nine unit tests, two integration tests and one documentation test. Each suite used a separate default CLI build for interoperability. [Python default](stock-python.log) · [Python experimental](opt-python.log) · [Rust default](stock-rust.log) · [Rust experimental](opt-rust.log)

## Optional feature and a negative control

The Rust crate's cc-rs accepts `CFLAGS=-DKOLMO_FASTQ_RECORD_BLOCKS`. The Python wheel builder accepts a `CC` executable. Its compile command constructs its own flag list. The experimental wheel used a compiler wrapper that executed `clang -DKOLMO_FASTQ_RECORD_BLOCKS "$@"`. Neither package exposes a dedicated public feature switch. The native binaries in both experimental builds contained the runtime switch string. The two default binaries omitted it. The public entry points demonstrably changed their block cuts only with the experimental build and runtime switch enabled.

The input repeats a 73-record tile of deterministic punctuation and opaque printable characters to 8,437,837 bytes. Its SHA-256 is `96e96a791aa0d9b5e267d6847f45abd92ab82ff2010af8edfea9d69ac4803351`. Python and Rust used Ultra, one encoder thread and a 256 MiB memory cap. Both packages selected an 8 MiB block target. Each public decoder restored its own frame. An independent stock CLI at two decoder threads restored all eight frames. [Fixture generator](input.py) · [Python consumer](consumer.py) · [Rust consumer](consumer.rs) · [All eight measured rows](matrix.csv) · [Verification](results.py)

| Build and runtime | First cut (bytes) | First codec | First compressed block | Whole frame |
| --- | ---: | --- | ---: | ---: |
| Default, runtime unset or 1 | 8,388,608 | 18 (BWT) | 15,476 B | 29,050 B |
| Experimental, runtime unset | 8,388,608 | 18 (BWT) | 15,476 B | 29,050 B |
| Experimental, runtime 1 | 8,388,471 | 16 (FQ family) | 1,837,575 B | 1,851,143 B |

The two packages emitted identical bytes under every matching setting. Moving the first cut 137 bytes produced a frame 63.72 times the control size on this repeated input. Both frames contained two blocks. Keep the option experimental until a repair is verified against repeated inputs and positive controls. These measurements qualify package parity and size on this one input. They do not measure speed. The corpus generator and both consumers can reproduce the regression without downloading data.
