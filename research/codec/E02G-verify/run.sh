#!/bin/sh
# E02G-verify (PROTOCOL.md). usage, from the repository root: research/codec/E02G-verify/run.sh DOWNLOAD_DIR OUT_DIR
set -eu
dl=$1; out=$2; v=research/codec/E02G-verify
[ ! -e "$out" ] || { echo "OUT_DIR must be new" >&2; exit 2; }
(cd research/codec/E02C-harness/harness && sha256sum -c ../../E02G/FROZEN.sha256) || { echo "harness differs from the frozen registration" >&2; exit 2; }
git diff --quiet 2f3308a3380a140ad0f6873ae228de7715e5eef5 -- src vendor Makefile tests benchmark/accept/TARGETS.md || { echo "production files changed" >&2; exit 2; }
mkdir -p "$out"
python3 -I research/codec/E02G/prepare.py "$dl" "$out/inputs" "$out/manifest.json"
make ARCH= build/libvendor.a
F="-ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include"
gcc -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined $F $v/dtime.c build/libvendor.a -o build/dtime-asan -llzma -lpthread -lm -lstdc++
gcc -O3 -march=native -fno-plt $F $v/dtime.c build/libvendor.a -o build/dtime -llzma -lpthread -lm -lstdc++
for f in "$out"/inputs/*.npy; do ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ./build/dtime-asan "$f" x fast 1 1 > /dev/null; done   # correctness pass, not timed
python3 -I - "$out/manifest.json" > "$out/order.txt" <<'PY'
import json, random, sys
m = json.load(open(sys.argv[1])); jobs = [(i['name'], mode) for i in m['inputs'] for mode in ('fast', 'hc')]
random.Random(20261118).shuffle(jobs); print('\n'.join(f'{n} {md}' for n, md in jobs))
PY
while read -r name mode; do ./build/dtime "$out/inputs/$name" "$name" "$mode" 15 20261118 >> "$out/dtime.jsonl"; done < "$out/order.txt"
python3 -I $v/verify.py "$out/dtime.jsonl" "$out/manifest.json" "$out/verify.json"
