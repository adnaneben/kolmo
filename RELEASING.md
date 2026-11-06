# Releasing kolmo

kolmo ships in three places, all at the same version:

| Channel | Name | Install | Where it lives |
|---|---|---|---|
| Homebrew (CLI) | `kolmo` | `brew install adnaneben/kolmo/kolmo` | tap repo [adnaneben/homebrew-kolmo](https://github.com/adnaneben/homebrew-kolmo), `Formula/kolmo.rb` |
| PyPI (Python) | `kolmoc` (imports as `kolmo`) | `pip install kolmoc` | `bindings/python`; published by `.github/workflows/wheels.yml` |
| crates.io (Rust) | `kolmo` | `cargo add kolmo` | `bindings/rust`; published by hand with `cargo publish` |

1.0.0 is live on all three. The one-time setup is done: the PyPI Trusted Publisher and `pypi`
environment, the crates.io owner, and the tap repo. The steps below are what each release needs.

## Rules

- **Never move or re-push a tag after its release is out.** The Homebrew formula pins the sha256 of
  GitHub's tarball for the tag. Moving the tag changes the hash and breaks `brew install` for everyone.
- **A version can be uploaded only once** to PyPI and to crates.io. To fix a published release, bump
  to a new version.

## 1. Bump the version

Set the new version in all four places, then commit:

- `VERSION`
- `bindings/rust/Cargo.toml` (`version`)
- `bindings/python/pyproject.toml` (`version`)
- `packaging/homebrew/kolmo.rb` (`url`; the sha256 is set in step 5)

If the native core (`src/`, `vendor/`) changed, refresh the copies bundled in both packages and
commit them:

```sh
(cd bindings/rust && ./sync-native.sh)
(cd bindings/python && ./sync-native.sh)
```

## 2. Tag and create the GitHub release

```sh
git tag -a vX.Y.Z -m "kolmo X.Y.Z" && git push origin vX.Y.Z
gh release create vX.Y.Z --title "kolmo X.Y.Z" --notes-file notes.md
```

## 3. PyPI (automatic)

Pushing the tag runs `wheels.yml`. It builds the sdist and the manylinux x86-64 and aarch64 wheels,
then publishes them through Trusted Publishing. No token is stored. You can also start it by hand
with **Run workflow** (`workflow_dispatch`); the version comes from `pyproject.toml`. The publish
step uses `skip-existing`, so running it again for a version that is already up is harmless.

The macOS wheels are disabled for now because GitHub doesn't give this repo macOS runners (see the
comment in `wheels.yml`). macOS users build the sdist, which takes about a minute. Check the run with
`gh run list --workflow wheels.yml`.

To publish by hand instead: `cd bindings/python && pipx run build && pipx run twine upload dist/*`.

## 4. crates.io (manual)

```sh
cd bindings/rust
cargo publish --dry-run    # packages and builds in isolation
cargo publish              # needs `cargo login`
```

## 5. Homebrew tap

After the release exists, hash its source tarball:

```sh
curl -sL https://github.com/adnaneben/kolmo/archive/refs/tags/vX.Y.Z.tar.gz | shasum -a 256
```

Put the new `url` and `sha256` in `packaging/homebrew/kolmo.rb`, commit it here, then copy the same
file to `Formula/kolmo.rb` in the tap repo and push. To check it:

```sh
brew update && brew upgrade adnaneben/kolmo/kolmo   # or: brew install …
brew test adnaneben/kolmo/kolmo
brew audit --strict adnaneben/kolmo/kolmo
```

homebrew-core (plain `brew install kolmo`) requires notability and a release history. Apply there
once the project qualifies.

## 6. Benchmark leaderboards (reach)

- **lzbench**: fork it, register kolmo as a codec (wrap `czn_compress`/`czn_decompress`), and
  submit results. This is the standard way compressors get compared publicly.
- **Squash Compression Benchmark** and **Large Text Compression Benchmark (enwik8/enwik9)**: submit
  results produced by `benchmark/bench.sh`.
