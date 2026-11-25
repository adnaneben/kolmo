#!/usr/bin/env bash
set -euo pipefail
phase=$1
out=$2
check_pe() {
  local binary=$1 log=$2
  objdump -f "$binary" | tee "$out/$log-format.txt"
  grep -qi 'file format pei-x86-64' "$out/$log-format.txt"
  objdump -p "$binary" > "$out/$log-pe.txt"
  grep -i 'DLL Name:' "$out/$log-pe.txt"
  grep -Eiq 'DLL Name: (api-ms-win-crt-|ucrtbase\.dll)' "$out/$log-pe.txt"
  if grep -Eiq 'DLL Name: (msys-2\.0|cygwin1)\.dll' "$out/$log-pe.txt"; then
    echo 'Product or control unexpectedly depends on a POSIX emulation runtime' >&2
    return 1
  fi
}
case $phase in
  smoke)
    cp bench/windows-ucrt/frames.csv "$out/frozen-expectations.csv"
    test "$MSYSTEM" = UCRT64
    case "$(command -v gcc)" in /ucrt64/bin/gcc|/ucrt64/bin/gcc.exe) ;; *) exit 1 ;; esac
    test "$(gcc -dumpmachine)" = x86_64-w64-mingw32
    uname -a
    gcc --version
    pacman -Q mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-winpthreads make
    gcc -O2 -Wall -Wextra -Werror -pthread bench/windows-ucrt/smoke.c -o "$out/ucrt-smoke.exe"
    check_pe "$out/ucrt-smoke.exe" smoke
    "$out/ucrt-smoke.exe"
    ;;
  product)
    set -x
    make -j2 CC=gcc AR=ar NO_ASM=1 CFLAGS='-O2 -march=x86-64 -pthread' build/libkolmo.a
    prefix="$out/install"
    mkdir -p "$prefix"/{bin,lib,include}
    gcc -shared -o build/kolmo.dll -Wl,--whole-archive build/libkolmo.a -Wl,--no-whole-archive \
      -Wl,--export-all-symbols -Wl,--out-implib,build/libkolmo.dll.a -lwinpthread -lm
    test -s build/libkolmo.dll.a
    cp build/kolmo.dll "$prefix/bin/"
    cp build/libkolmo.dll.a "$prefix/lib/"
    cp src/kolmo.h "$prefix/include/"
    check_pe "$prefix/bin/kolmo.dll" kolmo
    # Stage the runtime dependencies supplied by the native toolchain next to the DLL.
    for runtime in libwinpthread-1.dll libgcc_s_seh-1.dll; do
      if [[ -f /ucrt64/bin/$runtime ]]; then cp "/ucrt64/bin/$runtime" "$prefix/bin/"; fi
    done
    ;;
  *) exit 2 ;;
esac
