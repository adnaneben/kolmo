#!/bin/sh
# Re-run tonight's correctness evidence for the P2 type-first work (research only; local, no network).
# usage: sh research/codec/P2-SUMMARY/check.sh CORPUS_DIR   (the E02C development corpus, hashes in its manifest)
# Builds sanitizer binaries and checks: identity with production (choice, program, inverse, payload), differential
# fuzzing of the inverse and of the predictor choice, container mutation, CSV typing vs an independent oracle, and
# production-CLI decoding of every production-format frame. Exits non-zero on the first failure.
set -eu
C=${1:?corpus dir}; R=research/codec; B=build; T=$(mktemp -d)
DEFS="-DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0"; INC="-Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include"
SAN="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined -ffp-contract=off"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
[ -f $B/libvendor.a ] || make $B/libvendor.a >/dev/null
[ -f $B/kolmo ] || make $B/kolmo >/dev/null
for x in "P2-E2/fuzz.c fuzz" "P2-E9/diffchoose.c diffchoose" "P2-E11/p2e11.c p2e11" "P2-E11/tpcli.c tpcli" "P2-E6/p2e6.c p2e6"; do
  set -- $x; gcc $SAN $DEFS $INC $R/$1 $B/libvendor.a -o $T/$2 -llzma -lpthread -lm -lstdc++ 2>/dev/null
done
python3 -c "import json,hashlib,sys,os
m=json.load(open('$R/E02C-harness/corpus/manifest.json'))
bad=[f['name'] for f in m['inputs'] if hashlib.sha256(open(os.path.join('$C',f['name']),'rb').read()).hexdigest()!=f['sha256']]
sys.exit('corpus hash mismatch: %s'%bad if bad else 0)"
echo "1/6 identity with production (choice, program, inverse, kg_encode_num payload) under sanitizers"
for f in "$C"/*; do $T/p2e11 run "$f" "$(basename "$f")" 1 1 > $T/id.jsonl; done
echo "2/6 differential inverse fuzzing (fi_inverse vs kg_int_inv)"; $T/fuzz inverse 300000 1
echo "3/6 container mutation"; $T/fuzz container "$C/goog_date_i64.npy" tf_planes_huf 20000 7
echo "4/6 predictor choice vs production on random arrays"; $T/diffchoose 20000 1
echo "5/6 CSV typing vs the independent oracle"
python3 $R/P2-E6/gen_edge.py $T/edge >/dev/null; python3 $R/P2-E6/gen_swar.py $T/swar >/dev/null
for f in $T/edge/*.csv $T/swar/*.csv; do a=$($T/p2e6 types "$f" x); b=$(python3 $R/P2-E6/oracle.py "$f"); [ "$a" = "$b" ] || { echo "typing mismatch $f: $a vs $b"; exit 1; }
  $T/p2e6 mem "$f" mcsv_planes_huf >/dev/null; done
echo "6/6 production-format frames decoded by the production kolmo CLI"
for f in "$C"/*; do for c in tp_prod3 tp_phuf tp_phuf_pg16 tp_table3; do
  $T/tpcli -p $c -T1 "$f" $T/x.czn; $B/kolmo -d -T1 $T/x.czn $T/y; cmp -s "$f" $T/y || { echo "CLI decode mismatch $f $c"; exit 1; }; done; done
rm -rf "$T"; echo "all P2 correctness checks passed"
