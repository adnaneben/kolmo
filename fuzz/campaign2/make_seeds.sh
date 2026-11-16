#!/bin/bash
# Campaign 2 seeds (run from the repo root after `make ARCH= build/kolmo` and MODE=cov fuzz/campaign/build.sh).
# Writes raw inputs to $WORK/raw*, frames to $WORK/s*, and the per-target seed directories to $SEEDS/<target>.
# The selection step keeps, for every target decoder function, the 3 smallest frames that enter it (per-seed coverage
# replay with build/fuzzcov/fuzz_decompress_nocm, seedgen/funchk.sh).
set -eu
WORK=${WORK:?}; SEEDS=${SEEDS:?}; G=fuzz/campaign2/seedgen; K=$PWD/build/kolmo; C=fuzz/campaign/corpus
mkdir -p $WORK/raw $WORK/s $WORK/xr $WORK/xs $WORK/st $WORK/dct $WORK/rt $WORK/fc
python3 $G/gen1_tables_logs_images.py $WORK/raw; python3 $G/gen2_img12.py $WORK/raw; python3 $G/gen3_wide_records.py $WORK/raw
python3 $G/gen4_ragged_ints.py $WORK/raw; python3 $G/gen5_int_spikes.py $WORK/raw
cp /usr/bin/nasm $WORK/raw/x86_nasm.elf; cp /usr/bin/strace $WORK/raw/x86_strace.elf; cp /usr/bin/tmux $WORK/raw/x86_tmux.elf
N=$(nproc) $G/compress_all.sh $WORK/raw $WORK/s            # -m balanced|ratio|speed|hc|max|eco and -q
python3 $G/gen6_misc.py $WORK/raw $WORK/raw $WORK/xr $WORK/st $WORK/s/mbalanced__tbl_300k.csv.kolmo
N=$(nproc) $G/compress_all.sh $WORK/raw $WORK/s
# tflag-3 (version-2 table) frames: no encoder writes them; craft them from table frames
clang -O1 -w -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 -Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc \
  -Ivendor/brotli/include fuzz/campaign2/craft_tbl3.c build/libvendor.a -llzma -lpthread -lm -lstdc++ -o $WORK/craft_tbl3
for f in tbl_300k.csv tbl_1m.csv tbl_q_2m.csv tbl_4m.tsv ragged_1m.csv quotedc_1m.csv; do for m in mbalanced mhc; do
  $WORK/craft_tbl3 $WORK/s/${m}__$f.kolmo $WORK/raw/$f $WORK/s/t3_${m}__$f.kolmo || true; done; done
python3 - $WORK/s <<'PY'     # legacy golden frames (tests/legacy_frames.h)
import re, sys
s = open('tests/legacy_frames.h').read()
for m in re.finditer(r'static const uint8_t (\w+)\[\] = \{(.*?)\};', s, re.S):
    open('%s/%s.kolmo' % (sys.argv[1], m.group(1)), 'wb').write(bytes(int(x, 16) for x in re.findall(r'0x[0-9a-fA-F]+', m.group(2))))
PY
$G/funchk.sh build/fuzzcov/fuzz_decompress_nocm $WORK/s $WORK/fc
mkdir -p $WORK/pick; python3 - $WORK <<'PY'
import os, glob, shutil, collections, sys
W = sys.argv[1]; hits = collections.defaultdict(list)
for f in glob.glob(W + '/fc/p/*.funcs'):
    seed = W + '/s/' + os.path.basename(f)[:-6]
    for fn in open(f).read().split(): hits[fn].append((os.path.getsize(seed), seed))
for fn, l in sorted(hits.items()):
    for sz, s in sorted(l)[:3]: shutil.copy(s, W + '/pick/')
