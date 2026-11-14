#!/bin/sh
# Add kolmo to an lzbench checkout on macOS as two ordinary codecs, then build lzbench.
#
#   git clone https://github.com/inikep/lzbench
#   sh benchmark/lzbench/setup_macos.sh ./lzbench <new work dir> [commit] ["BUILD_ONLY codec list"]
#   ./lzbench/lzbench -ekolmo,4/kolmo_ctl,4/zstd,3 file
#
# What it does
#  1. Exports <commit> (default HEAD) of this repository twice into the work dir and builds the shared library in
#     each export with the release flags (`make ARCH= lib`: -O3 and no -mcpu, as the release binaries are built):
#        release   no extra define                     -> codec `kolmo`
#        control   -DKOLMO_FORCE_GENERIC_INT_INV       -> codec `kolmo_ctl` (generic integer inverse in the decoder)
#     The one difference from the stock `make lib` is at link time: only kolmo_* is exported and the install name is
#     the library's place in the lzbench checkout. The compiled code is the release build's.
#  2. Copies both libraries, kolmo.h and the wrapper into <lzbench>/misc/kolmo, and kolmo_dl.mk to mk/kolmo.mk.
#  3. Registers `kolmo` and `kolmo_ctl` (levels 0..17) in bench/codecs.h and bench/lzbench.h. Safe to run again.
#  4. Builds lzbench with lzbench's own default flags (-O3 -DNDEBUG, no -mcpu).
#
# Fairness rules this follows
#  - kolmo runs with threads = 1 and its default block size, as every other lzbench entry does by default.
#  - Same compiler and the same optimisation level on both sides; neither side is tuned for this machine's CPU.
#  - kolmo's vendored zstd, lz4, brotli, libbsc and liblzma live inside the two libraries and are not exported, so
#    kolmo never runs lzbench's copies and no other entry runs kolmo's.
# The Linux counterpart is setup.sh (static, partially linked, -march=native on both sides).
set -e
LZB=${1:?usage: setup_macos.sh <lzbench checkout> <new work dir> [commit] [BUILD_ONLY codec list]}
WORK=${2:?usage: setup_macos.sh <lzbench checkout> <new work dir> [commit] [BUILD_ONLY codec list]}
COMMIT=${3:-HEAD}
ONLY=${4:-}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
LZB=$(cd "$LZB" && pwd)
mkdir "$WORK"   # must not exist yet: nothing is overwritten
WORK=$(cd "$WORK" && pwd)
case "$LZB$WORK" in *[[:space:]]*) echo "setup_macos.sh: paths with spaces are not supported"; exit 1 ;; esac
ncpu=$(sysctl -n hw.ncpu)
commit=$(git -C "$root" rev-parse --verify "$COMMIT^{commit}")
echo "kolmo commit: $commit"

# 1. Two builds of the same commit.
printf '_kolmo_*\n' > "$WORK/kolmo.exp"
build() {   # <name> <ARCH value> <library file name in lzbench>
    mkdir "$WORK/$1"
    git -C "$root" archive "$commit" | tar -x -C "$WORK/$1"
    make -C "$WORK/$1" -j"$ncpu" ARCH="$2" lib \
        CXXLIB="-lc++ -Wl,-exported_symbols_list,$WORK/kolmo.exp -Wl,-install_name,$LZB/misc/kolmo/$3" > "$WORK/$1.log" 2>&1 \
        || { tail -n 20 "$WORK/$1.log"; echo "setup_macos.sh: the $1 build failed, see $WORK/$1.log"; exit 1; }
    other=$(nm -gU "$WORK/$1/build/libkolmo.dylib" | awk '$3 !~ /^_kolmo_/ {print $3}' | head -n 5)
    [ -z "$other" ] || { echo "setup_macos.sh: $1 library exports more than kolmo_*: $other"; exit 1; }
    echo "$1: $(nm -gU "$WORK/$1/build/libkolmo.dylib" | wc -l | tr -d ' ') exported symbols, all kolmo_*"
}
build release "" libkolmo.dylib
build control "-DKOLMO_FORCE_GENERIC_INT_INV" libkolmo_ctl.dylib

