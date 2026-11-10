#!/bin/sh
# Add kolmo to an lzbench checkout as an ordinary codec, then build lzbench.
#
#   git clone https://github.com/inikep/lzbench && sh benchmark/lzbench/setup.sh ./lzbench
#   ./lzbench/lzbench -ekolmo/zstd,1,19/xz,6,9 -t0,0 silesia.tar
#
# Fairness rules this follows:
#  - kolmo runs single-threaded (threads = 1) with its default block size and transforms.
#  - kolmo is built at -O3 -march=native; lzbench builds every other codec with the same
#    MOREFLAGS="-march=native" on top of its own -O3, so all entries get the same compiler options.
#  - kolmo's core, its vendored codecs and a static liblzma are partially linked into one object whose
#    only global symbols are kolmo_*, so kolmo never shares code with another entry and no other entry
#    uses kolmo's copies of zstd/brotli/lz4/bsc/xz.
# Linux (GNU ld + objcopy). The build needs a static liblzma (liblzma-dev provides liblzma.a).
set -e
LZB=${1:?usage: setup.sh <path-to-lzbench-checkout> [BUILD_ONLY codec list]}
ONLY=${2:-}
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
LZB=$(cd "$LZB" && pwd)

# 1. kolmo, built with its normal flags (-O3 -march=native).
make -C "$root" -j"$(nproc)" build/libkolmo.a build/libvendor.a >/dev/null

# 2. One isolated object: kolmo + vendored codecs + static liblzma, only kolmo_* left global.
lzma_a=""
for d in /usr/lib/x86_64-linux-gnu /usr/lib/aarch64-linux-gnu /usr/lib64 /usr/lib /usr/local/lib; do
    [ -f "$d/liblzma.a" ] && lzma_a="$d/liblzma.a" && break
done
[ -n "$lzma_a" ] || { echo "setup.sh: need a static liblzma (apt install liblzma-dev)"; exit 1; }
work=$(mktemp -d)
ld -r --whole-archive "$root/build/libkolmo.a" "$root/build/libvendor.a" --no-whole-archive "$lzma_a" \
   -o "$work/kolmo_all.o"
printf 'kolmo_compress\nkolmo_decompress\nkolmo_bound\nkolmo_original_size\nkolmo_strerror\n' > "$work/keep.txt"
# Rename every C++ symbol kolmo defines (libbsc's inline functions, templates). Their COMDAT group
# signatures carry the same names as lzbench's own libbsc; without the rename the linker keeps
# lzbench's copy of each group and discards kolmo's, leaving kolmo's localized references dangling.
nm --defined-only "$work/kolmo_all.o" | awk 'NF==3 && $3 ~ /^_Z/ {print $3" kolmo_"$3}' | sort -u > "$work/rename.txt"
objcopy --redefine-syms="$work/rename.txt" "$work/kolmo_all.o" "$work/kolmo_ren.o"
objcopy --keep-global-symbols="$work/keep.txt" "$work/kolmo_ren.o" "$work/kolmo_iso.o"

# 3. Drop the codec into lzbench.
mkdir -p "$LZB/misc/kolmo"
rm -f "$LZB/misc/kolmo/libkolmo_iso.a"
ar rcs "$LZB/misc/kolmo/libkolmo_iso.a" "$work/kolmo_iso.o"
cp "$root/src/kolmo.h" "$here/kolmo_lzbench.cpp" "$LZB/misc/kolmo/"
cp "$here/kolmo.mk" "$LZB/mk/kolmo.mk"
rm -rf "$work"

# 4. Register it (idempotent): declarations, the codec table entry, and per-level algorithm names.
python3 - "$LZB" <<'PY'
import sys, pathlib
lzb = pathlib.Path(sys.argv[1])
codecs = lzb / "bench/codecs.h"
s = codecs.read_text()
if "lzbench_kolmo_compress" not in s:
    decl = '''#ifndef BENCH_REMOVE_KOLMO
    int64_t lzbench_kolmo_compress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options);
    int64_t lzbench_kolmo_decompress(char *inbuf, size_t insize, char *outbuf, size_t outsize, codec_options_t *codec_options);
#else
    #define lzbench_kolmo_compress NULL
    #define lzbench_kolmo_decompress NULL
#endif

'''
    i = s.rindex("#endif // LZBENCH_COMPRESSORS_H")
    codecs.write_text(s[:i] + decl + s[i:])

hdr = lzb / "bench/lzbench.h"
# Drop any earlier kolmo registration, then register the current level ladder (see kolmo_lzbench.cpp).
lines = [l for l in hdr.read_text().split("\n") if '{ "kolmo",' not in l]
k = next(i for i, l in enumerate(lines) if "lzbench_kanzi_compress" in l)
lines.insert(k + 1, '    { "kolmo",      "kolmo 1.1.0",             "structure transforms + LZ/BWT/CM", 0,   12,    0,   NO_THREADING, lzbench_kolmo_compress,      lzbench_kolmo_decompress,      NULL,                    NULL },')
k = max(i for i, l in enumerate(lines) if l.strip().startswith('{ "kanzi",') and "lzbench_" not in l)
for lo, hi, alg in reversed([(0, 0, "transforms + LZ4"), (1, 3, "quick transforms + zstd"), (4, 4, "transforms + zstd"),
                             (5, 6, "transforms + zstd (high)"), (7, 7, "transforms + BWT"), (8, 8, "transforms + brotli"),
                             (9, 9, "transforms + zstd (high)"), (10, 10, "transforms + BWT or LZMA2"), (11, 12, "transforms + CM")]):
    lines.insert(k + 1, f'    {{ "kolmo",    {lo},  {hi}, "{alg}" }},')
hdr.write_text("\n".join(lines))
print("registered kolmo in", lzb)
PY

# 5. Build lzbench (optionally only some codecs, e.g. "KOLMO ZSTD XZ").
if [ -n "$ONLY" ]; then
    make -C "$LZB" -j"$(nproc)" MOREFLAGS="-march=native" BUILD_ONLY="$ONLY"
else
    make -C "$LZB" -j"$(nproc)" MOREFLAGS="-march=native"
fi
echo "done: $LZB/lzbench  (try: $LZB/lzbench -ekolmo silesia.tar)"
