#!/bin/sh
# Builds the lane's research harnesses (the library objects of `make` must exist: run `make` first).
set -e
cd "$(dirname "$0")/../../.."
for t in "${@:-cmx}"; do
  gcc -O3 -march=native -ffp-contract=off -fno-plt -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz \
      -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 $CMXFLAGS \
      benchmark/w4/cm/$t.c build/libkolmoarch.a build/libvendor.a -o build/$t$SUF -lpthread -lm
done
