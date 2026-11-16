#!/bin/bash
# Builds the stacking research tool (research/STACK-AUDIT.md, section 6) into OUT, from a repository checkout.
#   research/stack-audit/build.sh OUT        (run from the repository root after `make build/libvendor.a`)
# It copies src/ to OUT/src and adds two reporting hooks there (graph leaves in kg_decode, transformed blocks in
# decode_block); the repository's own sources are not changed. Not a Kolmo format: nothing it writes is kept.
set -e
R=$(pwd); OUT=${1:?usage: build.sh OUT}; H=$R/research/stack-audit
mkdir -p "$OUT"; rm -rf "$OUT/src"; cp -R "$R/src" "$OUT/src"
python3 -I - "$OUT/src" <<'EOF'
import sys, pathlib
d = pathlib.Path(sys.argv[1])
k = d / 'kgraph.inc'; s = k.read_text()
old = '        s[id].n = (size_t)rl; have[id] = 1; i += zl;'
new = '''        s[id].n = (size_t)rl; have[id] = 1;
        { int op = 0, oi = 0, no = 0; const uint8_t *pp = NULL; size_t pn = 0;      /* stacking tool: report the leaf */
          for (uint64_t r = 0; r < nn; r++) if (id >= nd[r].firstout && id < (uint64_t)nd[r].firstout + nd[r].nout) { op = nd[r].op; oi = (int)(id - nd[r].firstout); no = (int)nd[r].nout; pp = nd[r].par; pn = nd[r].plen; }
          kstk_leaf(op, oi, no, pp, pn, cc, s[id].p, (size_t)rl, (size_t)zl); }
        i += zl;'''
assert s.count(old) == 1; s = s.replace(old, new)
k.write_text('static void kstk_leaf(int op, int oi, int no, const uint8_t *par, size_t plen, int cc, const uint8_t *d, size_t rl, size_t zl);\n' + s)
c = d / 'kolmo.c'; s = c.read_text()
old = '    if (codec_decompress(x, codec, pay + meta, cl - meta, tgt, dlen)) return KOLMO_ERR_CORRUPT;'
assert s.count(old) == 1
s = s.replace(old, old + '\n    kstk_block(codec, tflag, pl.t, tgt, dlen, cl - meta);                /* stacking tool: report the block */')
s = s.replace('#include "kgraph.inc"', 'static void kstk_block(int codec, int tflag, int pt, const uint8_t *d, size_t n, size_t zl);\n#include "kgraph.inc"', 1)
c.write_text(s)
EOF
BREW=$(brew --prefix 2>/dev/null || echo /usr/local)
DEFS="-DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0"; [ "$(uname -m)" = x86_64 ] || DEFS="$DEFS -DZSTD_DISABLE_ASM"
INC="-I$OUT/src -I$R/vendor/zstd -I$R/vendor/lz4 -I$R/vendor/libbsc -I$R/vendor/brotli/include -I$BREW/include"
LZMA=$(ls $BREW/lib/liblzma.a 2>/dev/null || echo -llzma); CXX=$([ "$(uname -s)" = Darwin ] && echo -lc++ || echo -lstdc++)
cc -O3 -ffp-contract=off -I$R/src/experimental -c $H/kfam_api.c -o $OUT/kfam_api.o
cc -O3 -ffp-contract=off $DEFS $INC -c $H/stackbench.c -o $OUT/stackbench.o
cc -O3 $OUT/stackbench.o $OUT/kfam_api.o $R/build/libvendor.a $LZMA -lpthread -lm $CXX -o $OUT/stackbench
[ -f "$R/build/libkolmo.a" ] && cc -O2 -I$R/src -I$R/src/experimental $H/kvidlayout.c $R/build/libkolmo.a $R/build/libvendor.a $LZMA -lpthread -lm $CXX -o $OUT/kvidlayout
echo "built $OUT/stackbench $OUT/kvidlayout"
