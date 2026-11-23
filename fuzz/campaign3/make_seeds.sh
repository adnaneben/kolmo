#!/bin/bash
# Campaign 3 seeds. Usage (repo root, after `make build/kolmo` and fuzz/campaign3/build.sh):
#   W=workdir fuzz/campaign3/make_seeds.sh      -> $W/seeds/<target>/
# Raw inputs come from seedgen/gen_inputs.py (fixed seeds); frames are written by build/kolmo in every mode and route
# rc3 adds; codec payloads are cut out of those frames by seedgen/codec_seeds.c. Campaign 2's corpora (rc2 frames, which
# rc3 still reads) can be added as a second corpus directory when running.
set -u
W=${W:?}; K=$PWD/build/kolmo; R=$W/raw; S=$W/seeds; F=$W/frames
mkdir -p "$R" "$S" "$F"
python3 -I fuzz/campaign3/seedgen/gen_inputs.py "$R"
for t in fuzz_decompress fuzz_limited fuzz_stream fuzz_stream_limited fuzz_roundtrip fuzz_codecs fuzz_ext fuzz_profile fuzz_nplane; do mkdir -p "$S/$t"; done
# slices: the first 48 KiB of each input for the round trips, and 8 KiB for the slow modes (context mixing, GR6+, audio):
# under ASan+UBSan with comparison tracing a 48 KiB GR6+ or audio frame takes about 3 s per fuzz input
mkdir -p "$W/slices" "$W/small"
for f in "$R"/*.*; do b=$(basename "$f"); head -c 49152 "$f" > "$W/slices/$b"; head -c 8192 "$f" > "$W/small/$b"; done
modes=("-1" "-1 --analysis full" "" "--analysis quick" "-5" "-7" "-9" "-8" "--native-leaf")   # 0.0.2 levels (the 0.0.1 modes)
xmodes=("-U --codec-level 1" "-U" "-U --codec-level 2")
for f in "$R"/*.*; do
  b=$(basename "$f"); i=0
  for m in "${modes[@]}"; do $K $m -f -T 1 "$f" -o "$F/$b.m$i.kol" 2>/dev/null; i=$((i+1)); done
  # small slices only at -x (context-mixing seeds stay small; the CM decode is slow under ASan)
  i=0; for m in "${xmodes[@]}"; do $K $m -f -T 1 "$W/small/$b" -o "$F/$b.x$i.kol" 2>/dev/null; i=$((i+1)); done
  # long-form tables and line records also at -x whole (table slack at -x, codec 25 parts, GR6+)
  case $b in longform.csv|table.csv|records.txt|semi.txt|wide.csv|photo.pgm|fastq.fq) $K -U -f -T 2 "$f" -o "$F/$b.xfull.kol" 2>/dev/null;; esac
  # audio (codec 19) only from the small WAV files; the large ones go to the round trips
  case $b in mono.wav|stereo.wav) rm -f "$F/$b".*.kol;; esac
done
$K -3 --typed-huf -f -T 1 "$R/mono.wav" -o "$F/mono.wav.typed.kol" 2>/dev/null
# frames: at most 2 MiB each
for f in "$F"/*.kol; do [ "$(stat -c %s "$f")" -le 2097152 ] && cp "$f" "$S/fuzz_decompress/"; done
cp "$S"/fuzz_decompress/* "$S/fuzz_limited/"
# streams
for f in "$R"/table.csv "$R"/app.log "$R"/text.txt "$W"/slices/*.wav; do
  b=$(basename "$f"); $K --stream -f "$f" -o "$S/fuzz_stream/$b.czs" 2>/dev/null; $K --stream=1 -7 -f "$f" -o "$S/fuzz_stream/$b.s1.czs" 2>/dev/null
done
cp "$S"/fuzz_stream/* "$S/fuzz_stream_limited/"
# round trips: mode byte + slice (mode byte bits: see fuzz/fuzz_roundtrip.c)
for f in "$W"/slices/*; do b=$(basename "$f"); for mb in 0 1 2 3 4 9 17 33; do { printf "\\$(printf %o $mb)"; head -c 32768 "$f"; } > "$S/fuzz_roundtrip/$b.$mb"; done; done
# codecs: payloads cut out of the frames, Y4M layouts, and round-trip seeds (sel | 0x80)
cc -O1 -g -Isrc \
  fuzz/campaign3/seedgen/codec_seeds.c build/libkolmoarch.a -lpthread -lm -o "$W/codec_seeds"
"$W/codec_seeds" "$S/fuzz_codecs" "$F"/*.kol "$R"/*.y4m
for f in "$S"/fuzz_codecs/*_nested; do mv "$f" "$S/fuzz_decompress/"; done 2>/dev/null
for f in "$W"/small/fastq.fq "$W"/small/text.txt "$W"/small/records.txt "$W"/small/sparse.u16 "$W"/small/walk.i32 "$W"/small/mono.wav "$W"/small/stereo.wav "$W"/small/semi.txt; do
  b=$(basename "$f")
  for sel in 128 129 131 132 133 135 248; do for fl in 0 2 18 35 66; do { printf "\\$(printf %o $sel)\\0\\0\\0\\$(printf %o $fl)"; cat "$f"; } > "$S/fuzz_codecs/rt_${b}_${sel}_$fl"; done; done
done
# extensions: indexed files (tables, logs, JSON lines; with and without filters) and archives
for f in "$R"/table.csv "$R"/app.log "$R"/events.jsonl "$R"/records.txt "$R"/table.tsv; do
  b=$(basename "$f"); head -c 200000 "$f" > "$W/ix.$b"
  $K --index --block-size 0.0625 "$W/ix.$b" "$W/ix.$b.kol" 2>/dev/null && for q in 0 5 8 22 38 70 72 132; do { printf "\\0\\$(printf %o $q)"; cat "$W/ix.$b.kol"; } > "$S/fuzz_ext/$b.$q"; done
  $K --index -7 --block-size 0.0625 "$W/ix.$b" "$W/ix.$b.hc.kol" 2>/dev/null && { printf '\001\026'; cat "$W/ix.$b.hc.kol"; } > "$S/fuzz_ext/$b.hc"
done
( cd "$R/set" && $K archive --chunk 1 "$W/arc1.kol" snap0.csv snap1.csv snap2.csv && $K archive "$W/arc2.kol" snap3.csv snap4.csv snap5.csv ../small.txt ../empty.bin ) 2>/dev/null
for a in arc1 arc2; do for q in 0 1 8 129; do { printf "\\0\\$(printf %o $q)"; cat "$W/$a.kol"; } > "$S/fuzz_ext/$a.$q"; done; done
# profiles: trained on the snapshot set (balanced and hc), with frames written with them (CZN1 and compact CZC1).
# kolmo train and -P were removed (0.0.2; not available since 0.0.1), so this loop writes nothing; kept for the history.
for m in fast hc; do
  $K train -m $m -o "$W/p.$m.kpf" "$R/set" 2>/dev/null || continue
  L=$(stat -c %s "$W/p.$m.kpf")
  for x in "$R/set/snap1.csv" "$R/small.txt" "$R/table.csv"; do
    $K -P "$W/p.$m.kpf" "$x" "$W/pf.kol" 2>/dev/null || continue
    for sel in 0 1 3 8; do { printf "\\$(printf %o $sel)\\$(printf %o $((L & 255)))\\$(printf %o $((L >> 8 & 255)))"; cat "$W/p.$m.kpf" "$W/pf.kol"; } > "$S/fuzz_profile/$m.$(basename "$x").$sel"; done
  done
done
printf '\004size' > "$S/fuzz_profile/t1"; printf '\004decode>=1GB/s,encode>=50MB/s' > "$S/fuzz_profile/t2"
printf '\004cost:size/1e9*0.023 + (enc + 3.3*dec)*0.054/3600\0kolmo-calibration 1\nbalanced text 300 900\nratio table 20 60 1 2\n' > "$S/fuzz_profile/t3"
printf '\004cost:cloud=12' > "$S/fuzz_profile/t4"; printf '\004cost:link=1Gbit' > "$S/fuzz_profile/t5"; printf '\004cost:-(n/(size-enc))*((dec))' > "$S/fuzz_profile/t6"
# nplane: odd selector = round trip
for f in "$W"/slices/walk.i32 "$W"/slices/stamps.i64 "$W"/slices/sparse.u16; do for sel in 1 3 7 15; do { printf "\\$(printf %o $sel)"; head -c 16384 "$f"; } > "$S/fuzz_nplane/$(basename "$f").$sel"; done; done
# drop seeds slower than $SLOW_MS (default 600) per input in the sanitizer build (listed in $W/slow_seeds.txt)
SLOW_MS=${SLOW_MS:-600}; : > "$W/slow_seeds.txt"
for t in "$S"/*; do
  bin=$PWD/build/fuzz/$(basename "$t")_nocm; [ -x "$bin" ] || bin=$PWD/build/fuzz/$(basename "$t"); [ -x "$bin" ] || continue
  for f in "$t"/*; do
    s0=$(date +%s%N); timeout 60 "$bin" -runs=1 "$f" > /dev/null 2>&1; ms=$(( ($(date +%s%N) - s0) / 1000000 ))
    [ $ms -gt "$SLOW_MS" ] && { echo "$ms $(basename "$t")/$(basename "$f")" >> "$W/slow_seeds.txt"; rm -f "$f"; }
  done
done
for t in "$S"/*; do echo "$(basename "$t"): $(ls "$t" | wc -l) seeds, $(du -sh "$t" | cut -f1)"; done
