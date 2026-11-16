#!/bin/sh
# Builds and runs equiv.c against libbsc 3.4.0. libbsc is taken from vendor/libbsc if present, else from the git
# history (the last commit that had it), so the check can be repeated after its removal.
set -e
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
W=${EQUIV_WORK:-$(mktemp -d)}
if [ -d "$ROOT/vendor/libbsc/libbsc" ]; then BSC="$ROOT/vendor/libbsc"; else
  REV=$(git -C "$ROOT" log -n 1 --format=%H -- vendor/libbsc/libbsc/libbsc.h)^
  mkdir -p "$W/bsc" && git -C "$ROOT" archive "$REV" vendor/libbsc | tar -x -C "$W/bsc" && BSC="$W/bsc/vendor/libbsc"; fi
CXX=${CXX:-g++}; CC=${CC:-gcc}; FL="-O2 -march=native"
for f in $(cd "$BSC/libbsc" && find . -name '*.cpp'); do o="$W/bsc_$(echo "${f#./}" | tr / _).o"; $CXX $FL -c "$BSC/libbsc/$f" -o "$o"; done
$CC $FL -c "$BSC/libbsc/bwt/libsais/libsais.c" -o "$W/libsais.o"
$CC $FL -I"$BSC" -I"$ROOT/src" "$ROOT/benchmark/rc3/bwt/equiv.c" $(ls "$ROOT"/src/algo/lzp.c "$ROOT"/src/algo/qlfc*.c "$ROOT"/src/algo/par.c "$ROOT"/src/codecs/bwt.c) \
   "$W"/*.o -o "$W/equiv" -lstdc++ -lpthread
"$W/equiv" "$@"
