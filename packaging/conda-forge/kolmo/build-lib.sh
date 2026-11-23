#!/bin/bash
# libkolmo: the shared library and its header. The Makefile has no install target, so files are copied here.
set -euxo pipefail

# ARCH= drops the Makefile's -march=native/-mcpu=native default; conda-forge's CFLAGS set the target CPU baseline.
# The Makefile always appends -ffp-contract=off (frames must not depend on FMA contraction).
if [[ "${target_platform}" == osx-* ]]; then
  SOEXT=dylib
  SONAME_FLAG="-Wl,-install_name,@rpath/libkolmo.0.dylib"
  GC="-Wl,-dead_strip"
else
  SOEXT=so
  SONAME_FLAG="-Wl,-soname,libkolmo.so.0"
  GC="-Wl,--gc-sections"
fi

make -j"${CPU_COUNT}" lib CC="${CC}" ARCH= CFLAGS="${CFLAGS} -O3" \
  LIBS="${LDFLAGS} ${SONAME_FLAG} ${GC} -lpthread -lm"

install -d "${PREFIX}/lib" "${PREFIX}/include"
install -m 644 src/kolmo.h "${PREFIX}/include/kolmo.h"
if [[ "${SOEXT}" == dylib ]]; then
  install -m 755 build/libkolmo.dylib "${PREFIX}/lib/libkolmo.0.dylib"
  ln -sf libkolmo.0.dylib "${PREFIX}/lib/libkolmo.dylib"
else
  install -m 755 build/libkolmo.so "${PREFIX}/lib/libkolmo.so.${PKG_VERSION}"
  ln -sf "libkolmo.so.${PKG_VERSION}" "${PREFIX}/lib/libkolmo.so.0"
  ln -sf libkolmo.so.0 "${PREFIX}/lib/libkolmo.so"
fi
