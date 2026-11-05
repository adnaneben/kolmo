# Releasing kolmo

The packages are now self-contained: the Rust crate compiles its bundled C in `build.rs`
(no `make`, no external crates), and the Python package compiles its bundled C in `setup.py`.
Both were verified to build in isolation (extracted outside the repo). The steps below need
your accounts/credentials — run them yourself.

## 1. GitHub release (per version)

```sh
git tag -a v1.0.0 -m "kolmo 1.0.0" && git push origin v1.0.0
gh release create v1.0.0 --title "kolmo 1.0.0" --notes-file -   # paste notes, Ctrl-D
```
Then set the repo About blurb and topics (`compression`, `lossless-compression`, `zstd`,
`columnar`, `simd`, `apache-2`).

## 2. crates.io (Rust binding)

If the native core changed since the last release, refresh the crate's bundled copy first:

```sh
cd bindings/rust
./sync-native.sh            # copies ../../src + ../../vendor into native/ ; commit the result
cargo publish --dry-run     # packages and does a clean build (no repo paths needed)
cargo publish               # needs `cargo login` with a crates.io token
```
After this, `cargo add kolmo` works anywhere — the crate carries its own C sources and builds
them with the system compiler. `--features no-lzma` drops the liblzma dependency.

## 3. PyPI (Python binding) — automated

Pushing a `v*` tag runs `.github/workflows/wheels.yml`, which builds the sdist and manylinux +
macOS wheels (one `py3-none` wheel per platform, usable on every Python 3) and publishes them to
PyPI via **Trusted Publishing**. One-time setup: on PyPI, add a trusted publisher for this repo
and the `wheels.yml` workflow (https://docs.pypi.org/trusted-publishers/), and create a `pypi`
environment on the repo. No API token is stored.

To publish by hand instead:

```sh
cd bindings/python
./sync-native.sh                       # refresh native/ if the core changed; commit it
pipx run build                         # or: pip install build && python -m build
pipx run twine upload dist/*           # needs a PyPI token
```
Each wheel bundles its own `libkolmo` (liblzma is vendored in by auditwheel/delocate), so users
install with no toolchain. The sdist also builds from source on any platform.

## 4. Homebrew (CLI)

```sh
# after the GitHub release exists:
curl -sL https://github.com/adnaneben/kolmo/archive/refs/tags/v1.0.0.tar.gz | shasum -a 256
# paste the hash into packaging/homebrew/kolmo.rb (sha256), then:
```
Create a tap repo `github.com/adnaneben/homebrew-kolmo`, put `kolmo.rb` in it, push. Users then
`brew install adnaneben/kolmo/kolmo`. (homebrew-core itself requires notability + a release
history; the tap is the right first step.)

## 5. Benchmark leaderboards (reach)

- **lzbench**: fork it, register kolmo as a codec (wrap `czn_compress`/`czn_decompress`),
  and submit results — the standard way compressors get compared publicly.
- **Squash Compression Benchmark** and **Large Text Compression Benchmark (enwik8/enwik9)**:
  submit results produced by `benchmark/bench.sh`.
