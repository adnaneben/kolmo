#!/bin/sh
# Run on a fresh copy of this directory, with a local Kolmo checkout containing the pinned commit.
set -eu
repo=${1:?pass the Kolmo checkout}
evidence=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
rev=ac10ce80048ab0664074f7ba031634055f4d2225
git -C "$repo" archive "$rev" Makefile experimental.mk src | tar xf - -C "$work"
make -s -j4 -C "$work" ARCH= build/libkolmoarch.a build/libkolmoarch-asan.a
cp "$evidence/original-count.c" "$work/"
cd "$work"
cc -O1 -ffp-contract=off -DKOLMO_NO_GPU -I. -Isrc original-count.c build/libkolmoarch.a -lpthread -lm -o original
./original > "$evidence/original.log"
python3 "$evidence/instrument.py" "$work"
cp "$evidence/probe.c" .
cc -O1 -ffp-contract=off -DKOLMO_NO_GPU -Isrc probe.c build/libkolmoarch.a -lpthread -lm -o regular
./regular > "$evidence/rows.csv" 2> "$evidence/terminal.log"
cc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -ffp-contract=off -DKOLMO_NO_GPU -Isrc \
    probe.c build/libkolmoarch-asan.a -lpthread -lm -o sanitized
ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./sanitized > "$work/sanitized.csv" 2> "$evidence/sanitized.log"
cmp "$work/sanitized.csv" "$evidence/rows.csv"