# 2. Drop the codec into lzbench.
mkdir -p "$LZB/misc/kolmo"
cp "$WORK/release/build/libkolmo.dylib" "$LZB/misc/kolmo/libkolmo.dylib"
cp "$WORK/control/build/libkolmo.dylib" "$LZB/misc/kolmo/libkolmo_ctl.dylib"
cp "$WORK/release/src/kolmo.h" "$here/kolmo_lzbench_dl.cpp" "$LZB/misc/kolmo/"
cp "$here/kolmo_dl.mk" "$LZB/mk/kolmo.mk"
version=$(cat "$WORK/release/VERSION")
{
    echo "// written by kolmo's benchmark/lzbench/setup_macos.sh"
    echo "#define KOLMO_LIB_PATH \"$LZB/misc/kolmo/libkolmo.dylib\""
    echo "#define KOLMO_LIB_CTL_PATH \"$LZB/misc/kolmo/libkolmo_ctl.dylib\""
} > "$LZB/misc/kolmo/kolmo_paths.h"
{
    echo "kolmo commit $commit, version $version"
    echo "release: make ARCH= lib"
    echo "control: make ARCH=-DKOLMO_FORCE_GENERIC_INT_INV lib"
    echo "link change in both: -exported_symbols_list (kolmo_* only) and -install_name"
    shasum -a 256 "$LZB/misc/kolmo/libkolmo.dylib" "$LZB/misc/kolmo/libkolmo_ctl.dylib"
} > "$LZB/misc/kolmo/BUILD-INFO.txt"

# 3. Register the two codecs (idempotent): declarations, table entries, per-level algorithm names.
python3 -I - "$LZB" "$version" <<'PY'
import pathlib
import re
import sys

lzb, version = pathlib.Path(sys.argv[1]), sys.argv[2]
codecs = lzb / "bench/codecs.h"
s = re.sub(r"#ifndef BENCH_REMOVE_KOLMO\n.*?#endif\n\n", "", codecs.read_text(), flags=re.S)
sig = "(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options);"
fns = ["lzbench_kolmo_compress", "lzbench_kolmo_decompress", "lzbench_kolmo_ctl_compress", "lzbench_kolmo_ctl_decompress"]
decl = "#ifndef BENCH_REMOVE_KOLMO\n"
decl += "    char* lzbench_kolmo_init(size_t insize, size_t level, size_t);\n"
decl += "    char* lzbench_kolmo_ctl_init(size_t insize, size_t level, size_t);\n"
decl += "    void lzbench_kolmo_deinit(char* workmem);\n"
decl += "".join("    int64_t %s%s\n" % (f, sig) for f in fns)
decl += "#else\n"
decl += "".join("    #define %s NULL\n" % f for f in ["lzbench_kolmo_init", "lzbench_kolmo_ctl_init", "lzbench_kolmo_deinit"] + fns)
decl += "#endif\n\n"
i = s.rindex("#endif // LZBENCH_COMPRESSORS_H")
codecs.write_text(s[:i] + decl + s[i:])

hdr = lzb / "bench/lzbench.h"
lines = [l for l in hdr.read_text().split("\n") if '{ "kolmo",' not in l and '{ "kolmo_ctl",' not in l]
k = next(i for i, l in enumerate(lines) if "lzbench_kanzi_compress" in l)
row = '    { "%s", "%s %s", "structure transforms + LZ/BWT/CM", 0, 17, 0, NO_THREADING, %s, %s, %s, lzbench_kolmo_deinit },'
lines.insert(k + 1, row % ("kolmo_ctl", "kolmo_ctl", version, "lzbench_kolmo_ctl_compress", "lzbench_kolmo_ctl_decompress", "lzbench_kolmo_ctl_init"))
lines.insert(k + 1, row % ("kolmo", "kolmo", version, "lzbench_kolmo_compress", "lzbench_kolmo_decompress", "lzbench_kolmo_init"))
k = max(i for i, l in enumerate(lines) if l.strip().startswith('{ "kanzi",') and "lzbench_" not in l)
algs = [(0, 0, "transforms + LZ4"), (1, 3, "quick transforms + zstd"), (4, 4, "transforms + zstd"),
        (5, 6, "transforms + zstd (high)"), (7, 7, "transforms + BWT"), (8, 8, "transforms + brotli"),
        (9, 9, "transforms + zstd (high)"), (10, 10, "transforms + BWT or LZMA2"), (11, 13, "transforms + CM"),
        (14, 14, "transforms + zstd, plane-context leaves"), (15, 15, "transforms + zstd, typed Huffman"),
        (16, 16, "minimal transforms + LZ4"), (17, 17, "minimal transforms + zstd")]
for name in ("kolmo_ctl", "kolmo"):
    for lo, hi, alg in reversed(algs):
        lines.insert(k + 1, '    { "%s",    %d,  %d, "%s" },' % (name, lo, hi, alg))
hdr.write_text("\n".join(lines))
print("registered kolmo and kolmo_ctl (%s) in %s" % (version, lzb))
PY

# 4. Build lzbench with its default flags (optionally only some codecs, e.g. "KOLMO ZSTD XZ").
if [ -n "$ONLY" ]; then
    make -C "$LZB" -j"$ncpu" BUILD_ONLY="$ONLY"
else
    make -C "$LZB" -j"$ncpu"
fi
echo "done: $LZB/lzbench  (try: $LZB/lzbench -ekolmo,4/kolmo_ctl,4/zstd,3 file)"
