# Releasing kolmo

Steps that need your credentials (run them yourself; nothing here is automated in CI yet).

## GitHub release (per version)

```sh
git tag -a v1.0.0 -m "kolmo 1.0.0" && git push origin v1.0.0
gh release create v1.0.0 --title "kolmo 1.0.0" --notes-file -   # paste notes, Ctrl-D
```
Then set the repo About blurb and topics (`compression`, `lossless-compression`, `zstd`,
`columnar`, `simd`, `apache-2`).

## crates.io (Rust binding)

```sh
cd bindings/rust
cargo publish --dry-run     # check
cargo publish               # needs `cargo login` with a crates.io token
```

## PyPI (Python binding)

Build a wheel per platform and upload. Locally for one platform:

```sh
cd bindings/python
python -m build             # builds sdist + a platform wheel (compiles libkolmo)
python -m twine upload dist/*
```
For broad coverage, build manylinux + macOS wheels with `cibuildwheel` in CI (each wheel
bundles its own `libkolmo`, so users need no toolchain).

## Benchmark leaderboards (reach)

- **lzbench**: fork it, register kolmo as a codec (wrap `czn_compress`/`czn_decompress`),
  and submit results — the standard way compressors get compared publicly.
- **Squash Compression Benchmark** and **Large Text Compression Benchmark (enwik8/enwik9)**:
  submit results produced by `benchmark/bench.sh`.
