#!/bin/sh
# 7-Zip, LZMA2 at -mx9 with $1 threads: compress file $2 into archive $3 (one entry, named "f").
set -e; d=$(mktemp -d); trap 'rm -rf "$d"' EXIT; ln "$2" "$d/f" 2>/dev/null || cp "$2" "$d/f"
rm -f "$3"; (cd "$d" && 7zz a -t7z -mx9 -m0=lzma2 -mmt="$1" -bso0 -bsp0 "$(cd "$(dirname "$3")" && pwd)/$(basename "$3")" f)
