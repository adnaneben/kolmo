#!/bin/sh
# Refresh the native core and project notices in the self-contained Python source distribution.
set -e
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
cp "$root/LICENSE" "$root/NOTICE" "$here/"
dst="$here/native"
rm -rf "$dst"; mkdir -p "$dst"
cp -R "$root/src" "$dst/src"
# research tools are not part of the library: keep only the frontier sources kolmo.c can include (-DKOLMO_FRONTIER)
find "$dst/src/experimental" -type f ! -name 'frontier_*' -delete
echo "synced native/ from $root (src)"
