# Rank-one arrays and Arrow IPC compatibility

We tested the combined encoder at ac10ce80048ab0664074f7ba031634055f4d2225 against main a1d39559686a9ec17c8614b9154a64d7b5b98672. All 32 input/preset pairs produced identical frames on the two encoders. All 256 timed restores reproduced the complete input bytes, including headers, padding, Arrow validity bitmaps and NaN payloads. The v0.0.2 reader at 550c02fd89d00b767ca540b117301c185dbbb6c9 independently restored both encoders’ 64 warmup frames.

The array values came from two independent upstream sources. The first 32,768 float32 values of the 2023 NCEP air grid and first 32,768 signed samples of an ESC-50 recording were written as rank-one NPY in both byte orders. Each producer also provided one nullable Arrow column with an integer row column. These six containers were created for this test. A separately generated NPY carries literal positive and negative zero, signed NaNs with distinct payloads, infinities and signed subnormals. The eighth file is an untouched 594-byte LZ4 Feather fixture shipped by PyArrow. NumPy and Arrow reopened every result. The original NCEP and ESC-50 parent files were hashed, while the measured bytes are the smaller derived containers.

| Input | Input bytes | L1 stored bytes | L3 | L7 | Ultra |
|---|---:|---:|---:|---:|---:|
| climate-le.npy | 131,200 | 66,640 | 39,561 | 37,633 | 22,934 |
| climate-be.npy | 131,200 | 65,122 | 48,745 | 41,872 | 22,921 |
| audio-le.npy | 65,664 | 65,716 | 46,787 | 46,782 | 46,516 |
| audio-be.npy | 65,664 | 65,716 | 60,600 | 51,690 | 45,540 |
| bits-le.npy | 131,200 | 697 | 194 | 207 | 217 |
| climate.arrow | 266,858 | 83,665 | 41,083 | 38,816 | 23,518 |
| audio.arrow | 201,322 | 67,145 | 54,101 | 54,632 | 51,063 |
| published.feather | 594 | 432 | 439 | 432 | 264 |

On the little-endian climate array, L3 stored 39,561 bytes at 8.29 MB/s combined and Ultra stored 22,934 at 0.29 MB/s. On the little-endian synthetic bit-pattern array L3 stored 194 bytes and Ultra 217. The larger level is not always the smallest for a particular small input. Keep the existing stream semantics. A new array transform would require a measurable advantage against these actual frames across more producers.

Each cell used one warmup plus three measured repetitions in shuffled cell and arm order, one requested thread, and the complete file-to-file CLI. Timings are warm-cache measurements on a shared M5. The exact dtype, shape or Arrow schema, parent and input SHA-256s, and binary digests are in [manifest.json](manifest.json). [Raw timings and restored hashes](raw.csv) · [Per-preset phases](score.csv) · [Replay](replay.py)
