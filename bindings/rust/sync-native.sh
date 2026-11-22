#!/bin/sh
# Copy the native core (src; every coder is Kolmo's own) into the crate so the published
# crate is self-contained. Run this after updating anything under ../../src,
# then commit bindings/rust/native/. crates.io cannot reach files
# outside the crate directory, so the crate carries its own copy.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
dst="$here/native"
rm -rf "$dst"
mkdir -p "$dst"
cp -R "$root/src"    "$dst/src"
# research tools are not part of the library: keep only the frontier sources kolmo.c can include (-DKOLMO_FRONTIER)
find "$dst/src/experimental" -type f ! -name 'frontier_*' -delete
echo "synced native/ from $root (src)"
