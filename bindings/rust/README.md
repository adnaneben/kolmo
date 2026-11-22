# kolmo (Rust binding)

Safe Rust wrappers over the [kolmo](https://github.com/adnaneben/kolmo) native core. The
build compiles and statically links the C core, so there is no runtime shared-library
dependency. Building needs a C compiler and nothing else: every coder is Kolmo's own (KLZ,
KLZH2, KLZMA, khuf, BWT, context mixing), with no third-party codec library.

```sh
cargo add kolmo
```

```rust
let blob = kolmo::compress(data, kolmo::Mode::BALANCED, 0)?; // 0 threads = all cores
let back = kolmo::decompress(&blob, 0)?;
let part = kolmo::read_range(&blob, 1_000_000, 4096, 0)?;    // decode only the overlapping blocks
```

Modes: `Mode::SPEED` (fastest), `Mode::BALANCED` (the usual choice), `Mode::RATIO` (smallest at
practical speed). The underlying settings are `Rt` · `Fast` · `Eco` · `Hc` · `Max` · `Ultra`
(context-mixing archive tier).

**From 1.x:** 0.0.1 writes container version 12 and cannot read blobs written by kolmo 1.0.x
(they fail with code -13, `KOLMO_ERR_LEGACY`; never wrong data). Dictionaries and trained
profiles are removed. A dependency written as `kolmo = "1"` will not resolve to 0.0.1; use
`kolmo = "0.0.1"`. The `no-lzma` feature no longer exists.

For the command-line tool, use `brew install adnaneben/kolmo/kolmo`. The Python package is
`pip install kolmoc`.

Apache-2.0.
