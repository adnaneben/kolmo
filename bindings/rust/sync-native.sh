#!/bin/sh
# Copy the native core (src + vendored codecs) into the crate so the published
# crate is self-contained. Run this after updating anything under ../../src or
# ../../vendor, then commit bindings/rust/native/. crates.io cannot reach files
# outside the crate directory, so the crate carries its own copy.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
dst="$here/native"
rm -rf "$dst"
mkdir -p "$dst"
cp -R "$root/src"    "$dst/src"
cp -R "$root/vendor" "$dst/vendor"
echo "synced native/ from $root (src + vendor)"
