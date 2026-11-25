#!/bin/bash
# Build and run the small-input benchmark against one Kolmo source tree.
# usage: benchmark/small/run.sh KOLMO_TREE OUT.tsv [smallbench options]
# KOLMO_TREE: a checkout of Kolmo. Its src/kolmo.c and every C source of libkolmoarch.a (ARCHSRC: the native coders,
# block sorting, the kernels) are compiled here with the Makefile's flags, their calls to malloc, calloc, realloc and
# free retargeted to the harness's counters (Mach-O symbol names: macOS); assembly sources come from the tree's own
# build/libkolmoarch.a (`make build/libkolmoarch.a` first). Rivals: Homebrew's static zstd, brotli and lz4
# (HB, default /opt/homebrew). Data: the review workspace ($V), six kinds below.
set -euo pipefail
TREE=$(cd "$1" && pwd); OUT=$2; shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
V=${V:-/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD}; HB=${HB:-/opt/homebrew}
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
printf 'smallbench-vars:\n\t@echo "$(ARCHSRC)"\n\t@echo "$(CFLAGS) $(VSECT) $(DEFS) $(INC)"\n' > "$W/vars.mk"
VARS=$(cd "$TREE" && make -s -f Makefile -f "$W/vars.mk" smallbench-vars)
SRCS=$(echo "$VARS" | sed -n 1p); FLAGS=$(echo "$VARS" | sed -n 2p)
retarget() {   # retarget SRC OBJ: compile to assembly, rename the heap calls, assemble
    (cd "$TREE" && cc $FLAGS -S "$1" -o "$W/t.s" 2>/dev/null)
    sed -E 's/([^A-Za-z0-9_])_(malloc|calloc|realloc|free)([^A-Za-z0-9_]|$)/\1_mt_\2\3/g' "$W/t.s" > "$W/t_mt.s"
    cc -c "$W/t_mt.s" -o "$2"
}
OBJS="$W/kolmo.o"; retarget src/kolmo.c "$W/kolmo.o"
i=0; for s in $SRCS; do case $s in *.c) i=$((i + 1)); retarget "$s" "$W/a$i.o"; OBJS="$OBJS $W/a$i.o";; esac; done
cc -O2 -I"$TREE/src" -I"$HB/include" "$HERE/smallbench.c" $OBJS "$TREE/build/libkolmoarch.a" \
   "$HB/lib/libzstd.a" "$HB/lib/libbrotlienc.a" "$HB/lib/libbrotlidec.a" "$HB/lib/libbrotlicommon.a" "$HB/lib/liblz4.a" \
   -lpthread -lm -o "$W/smallbench"
"$W/smallbench" "$@" \
  json="$V/review40b/work/R21/data/gh_test.ndjson:line" \
  log="$V/review40b/work/R21/data/nasa_test.log:line" \
  csv="$V/review40b/work/R21/data/ghcn_test.csv:line" \
  kv="$V/corpus-holdout/osdb:raw" \
  code="$V/corpus-holdout/samba:line" \
  binary="$V/corpus/ooffice:raw" > "$OUT"
