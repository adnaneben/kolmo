# Held-out policy evaluation on Apple M5

## Decision

Keep direct stock CLI dispatch for this executable bucket. No development option produced the required 2% byte saving on either GNU tar or LZ4. All four options tied the stored byte count on both files. The fixed policy therefore had zero routes before it saw the Tukaani XZ and libjpeg-turbo holdouts.

The published wrapper is opt-in. Even an empty policy introduces a Python startup cost. Bypassing it when the accepted table is empty recovers the direct CLI performance. A native or persistent router is a separate experiment. The evidence measures one host, level, thread count, file family and size bucket. The absolute times are a concurrent-host pilot; small decode differences are inconclusive.

| Holdout | Input B | Frame B | Path | Encode ms | Decode ms | Combined MB/s | Peak encode MiB | Peak decode MiB |
|---|---:|---:|---|---:|---:|---:|---:|---:|
| tukaani-xz | 114,336 | 48,424 | Stock CLI | 2.646 | 2.075 | 23.84 | 2.92 | 2.09 |
| tukaani-xz | 114,336 | 48,424 | Policy fallback | 36.280 | 2.298 | 2.97 | 17.59 | 2.09 |
| libjpeg-turbo | 166,976 | 90,792 | Stock CLI | 2.951 | 2.537 | 30.33 | 3.03 | 2.20 |
| libjpeg-turbo | 166,976 | 90,792 | Policy fallback | 36.787 | 2.301 | 4.24 | 17.64 | 2.20 |

Seven shuffled repetitions per input and configuration. Median combined throughput uses complete input bytes divided by the median observed encode-plus-decode time, in decimal MB/s. The full development and holdout run made 98 exact encode/decode round trips (70 development, 28 holdout). Each raw receipt has input and frame SHA-256, per-phase medians and individual timings and RSS. Ordinary v0.0.2 decoded every frame. The independent projects share a Homebrew macOS toolchain. Results do not cover another distribution or ISA.

## Reproduction

Use the versions below and verify the SHA-256 values from development.json and holdout.json when reconstructing the inputs. Place the four unmodified executable files next to copies of the two manifests under names gtar, lz4, xz and djpeg. Keep that directory separate from the source checkout. The experiment did not distribute the binaries.

| Split | Source and version | Bytes |
|---|---|---:|
| Development | GNU tar Homebrew 1.35_1 | 456,768 |
| Development | LZ4 1.11.1 | 285,824 |
| Holdout | Tukaani XZ 5.8.4 | 114,336 |
| Holdout | libjpeg-turbo 3.2.1 djpeg | 166,976 |

The codec source is pinned at a1d39559686a9ec17c8614b9154a64d7b5b98672. Build with `make build/kolmo`. Follow the seal, learn and validate commands in docs/ENCODER-POLICY.md using this directory's config and manifests. Write outputs to a fresh directory. The draft and accepted files record the hash of the stock decoder, encoder, catalogue and policy script. Local output will use its own scoped fingerprints. The initial-wait4 directory preserves the pilot that exposed the missing macOS pre-exec RSS. The final receipts record that peak.

Real codec, fallback and staged route regressions passed with:

```sh
KOLMO_TEST_ENCODER="$PWD/build/kolmo" KOLMO_TEST_DECODER=/opt/homebrew/bin/kolmo python3 tests/test_encoder_policy.py
```
