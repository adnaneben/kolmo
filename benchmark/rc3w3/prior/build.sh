#!/bin/sh
# Builds the lane's research harnesses against the library objects of `make` (run `make` first).
set -e
cd "$(dirname "$0")/../../.."
for t in "${@:-cmprior}"; do
  gcc -O3 -march=native -ffp-contract=off -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/brotli/include -Ivendor/xz \
      -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 \
      benchmark/rc3w3/prior/$t.c build/libkolmoarch.a build/libvendor.a -o build/${OUT:-$t} -lpthread -lm
done
