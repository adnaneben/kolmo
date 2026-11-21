#!/bin/sh
# Builds build/natbench and build/hashq (native lane, rc3 wave 2). usage: make all && sh benchmark/rc3w2/native/build.sh
set -e
D=benchmark/rc3w2/native
NAT="src/algo/khuf.c src/algo/krans.c src/algo/klz.c src/algo/klzma.c src/algo/khash.c"
${CC:-cc} -O3 ${ARCH:--march=native} -Wall -DXXH_NAMESPACE=ZSTD_ -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz \
    $D/natbench.c $NAT src/arch/xxh3.c build/libvendor.a -o build/natbench -lpthread -lm
${CC:-cc} -O3 ${ARCH:--march=native} -Wall -Isrc $D/hashq.c src/algo/khash.c src/arch/xxh3.c -o build/hashq -lm
