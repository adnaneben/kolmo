#!/bin/bash
# Build the Apple silicon wheel for a kolmoc release that is already on PyPI, and test it.
#
#   bash bindings/python/build-macos-wheel.sh [X.Y.Z]      (default: version in pyproject.toml)
#
# CI has no macOS runners, so this runs on an Apple silicon Mac. It builds from the exact sdist on
# PyPI (sha256 checked), verifies the wheel and runs the tests against it. From 0.0.1 every coder
# is Kolmo's own, so no codec library is built or linked. It does not
# upload: it prints the twine command to run yourself. See RELEASING.md, step 3b.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
VERSION="${1:-$(sed -n 's/^version = "\(.*\)"/\1/p' "$HERE/pyproject.toml")}"
MINOS=11.0
OUT="$HERE/dist-macos"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/kolmoc-macos-wheel.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

[ "$(uname -s)" = Darwin ] && [ "$(uname -m)" = arm64 ] || { echo "needs an Apple silicon Mac" >&2; exit 1; }
echo "== kolmoc $VERSION, macOS $MINOS+ arm64; work dir $WORK"

# 1. The published sdist, checked against PyPI's sha256.
read -r SDIST_URL SDIST_SHA < <(curl -fsS "https://pypi.org/pypi/kolmoc/$VERSION/json" | /usr/bin/python3 -I -c '
import json, sys
u = [x for x in json.load(sys.stdin)["urls"] if x["packagetype"] == "sdist"][0]
print(u["url"], u["digests"]["sha256"])')
curl -fsSL -o "$WORK/kolmoc-$VERSION.tar.gz" "$SDIST_URL"
echo "$SDIST_SHA  $WORK/kolmoc-$VERSION.tar.gz" | shasum -a 256 -c -

# 2. Build tools in their own venv (setuptools >= 61 reads pyproject.toml metadata).
/usr/bin/python3 -m venv "$WORK/venv"
"$WORK/venv/bin/pip" -q install --upgrade pip 'setuptools>=61' wheel delocate twine 2>/dev/null

# 3. The wheel.
mkdir -p "$OUT"
env MACOSX_DEPLOYMENT_TARGET=$MINOS ARCHFLAGS="-arch arm64" _PYTHON_HOST_PLATFORM="macosx-$MINOS-arm64" \
    "$WORK/venv/bin/pip" wheel -q --no-deps --no-build-isolation -w "$OUT" "$WORK/kolmoc-$VERSION.tar.gz"
WHEEL="$OUT/kolmoc-$VERSION-py3-none-macosx_${MINOS/./_}_arm64.whl"
[ -f "$WHEEL" ] || { echo "expected $WHEEL was not built" >&2; ls "$OUT" >&2; exit 1; }

# 4. Verify: architecture, minimum macOS, system-only dependencies, no codec library, metadata.
mkdir "$WORK/unz" && unzip -q "$WHEEL" -d "$WORK/unz"
LIB="$WORK/unz/libkolmo.dylib"
[ "$(lipo -archs "$LIB")" = arm64 ] || { echo "libkolmo.dylib is not arm64-only" >&2; exit 1; }
otool -l "$LIB" | grep -A3 LC_BUILD_VERSION | grep -q "minos $MINOS" || { echo "minimum macOS is not $MINOS" >&2; exit 1; }
DEPS="$("$WORK/venv/bin/delocate-listdeps" --all "$WHEEL")"
echo "$DEPS" | grep -qv '^/usr/lib/' && { echo "non-system dependency:" >&2; echo "$DEPS" >&2; exit 1; }
nm -gU "$LIB" | grep -Eq ' _kolmo_khash64$' || { echo "kolmo_khash64 is missing: not a 0.0.1+ build" >&2; exit 1; }
nm -g "$LIB" | grep -Eiq ' _(lzma_|ZSTD_|LZ4_|Brotli|XXH)' && { echo "a third-party codec symbol is present" >&2; exit 1; }
"$WORK/venv/bin/twine" check "$WHEEL" 2>/dev/null | tail -1

# 5. Install in a clean venv and run the tests from an empty directory, so the import is the wheel.
/usr/bin/python3 -m venv "$WORK/test"
"$WORK/test/bin/pip" -q install "$WHEEL" 2>/dev/null
mkdir "$WORK/run" && cp "$HERE/test_kolmo.py" "$WORK/run/"
(cd "$WORK/run" && "$WORK/test/bin/python" -I test_kolmo.py | tail -1)

echo
echo "Built and tested: $WHEEL ($(shasum -a 256 "$WHEEL" | cut -c1-12)…)"
echo "Upload from your own terminal (token limited to kolmoc):"
echo "  pipx run twine upload \"$WHEEL\""
