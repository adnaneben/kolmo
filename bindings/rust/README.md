# kolmo (Rust binding)

Safe Rust wrappers over the [kolmo](https://github.com/adnaneben/kolmo) native core. The
build compiles and statically links the C core, so there is no runtime shared-library
dependency (a C toolchain and, by default, `liblzma` are needed at build time).

```rust
let blob = kolmo::compress(data, kolmo::Mode::Hc, 0)?;  // 0 threads = all cores
let back = kolmo::decompress(&blob, 0)?;
let part = kolmo::read_range(&blob, 1_000_000, 4096, 0)?; // decode only the overlapping blocks
```

Modes: `Rt` (LZ4-speed) · `Fast` · `Eco` · `Hc` · `Max` · `Ultra` (archive tier, best ratio).

Build without liblzma (the `max`/`ultra` modes then use the other coders):

```sh
cargo build --features no-lzma
```

Apache-2.0.
