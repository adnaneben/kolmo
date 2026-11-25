#!/bin/sh
# Extract immutable sources; the test controls never enter the shipping tree.
set -eu
repo=$1
out=$2
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$out"
test ! -e "$out/src"
git -C "$repo" archive 8d23095b2f09d5c9e3c3425123bea63641ca5d71 src Makefile experimental.mk | tar -x -C "$out"
python3 "$here/instrument.py" "$out"
make -C "$out" -j4 build/kolmo
