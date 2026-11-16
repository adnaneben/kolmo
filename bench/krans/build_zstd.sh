#!/bin/sh
# Build zstd's library (for build/krans-rivals) outside the repository: download the zstd 1.5.6 release, check its
# hash, compile lib/ with $CC -O3 (zstd selects BMI2 code at run time), write <dir>/zstd-1.5.6/lib/libzstd.a.
# usage: bench/krans/build_zstd.sh <dir>     then: make build/krans-rivals ZSTD_DIR=<dir>/zstd-1.5.6/lib
set -eu
D=${1:?usage: build_zstd.sh dir}; CC=${CC:-cc}
mkdir -p "$D"; cd "$D"
[ -f zstd-1.5.6.tar.gz ] || curl -sSfL -o zstd-1.5.6.tar.gz https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz
echo "8c29e06cf42aacc1eafc4077ae2ec6c6fcb96a626157e0593d5e82a34fd403c1  zstd-1.5.6.tar.gz" | sha256sum -c -
tar xzf zstd-1.5.6.tar.gz
L=$PWD/zstd-1.5.6/lib; mkdir -p obj
for f in "$L"/common/*.c "$L"/compress/*.c "$L"/decompress/*.c; do $CC -O3 -I"$L" -I"$L/common" -c "$f" -o obj/$(basename "$f" .c).o; done
case $(uname -m) in x86_64) $CC -c "$L/decompress/huf_decompress_amd64.S" -o obj/huf_amd64.o ;; esac
ar rcs "$L/libzstd.a" obj/*.o
echo "ZSTD_DIR=$L"