PY
# -x seeds (context-mixing targets), dictionary frames, streams, round-trip inputs
for f in $WORK/xr/*; do for o in "-x" "-x -l 1" "-x -l 2"; do $K $o $f $WORK/xs/$(printf "%s" "$o" | tr -d ' -')__$(basename $f).kolmo; done; done
for f in tbl_300k.csv wide300_200k.csv fixed_500k.txt access_600k.log w4o2_s.bin w4o8_u2.bin; do for o in "-m balanced" "-m hc" "-m ratio"; do
  $K -D fuzz/campaign/dict.bin $o $WORK/raw/$f $WORK/dct/D$(printf "%s" "$o" | tr -d ' -')__$f.kolmo; done; done
for f in tbl_1m.csv x86_tmux.elf raw10le_noh.bin wide300_1m.csv recq12_600k.bin ragged_1m.csv w4o4_s.bin fixed_500k.txt; do
  for o in "-s" "-S 1" "-s -m hc"; do $K $o $WORK/raw/$f $WORK/st/$(printf "%s" "$o" | tr -d ' -')__$f.czns; done; done
for spec in "tbl_1m.csv 300000" "access_600k.log 600000" "x86_tmux.elf 1000000" "raw10le_noh.bin 614400" "recq12_600k.bin 600000" \
    "wide300_200k.csv 155830" "w4o2_s.bin 600000" "fixed_500k.txt 440000" "ragged_1m.csv 600000" "lines_2m.sql 600000" "ndjson_3m.json 600000" \
    "syslog_3m.log 600000" "app_3m.log 600000" "widetext_tab.tsv 300000" "img12_640x480.pgm 614416" "quotedc_1m.csv 400000"; do
  set -- $spec; for p in 1 3 4 17 41 9; do python3 -c "import sys; d=open('$WORK/raw/$1','rb').read()[:$2]; open('$WORK/rt/p${p}__$1','wb').write(bytes([$p])+d)"; done
done
# per-target seed directories
for t in decompress limited; do mkdir -p $SEEDS/fuzz_${t}_nocm; cp $C/fuzz_$t/* $WORK/pick/* $SEEDS/fuzz_${t}_nocm/; done
mkdir -p $SEEDS/fuzz_dict_nocm; cp $C/fuzz_dict/* $WORK/dct/* $SEEDS/fuzz_dict_nocm/; cp $WORK/pick/mbalanced__* $SEEDS/fuzz_dict_nocm/
for t in stream stream_limited; do mkdir -p $SEEDS/fuzz_${t}_nocm; cp $C/fuzz_stream/* $WORK/st/* $SEEDS/fuzz_${t}_nocm/; done
mkdir -p $SEEDS/fuzz_roundtrip_nocm; cp $C/fuzz_roundtrip/* $WORK/rt/* $SEEDS/fuzz_roundtrip_nocm/
for t in roundtrip nplane krc kbcx fastq; do mkdir -p $SEEDS/fuzz_$t; cp $C/fuzz_$t/* $SEEDS/fuzz_$t/; done
# context-mixing targets: up to 60 CM frames of at most 8 KiB from campaign 1's corpus, the -x seeds, minus those
# slower than 4 s per run under ASan
python3 - $SEEDS $WORK <<'PY'
import os, glob, shutil, struct, random, sys
S, W = sys.argv[1:3]
def is_cm(b):
    if len(b) < 32 or b[:4] != b'CZN1': return False
    nb = struct.unpack('<I', b[20:24])[0]
    return 32 + 16 * nb <= len(b) and nb < 100000 and any(b[32 + 16 * i + 12] in (7, 17) for i in range(nb))
for t in ['decompress', 'limited', 'dict']:
    d = '%s/fuzz_%s' % (S, t); os.makedirs(d, exist_ok=True)
    c = sorted(f for f in glob.glob('fuzz/campaign/corpus/fuzz_%s/*' % t) if os.path.getsize(f) <= 8192 and is_cm(open(f, 'rb').read()))
    random.Random(1).shuffle(c)
    for f in c[:60] + glob.glob(W + '/xs/*'): shutil.copy(f, d)
PY
for t in decompress limited dict; do $G/filter_slow.sh build/fuzz/fuzz_$t $SEEDS/fuzz_$t $WORK/slow_$t 4; done
for t in decompress limited dict; do cp $WORK/xs/x__img_128x128_1.pgm.kolmo $SEEDS/fuzz_$t/ 2>/dev/null || true; done   # CMI (10 s, kept)
