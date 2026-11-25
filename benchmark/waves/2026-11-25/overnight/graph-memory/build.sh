#!/bin/sh
# Extract only the pinned sources; do not modify the checkout or link a new default.
set -eu
repo=$1
out=$2
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$out"
test ! -e "$out/src"
git -C "$repo" archive a1d39559686a9ec17c8614b9154a64d7b5b98672 src Makefile experimental.mk | tar -x -C "$out"
cp "$here/probe.h" "$out/src/transforms/probe.h"
python3 "$here/instrument.py" "$out/src/transforms/graph.c"
make -C "$out" -j4 build/kolmo
