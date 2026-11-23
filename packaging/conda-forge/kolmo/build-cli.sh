#!/bin/bash
# kolmo: the command line. It links libkolmo.a statically, as `make` builds it upstream.
set -euxo pipefail

# ARCH= drops the Makefile's -march=native/-mcpu=native default; conda-forge's CFLAGS set the target CPU baseline.
if [[ "${target_platform}" == osx-* ]]; then GC="-Wl,-dead_strip"; else GC="-Wl,--gc-sections"; fi

make -j"${CPU_COUNT}" build/kolmo CC="${CC}" ARCH= CFLAGS="${CFLAGS} -O3" LIBS="${LDFLAGS} ${GC} -lpthread -lm"

install -d "${PREFIX}/bin"
install -m 755 build/kolmo "${PREFIX}/bin/kolmo"
