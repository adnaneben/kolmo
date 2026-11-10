#!/bin/bash
# Build the Apple silicon wheel for a kolmoc release that is already on PyPI, and test it.
#
#   bash bindings/python/build-macos-wheel.sh [X.Y.Z]      (default: version in pyproject.toml)
#
# CI has no macOS runners, so this runs on an Apple silicon Mac. It builds from the exact sdist on
# PyPI (sha256 checked), links a static liblzma built for macOS 11+ from Homebrew's
# checksum-verified xz source, verifies the wheel and runs the tests against it. It does not
# upload: it prints the twine command to run yourself. See RELEASING.md, step 3b.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
VERSION="${1:-$(sed -n 's/^version = "\(.*\)"/\1/p' "$HERE/pyproject.toml")}"
MINOS=11.0
OUT="$HERE/dist-macos"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/kolmoc-macos-wheel.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

[ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = arm64 ] || { echo "needs an Apple silicon Mac" >&2; exit 1; }
command -v brew >/dev/null || { echo "needs Homebrew (for the xz source)" >&2; exit 1; }
echo "== kolmoc $VERSION, macOS $MINOS+ arm64; work dir $WORK"

# 1. The published sdist, checked against PyPI's sha256.
read -r SDIST_URL SDIST_SHA < <(curl -fsS "https://pypi.org/pypi/kolmoc/$VERSION/json" | /usr/bin/python3 -I -c '
import json, sys
u = [x for x in json.load(sys.stdin)["urls"] if x["packagetype"] == "sdist"][0]
print(u["url"], u["digests"]["sha256"])')
curl -fsSL -o "$WORK/kolmoc-$VERSION.tar.gz" "$SDIST_URL"
echo "$SDIST_SHA  $WORK/kolmoc-$VERSION.tar.gz" | shasum -a 256 -c -

# 2. Static liblzma for macOS 11+, from Homebrew's checksum-verified xz source.
HOMEBREW_NO_AUTO_UPDATE=1 brew fetch -q --build-from-source xz >/dev/null
XZ_TARBALL="$(brew --cache --build-from-source xz)"
mkdir -p "$WORK/xz" && tar -xzf "$XZ_TARBALL" -C "$WORK/xz" --strip-components 1
(cd "$WORK/xz" &&
  MACOSX_DEPLOYMENT_TARGET=$MINOS CFLAGS="-O2 -arch arm64 -mmacosx-version-min=$MINOS" \
  ./configure -q --prefix="$WORK/lzma" --disable-shared --enable-static --disable-xz --disable-xzdec \
    --disable-lzmadec --disable-lzmainfo --disable-scripts --disable-doc --disable-nls >/dev/null &&
  make -s -j"$(sysctl -n hw.ncpu)" >/dev/null 2>&1 && make -s install >/dev/null)
echo "liblzma: $(lipo -archs "$WORK/lzma/lib/liblzma.a")"

# 3. Build tools in their own venv (setuptools >= 61 reads pyproject.toml metadata).
/usr/bin/python3 -m venv "$WORK/venv"
"$WORK/venv/bin/pip" -q install --upgrade pip 'setuptools>=61' wheel delocate twine 2>/dev/null

# 4. The wheel. setup.py finds liblzma through CPATH/LIBRARY_PATH; only the static archive is there.
mkdir -p "$OUT"
env MACOSX_DEPLOYMENT_TARGET=$MINOS ARCHFLAGS="-arch arm64" _PYTHON_HOST_PLATFORM="macosx-$MINOS-arm64" \
    CPATH="$WORK/lzma/include" LIBRARY_PATH="$WORK/lzma/lib" \
    "$WORK/venv/bin/pip" wheel -q --no-deps --no-build-isolation -w "$OUT" "$WORK/kolmoc-$VERSION.tar.gz"
WHEEL="$OUT/kolmoc-$VERSION-py3-none-macosx_${MINOS/./_}_arm64.whl"
[ -f "$WHEEL" ] || { echo "expected $WHEEL was not built" >&2; ls "$OUT" >&2; exit 1; }

# 5. Verify: architecture, minimum macOS, system-only dependencies, liblzma linked in, metadata.
mkdir "$WORK/unz" && unzip -q "$WHEEL" -d "$WORK/unz"
LIB="$WORK/unz/libkolmo.dylib"
[ "$(lipo -archs "$LIB")" = arm64 ] || { echo "libkolmo.dylib is not arm64-only" >&2; exit 1; }
otool -l "$LIB" | grep -A3 LC_BUILD_VERSION | grep -q "minos $MINOS" || { echo "minimum macOS is not $MINOS" >&2; exit 1; }
DEPS="$("$WORK/venv/bin/delocate-listdeps" --all "$WHEEL")"
echo "$DEPS" | grep -qv '^/usr/lib/' && { echo "non-system dependency:" >&2; echo "$DEPS" >&2; exit 1; }
nm -gU "$LIB" | grep -q ' _lzma_raw_buffer_encode$' || { echo "liblzma is not linked in" >&2; exit 1; }
"$WORK/venv/bin/twine" check "$WHEEL" 2>/dev/null | tail -1

# 6. Install in a clean venv and run the tests from an empty directory, so the import is the wheel.
/usr/bin/python3 -m venv "$WORK/test"
"$WORK/test/bin/pip" -q install "$WHEEL" 2>/dev/null
mkdir "$WORK/run" && cp "$HERE/test_kolmo.py" "$WORK/run/"
(cd "$WORK/run" && "$WORK/test/bin/python" -I test_kolmo.py | tail -1)

echo
echo "Built and tested: $WHEEL ($(shasum -a 256 "$WHEEL" | cut -c1-12)…)"
echo "Upload from your own terminal (token limited to kolmoc):"
echo "  pipx run twine upload \"$WHEEL\""
