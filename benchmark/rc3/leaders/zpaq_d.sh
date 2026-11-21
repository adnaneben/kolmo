#!/bin/sh
# zpaq: extract the entry of archive $2 to file $3 with $1 threads.
set -e; d=$(mktemp -d); trap 'rm -rf "$d"' EXIT; (cd "$d" && zpaq x "$2" -t"$1" > /dev/null); mv "$d/f" "$3"
