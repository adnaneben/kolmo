#!/bin/sh
# Copy the native core (src + vendored codecs) into the Python package so the sdist and wheels
# are self-contained. Run after changing ../../src or ../../vendor, then commit native/.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
dst="$here/native"
rm -rf "$dst"; mkdir -p "$dst"
cp -R "$root/src" "$dst/src"
cp -R "$root/vendor" "$dst/vendor"
echo "synced native/ from $root (src + vendor)"
