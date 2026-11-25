# Releasing kolmo

kolmo ships in three places, all at the same version:

| Channel | Name | Install | Where it lives |
|---|---|---|---|
| Homebrew (CLI) | `kolmo` | `brew install adnaneben/kolmo/kolmo` | tap repo [adnaneben/homebrew-kolmo](https://github.com/adnaneben/homebrew-kolmo), `Formula/kolmo.rb` |
| PyPI (Python) | `kolmoc` (imports as `kolmo`) | `pip install kolmoc` | `bindings/python`; published by `.github/workflows/wheels.yml`; Apple silicon wheel built by hand (step 3b) |
| crates.io (Rust) | `kolmo` | `cargo add kolmo` | `bindings/rust`; published by hand with `cargo publish` |

0.0.x is live on all three (1.0.0 and 1.0.1 are yanked on PyPI and crates.io). The one-time setup is done: the PyPI Trusted Publisher and `pypi`
environment, the crates.io owner, and the tap repo. The steps below are what each release needs.

## Rules

- **Never move or re-push a tag after its release is out.** The Homebrew formula pins the sha256 of
  GitHub's tarball for the tag. Moving the tag changes the hash and breaks `brew install` for everyone.
- **A version can be uploaded only once** to PyPI and to crates.io. To fix a published release, bump
  to a new version.

## 1. Bump the version

Set the new version in all five places, then commit:

- `VERSION`
- `bindings/rust/Cargo.toml` (`version`)
- `bindings/python/pyproject.toml` (`version`)
- `packaging/homebrew/kolmo.rb` (`url`; the sha256 is set in step 5)
- `src/cli/cli.c` (`KOLMO_VERSION_STR`, printed by `kolmo -V`)

Then check what users will read (the PyPI and crates.io pages are frozen once published):

```sh
python3 tests/check_release_docs.py build/kolmo     # stale claims in release-facing files; all labels = VERSION
```

It also fails when a user-facing document or `kolmo -h` shows an option removed in 0.0.2 as current usage, and when
the level table of `docs/LEVELS.md` and the one `kolmo -h` prints disagree.

and read `bindings/python/README.md` and `bindings/rust/README.md` in full: they are the package pages.

If the native core (`src/`) changed, refresh the copies bundled in both packages and
commit them:

```sh
(cd bindings/rust && ./sync-native.sh)
(cd bindings/python && ./sync-native.sh)
```

Freeze this release's frames (COMPATIBILITY.md): build the release binary, add its frames to the corpus and commit
them with the version bump. Existing frames are never regenerated.

```sh
make -j4 ARCH= build/kolmo
python3 tests/make_v12_frames.py --kolmo build/kolmo --release X.Y.Z    # adds tests/v12/rX.Y.Z/ and MANIFEST lines
python3 tests/check_v12_pinned.py --write                                # pins the new files in tests/v12.sha256 (append-only)
make check-cm-reference                                                  # decodes every context-mixing stream whole (20-40 minutes)
python3 tests/cm_reference.py --write-prefixes tests/v12                 # adds the new frames' digests to tests/vectors/cm/prefixes.json
make check                                                               # the corpus tests include the new frames
```

The distribution drafts in `packaging/` (conda-forge, Debian, AUR, nixpkgs, homebrew-core) name a release tarball and
its sha256, and CI builds them from that tarball. Bump them after the tag exists (`packaging/README.md`), not here.

## 2. Tag and create the GitHub release

```sh
git tag -a vX.Y.Z -m "kolmo X.Y.Z" && git push origin vX.Y.Z
gh release create vX.Y.Z --title "kolmo X.Y.Z" --notes-file notes.md
```

Pushing the tag also runs `.github/workflows/release.yml`: it builds the source tarball and the CLI binaries
(Linux x86-64 and arm64, macOS arm64), signs them with build provenance, and attaches them and `SHA256SUMS` to the
release (as a draft if `gh release create` has not run yet; publish the draft by hand). See
[Signed release files](#7-signed-release-files).

## 3. PyPI (automatic)

Pushing the tag runs `wheels.yml`. It builds the sdist and the manylinux x86-64 and aarch64 wheels,
then publishes them through Trusted Publishing. No token is stored. You can also start it by hand
with **Run workflow** (`workflow_dispatch`); the version comes from `pyproject.toml`. The publish
step uses `skip-existing`, so running it again for a version that is already up is harmless.

Check the run with `gh run list --workflow wheels.yml`.

To publish by hand instead: `cd bindings/python && pipx run build && pipx run twine upload dist/*`.

### 3b. Apple silicon wheel (manual, on a Mac)

CI can't build macOS wheels because GitHub doesn't give this repo macOS runners (see the comment
in `wheels.yml`). Build the Apple silicon wheel on an Apple silicon Mac once the release is on PyPI:

```sh
bash bindings/python/build-macos-wheel.sh            # version from pyproject.toml, or pass X.Y.Z
```

The script:
- downloads the release sdist from PyPI and checks its sha256 against PyPI's record, so the wheel
  is built from exactly the published source
- (no codec library is needed any more: every coder is Kolmo's own; earlier versions built a static liblzma here)
- builds the wheel (`py3-none-macosx_11_0_arm64`), then checks the architecture, the minimum macOS
  version, that it depends only on system libraries, and `twine check`
- installs it in a clean venv, runs `test_kolmo.py`, and prints the upload command

Upload with `twine upload <wheel>` from your own terminal, using a PyPI token limited to `kolmoc`.
PyPI accepts new wheel files for a version that is already published.

An Intel (x86_64) wheel needs an Intel Mac, or Rosetta to test it, so Intel Macs install from the
sdist.

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

- **lzbench**: fork it, register kolmo as a codec (wrap `kolmo_compress`/`kolmo_decompress`), and
  submit results. This is the standard way compressors get compared publicly.
- **Squash Compression Benchmark** and **Large Text Compression Benchmark (enwik8/enwik9)**: submit
  results produced by `benchmark/bench.sh`.

## 7. Signed release files

Every release file built in CI carries a GitHub build provenance attestation: a Sigstore-signed SLSA v1 statement
that names the file's sha256, this repository, the workflow file, the commit and the tag. No key is stored: the
signature uses the workflow's short-lived OIDC identity, and the statement is logged in Sigstore's public
transparency log (Rekor).

| Files | Workflow | When |
|---|---|---|
| `kolmo-X.Y.Z.tar.gz` (source), `kolmo-X.Y.Z-{linux-x86_64,linux-arm64,macos-arm64}.tar.gz` (CLI) | `release.yml` | tag push, or **Run workflow** |
| PyPI sdist and wheels | `wheels.yml` (`publish` job) | tag push, or **Run workflow** |

The Apple silicon wheel built by hand (step 3b) and the crate published with `cargo publish` (step 4) are not
attested. The PyPI upload also attaches PEP 740 attestations, shown on each file's PyPI page.

A pull request that changes `release.yml` runs it too, to check the builds, but signs and uploads nothing. The
release workflow needs no repository secret; `wheels.yml` keeps using the `pypi` environment.

### Verifying release files

With the GitHub CLI (2.49 or later):

```sh
gh attestation verify kolmo-X.Y.Z-linux-x86_64.tar.gz --repo adnaneben/kolmo
gh attestation verify kolmoc-X.Y.Z.tar.gz --repo adnaneben/kolmo          # a PyPI file, downloaded with pip download
```

It checks that the file's sha256 is covered by a signed statement from a workflow of `adnaneben/kolmo`. To also
pin the workflow and the tag:

```sh
gh attestation verify kolmo-X.Y.Z-linux-x86_64.tar.gz --repo adnaneben/kolmo \
  --signer-workflow adnaneben/kolmo/.github/workflows/release.yml --source-ref refs/tags/vX.Y.Z
```

Without `gh`, download the bundle (`gh attestation download`, or the release's attestation page) and verify it with
`cosign verify-blob-attestation` or the `sigstore` Python package. The release's `SHA256SUMS` lists the same hashes.

The source tarball of the release workflow is not the tarball GitHub generates for the tag
(`archive/refs/tags/vX.Y.Z.tar.gz`, which the Homebrew formula pins); GitHub's own tarball is not attested.

### Reproducible builds

What holds (checked 2026-11-23):

- **Source tarball.** `git archive` of the tag piped through `gzip -n`: file order, mtimes (the commit time) and
  owners come from the commit, and `gzip -n` stores no name or time. `release.yml` builds it twice and compares.
- **CLI binaries.** Built with `make build/kolmo ARCH=` (no `-march=native`) and `-ffile-prefix-map`. The sources are
  compiled by relative path and use no `__DATE__`, `__TIME__` or `__TIMESTAMP__`, so the binary does not depend on
  the checkout's path or the time. On macOS arm64 (Apple clang 21) two checkouts at different paths gave identical
  binaries with and without the prefix map. `release.yml` builds each binary twice, from checkouts at different
  paths on the same runner, and fails if the bytes differ.
- **CLI tarballs.** ustar format, fixed file list and order, mode, owner `root:0`, mtime = commit time, no extended
  attributes, `gzip -n`: identical across runs (checked on macOS).

What does not hold yet:

- The bytes depend on the compiler and system libraries: a rebuild on another runner image, compiler version or
  macOS SDK can differ. Rebuilding a release bit for bit needs the same image (named in the attestation's
  `runnerEnvironment` and in the job log) or a pinned container; there is none yet.
- The PyPI wheels (cibuildwheel and auditwheel) and the hand-built macOS wheel have not been checked.
- `SOURCE_DATE_EPOCH` is not needed by the build itself (nothing embeds a time); the archives take the commit time.
