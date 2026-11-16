#!/bin/bash
# Makes seed frames with build/kolmo (and the research CLIs) from the raw inputs in $RAW, into $SEEDS/<target>.
# Usage: RAW=dir SEEDS=dir fuzz/campaign/make_seeds.sh   (run from the repo root after `make ARCH= build/kolmo`
# and `make ARCH= build/krc-cpu build/kbc build/kbx build/kolmo-fastq`)
set -u
K=$PWD/build/kolmo; DICT=$PWD/fuzz/campaign/dict.bin
mkdir -p "$SEEDS"/{kolmo,stream,dict,krc,kbcx,fastq}
OPTS=("-m speed" "-m rt" "-m balanced" "-q" "-Q" "-n" "-m eco" "-m hc" "-m ratio" "-m max -l 1" "-m max -l 10"
      "-x" "-x -l 1" "-x -l 2" "--native-leaf" "-T 1" "-B 0.0625")
N=$(nproc)
for f in "$RAW"/*; do
  list=("${OPTS[@]}"); case "$f" in *.npy|*.wav) list+=("-m fast -T 1 --typed-huf");; esac
  for o in "${list[@]}"; do
    tag=$(printf "%s" "$o" | tr -d ' -' | tr '.' '_'); out="$SEEDS/kolmo/${tag}__$(basename "$f").kolmo"
    ( timeout 300 $K $o "$f" "$out" 2>/dev/null || rm -f "$out" ) &
    while [ "$(jobs -rp | wc -l)" -ge "$N" ]; do wait -n; done
  done
done; wait
# streaming frames (CZNS), including one multi-segment stream
for f in "$RAW"/{prose_4k.txt,csv_8k.csv,int32_16k.bin,empty.bin,elf_16k.bin,img_rgb.ppm} "$BIG"; do
  for o in "-s" "-S 1" "-s -m rt" "-s -m hc"; do tag=$(printf "%s" "$o" | tr -d ' -'); timeout 300 $K $o "$f" "$SEEDS/stream/${tag}__$(basename "$f").czns" 2>/dev/null; done
done
# dictionary frames
for f in "$RAW"/{json_8k.json,log_16k.log,csv_8k.csv,small_100.txt,prose_4k.txt,one.bin}; do
  for o in "-m balanced" "-m hc" "-m rt" "-m eco" "-m ratio" "-T 1"; do tag=$(printf "%s" "$o" | tr -d ' -')
    timeout 300 $K -D "$DICT" $o "$f" "$SEEDS/dict/D${tag}__$(basename "$f").kolmo" 2>/dev/null; done
done
# research decoders: frames from their own command-line tools
for f in "$RAW"/*; do b=$(basename "$f")
  timeout 300 build/krc-cpu "$f" "$SEEDS/krc/$b.krc" >/dev/null 2>&1 || rm -f "$SEEDS/krc/$b.krc"
  timeout 300 build/kbc "$f" "$SEEDS/kbcx/$b.kbc" >/dev/null 2>&1 || rm -f "$SEEDS/kbcx/$b.kbc"
  for l in 0 1 2; do timeout 300 build/kbx -l $l "$f" "$SEEDS/kbcx/$b.l$l.kbx" >/dev/null 2>&1 || rm -f "$SEEDS/kbcx/$b.l$l.kbx"; done
  timeout 300 build/kolmo-fastq "$f" "$SEEDS/fastq/$b.kfq" >/dev/null 2>&1 || rm -f "$SEEDS/fastq/$b.kfq"
done
true
