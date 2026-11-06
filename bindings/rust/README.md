# kolmo (Rust binding)

Safe Rust wrappers over the [kolmo](https://github.com/adnaneben/kolmo) native core. The
build compiles and statically links the C core, so there is no runtime shared-library
dependency. Building needs a C compiler. liblzma is used if it is found and skipped if it isn't.

```sh
cargo add kolmo
```

```rust
let blob = kolmo::compress(data, kolmo::Mode::Hc, 0)?;  // 0 threads = all cores
let back = kolmo::decompress(&blob, 0)?;
let part = kolmo::read_range(&blob, 1_000_000, 4096, 0)?; // decode only the overlapping blocks
```

Modes: `Rt` (LZ4-speed) · `Fast` · `Eco` · `Hc` · `Max` · `Ultra` (archive tier, best ratio).

To build without liblzma even when it is installed (`max`/`ultra` then use the other coders):

```toml
kolmo = { version = "1", features = ["no-lzma"] }
```

For the command-line tool, use `brew install adnaneben/kolmo/kolmo`. The Python package is
`pip install kolmoc`.

Apache-2.0.
