#!/bin/sh
# Usage: CLANG=clang LLD=ld.lld ./rebuild-probes.sh PATCHED_CHECKOUT EXTRACTED_DEBIAN_SYSROOT OUTPUT_DIR
set -eu
repo=$1
sysroot=$2
out=$3
cc=${CLANG:-clang}
lld=${LLD:-ld.lld}
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
lib=$sysroot/usr/lib/x86_64-linux-gnu
mkdir -p "$out"
for name in cgroup_probe before governor live-floor; do
  case $name in
    cgroup_probe) source=$repo/tests/cgroup_probe.c; opt=-O1;;
    before) source=$here/cgroup-probe-before.c; opt=-O1;;
    governor) source=$here/governor.c; opt=-O1;;
    live-floor) source=$here/../live_floor.c; opt=-O2;;
  esac
  if test "$name" = before; then set --; else set -- -I"$repo/src" -ffunction-sections -fdata-sections -ffp-contract=off; fi
  "$cc" --target=x86_64-linux-gnu --sysroot="$sysroot" -isystem "$sysroot/usr/include/x86_64-linux-gnu" "$@" "$opt" -c "$source" -o "$out/$name.o"
  "$lld" -m elf_x86_64 --gc-sections --dynamic-linker /lib64/ld-linux-x86-64.so.2 -o "$out/$name" "$lib/crt1.o" "$out/$name.o" "$lib/libc.so.6" "$lib/libc_nonshared.a" "$lib/ld-linux-x86-64.so.2"
done
