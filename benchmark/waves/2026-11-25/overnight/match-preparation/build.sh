#!/bin/sh
# Build a single-object trial against a completed, unmodified PR #104 build.
set -eu
src=$1
scratch=$2
here=$(cd "$(dirname "$0")" && pwd)
for arm in reference variant counter; do
    out="$scratch/$arm"
    mkdir -p "$out"
    case "$arm" in
        reference) python3 "$here/prototype.py" "$src/src/algo/klz.c" "$out/klz.c" --reference ;;
        counter) python3 "$here/prototype.py" "$src/src/algo/klz.c" "$out/klz.c" --counters ;;
        variant) python3 "$here/prototype.py" "$src/src/algo/klz.c" "$out/klz.c" ;;
    esac
    cc -O3 -mcpu=native -ffp-contract=off -ffunction-sections -fdata-sections \
        -I"$src/src" -I"$src/src/algo" -c "$out/klz.c" -o "$out/klz.o"
    cp "$src/build/libkolmo.a" "$out/libkolmo.a"
    ar -r "$out/libkolmo.a" "$out/klz.o"
    cc -O3 -mcpu=native -ffp-contract=off -I"$src/src" "$src/src/cli/cli.c" \
        "$src/src/cli/features.c" "$out/libkolmo.a" -o "$out/kolmo" -Wl,-dead_strip -lpthread -lm
done
