#!/bin/sh
# Builds the w4 entropy lane's tools into build/. usage: make all && sh benchmark/w4/entropy/build.sh
set -e
D=benchmark/w4/entropy
F="-O3 ${ARCH:--march=native} -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz"
${CC:-cc} $F $D/qtool.c build/libkolmoarch.a build/libvendor.a -o build/qtool -lpthread -lm
KA=src/arch/x86_64/khuf_x86_64.S; [ "$(uname -m)" = x86_64 ] || KA=-DKOLMO_NO_ASM
${CC:-cc} $F -Wall $D/hbench.c src/algo/khuf.c $KA build/libvendor.a -o build/hbench -lpthread -lm
${CC:-cc} $F $D/qdec.c src/algo/qlfc.c src/algo/par.c -o build/qdec -lpthread
${CC:-cc} $F $D/ebench.c build/libkolmoarch.a build/libvendor.a -o build/ebench -lpthread -lm
