#!/bin/sh
# Builds build/kolmo-libprof: build/kolmo with every call into a dependency timed (benchmark/rc3w2/native/libprof.c).
# usage: make all && sh benchmark/rc3w2/native/build_libprof.sh   (from the repository root)
set -e
F=benchmark/rc3w2/native/libprof.c
W=$(sed -n '/static const char \*pn/,/};/p' $F | grep -o '"[A-Za-z0-9_]*"' | tr -d '"' | sed 's/^/-Wl,--wrap=/' | tr '\n' ' ')
${CC:-cc} -O3 -march=native -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz \
    src/cli/cli.c src/cli/features.c $F build/libkolmo.a build/libvendor.a $W -o ${OUT:-build/kolmo-libprof} -lpthread -lm
