# kolmo (Rust binding)

Safe Rust wrappers over the [kolmo](https://github.com/adnaneben/kolmo) native core. The
build compiles and statically links the C core, so there is no runtime shared-library
dependency. Building needs a C compiler and nothing else: every coder is Kolmo's own (KLZ,
KLZH2, KLZMA, khuf, BWT, context mixing), with no third-party codec library.

```sh
cargo add kolmo
```

```rust
let blob = kolmo::compress(data, kolmo::Level::COMPACT, 0)?; // 0 threads = all cores
let back = kolmo::decompress(&blob, 0)?;
let part = kolmo::read_range(&blob, 1_000_000, 4096, 0)?;     // decode only the overlapping blocks
```

Levels: one scale, `Level::new(1..=9)`, from `Level::FASTEST` (1) through `Level::LIGHT` (3, the default),
`Level::BALANCED` (5) and `Level::COMPACT` (7) to `Level::MAX` (9); `Level::ULTRA` is the context-mixing tier
outside the scale. A level gives the same bytes as `kolmo -N` on the command line. `compress_limited` and
`decompress_limited` take a memory limit (and `decompress_limited` an optional bound on the decoded size, advisable
for untrusted input); `decompress` checks every frame before it reserves the output, and a size the system cannot
reserve is an error, never an abort. The decoding functions read everything the command line writes: one frame,
frames one after another, and `kolmo --stream` output (also what the CLI writes for pipes over 256 MiB).

The crate declares `links = "kolmo"`, so cargo refuses a dependency graph holding two versions of it (they would share
one copy of the C core). The build compiles the C core with the [`cc`](https://docs.rs/cc) crate for the target, so
cross builds work (`cargo build --target x86_64-apple-darwin` on Apple silicon; `CC_<target>`, `CFLAGS_<target>` and
`AR_<target>` are honoured).

**New in 0.0.3: a data-loss fix.** Earlier 0.0.x releases could write a blob that they could not read back: a
table (CSV, TSV, or a space-separated log) with one field longer than 65,535 bytes, at levels 3 to 9. `compress`
succeeded and `decompress` then failed with code -1 (`KOLMO_ERR_CORRUPT`, "corrupt or unsupported input"). The data in those blobs is
intact. This release reads them, and it no longer writes such blocks, so the earlier releases read its output. If you
compressed data with an earlier release, upgrade and run `decompress` on those blobs before you delete the
originals. Details are in the [changelog](https://github.com/adnaneben/kolmo/blob/main/CHANGELOG.md).

**Since 0.0.2:** `Mode` was replaced by `Level` (`Mode::SPEED` is now `Level::FASTEST`, `BALANCED` `Level::LIGHT`,
`RATIO` `Level::MAX`; `Eco` level 5, `Hc` level 7, `Ultra` `Level::ULTRA`), and the dictionary functions were removed.

**From 1.x:** since 0.0.1, kolmo writes container version 12 and cannot read blobs written by kolmo 1.0.x
(they fail with code -13, `KOLMO_ERR_LEGACY`; never wrong data). Dictionaries and trained
profiles are removed. A dependency written as `kolmo = "1"` will not resolve to the 0.0.x line; switch
it to the current version with `cargo remove kolmo && cargo add kolmo`. The `no-lzma` feature no longer exists.

For the command-line tool, use `brew install adnaneben/kolmo/kolmo`. The Python package is
`pip install kolmoc`.

Apache-2.0.
