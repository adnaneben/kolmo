#!/bin/sh
# zpaq -m5 with $1 threads: compress file $2 into archive $3 (one entry, named "f").
set -e; d=$(mktemp -d); trap 'rm -rf "$d"' EXIT; { ln "$2" "$d/f" 2>/dev/null || cp "$2" "$d/f"; }; rm -f "$3"
(cd "$d" && zpaq a "$(cd "$(dirname "$3")" && pwd)/$(basename "$3")" f -m5 -t"$1" > /dev/null)
