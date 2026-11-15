#!/bin/sh
# Linux counterpart of kolmo's benchmark/lzbench/setup_macos.sh, written for the rc2 Linux run (not a source change).
# Same design: two exports of one commit, each built with `make ARCH= lib` (-O3 -fno-plt, no -march), a shared
# library that exports only kolmo_* (GNU ld version script instead of -exported_symbols_list), static liblzma linked
# in (as on the Mac), loaded by kolmo_lzbench_dl.cpp; lzbench built with its own default flags (no -march=native).
set -e
LZB=$1; WORK=$2; COMMIT=${3:-HEAD}; root=$4
here="$root/benchmark/lzbench"
LZB=$(cd "$LZB" && pwd)
mkdir "$WORK"; WORK=$(cd "$WORK" && pwd)
ncpu=$(nproc)
commit=$(git -C "$root" rev-parse --verify "$COMMIT^{commit}")
echo "kolmo commit: $commit"
printf '{ global: kolmo_*; local: *; };\n' > "$WORK/kolmo.map"
LZMA_A=/usr/lib/x86_64-linux-gnu/liblzma.a
build() {
    mkdir "$WORK/$1"
    git -C "$root" archive "$commit" | tar -x -C "$WORK/$1"
    make -C "$WORK/$1" -j"$ncpu" ARCH="$2" LZMALIB="$LZMA_A" lib \
        CXXLIB="-lstdc++ -Wl,--version-script=$WORK/kolmo.map" > "$WORK/$1.log" 2>&1 \
        || { tail -n 20 "$WORK/$1.log"; echo "the $1 build failed"; exit 1; }
    other=$(nm -D --defined-only "$WORK/$1/build/libkolmo.so" | awk '$2 ~ /[TDBRVW]/ && $3 !~ /^kolmo_/ {print $3}' | head -n 5)
    [ -z "$other" ] || { echo "$1 library exports more than kolmo_*: $other"; exit 1; }
    echo "$1: $(nm -D --defined-only "$WORK/$1/build/libkolmo.so" | awk '$3 ~ /^kolmo_/' | wc -l) exported symbols, all kolmo_*; deps: $(ldd "$WORK/$1/build/libkolmo.so" | awk '{print $1}' | tr '\n' ' ')"
}
build release ""
build control "-DKOLMO_FORCE_GENERIC_INT_INV -DKOLMO_FORCE_GENERIC_INT_FWD"
mkdir -p "$LZB/misc/kolmo"
cp "$WORK/release/build/libkolmo.so" "$LZB/misc/kolmo/libkolmo.so"
cp "$WORK/control/build/libkolmo.so" "$LZB/misc/kolmo/libkolmo_ctl.so"
cp "$WORK/release/src/kolmo.h" "$here/kolmo_lzbench_dl.cpp" "$LZB/misc/kolmo/"
cp "$here/kolmo_dl.mk" "$LZB/mk/kolmo.mk"
version=$(cat "$WORK/release/VERSION")
{ echo "#define KOLMO_LIB_PATH \"$LZB/misc/kolmo/libkolmo.so\""; echo "#define KOLMO_LIB_CTL_PATH \"$LZB/misc/kolmo/libkolmo_ctl.so\""; } > "$LZB/misc/kolmo/kolmo_paths.h"
{ echo "kolmo commit $commit, version $version"; echo "release: make ARCH= LZMALIB=$LZMA_A lib"
  echo "control: make ARCH=\"-DKOLMO_FORCE_GENERIC_INT_INV -DKOLMO_FORCE_GENERIC_INT_FWD\" LZMALIB=$LZMA_A lib"
  echo "link change in both: --version-script (kolmo_* only)"; sha256sum "$LZB/misc/kolmo/libkolmo.so" "$LZB/misc/kolmo/libkolmo_ctl.so"; } > "$LZB/misc/kolmo/BUILD-INFO.txt"
# Register: reuse the python block of setup_macos.sh verbatim.
sed -n "/^python3 -I - \"\$LZB\" \"\$version\" <<'PY'/,/^PY\$/p" "$here/setup_macos.sh" > "$WORK/register.sh"
sh "$WORK/register.sh"
make -C "$LZB" -j"$ncpu"
echo "done: $LZB/lzbench"
