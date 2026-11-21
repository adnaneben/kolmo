#!/bin/sh
# Builds build/gpu2/gpu2bench (macOS, Metal). Research harness of the rc3 wave 2 gpu2 lane; not part of the default
# build. Needs the default build's archives first: make build/libkolmoarch.a build/libvendor.a
# Run from the repository root: sh benchmark/rc3w2/gpu2/build.sh
set -e
O=build/gpu2
mkdir -p $O
CC=${CC:-cc}
CFLAGS="-O3 -mcpu=native -ffp-contract=off"
DEFS="-DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -DZSTD_DISABLE_ASM"
INC="-Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz -I$O"
# Metal sources are compiled at run time; embed them as byte lists
for k in gpu2 kvid sa; do
    xxd -i < src/arch/metal/$k.metal > $O/${k}_metal.inc
done
$CC $CFLAGS $DEFS $INC -c benchmark/rc3w2/gpu2/g6wrap.c -o $O/g6wrap.o
$CC $CFLAGS -Wall -Wextra -Wno-unused-parameter $INC -c src/arch/metal/gpu2_ref.c -o $O/gpu2_ref.o
$CC $CFLAGS -Wall $INC -c benchmark/rc3w2/gpu2/vidref.c -o $O/vidref.o
$CC $CFLAGS $INC -c src/arch/aarch64/kvid_neon.c -o $O/kvid_neon.o
$CC $CFLAGS -Wall -fobjc-arc $INC -c src/arch/metal/gpu2_metal.m -o $O/gpu2_metal.o
$CC $CFLAGS -fobjc-arc $INC -c src/arch/metal/sa_metal.m -o $O/sa_metal.o
$CC $CFLAGS -Wall -fobjc-arc $DEFS $INC -c benchmark/rc3w2/gpu2/gpu2bench.m -o $O/gpu2bench.o
$CC $CFLAGS $O/gpu2bench.o $O/g6wrap.o $O/gpu2_ref.o $O/vidref.o $O/kvid_neon.o $O/gpu2_metal.o $O/sa_metal.o \
    build/libkolmoarch.a build/libvendor.a -framework Metal -framework Foundation -framework IOKit -framework CoreFoundation \
    -lpthread -lm -o $O/gpu2bench
echo "built $O/gpu2bench"
