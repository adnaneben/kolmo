#!/bin/sh
# Component benchmark of the native coders against the libraries (native lane, rc3 wave 2).
# usage: sh benchmark/rc3w2/native/run_components.sh SILESIA_DIR STRUCT_DIR OUT_FILE [ROLES]
# ROLES: a subset of "speed balanced huf eco hc ratio" (pass 2 ran "speed balanced huf", then the Huffman leaf chunks:
# build/natbench huf,khuf 1 4096 9 leaf.chunks, chunks dumped with LIBPROF_DUMP_HUF from build/kolmo-libprof -d)
# Each line: role, set, threads, then natbench's line. Blocks: 4 MiB for the speed and balanced roles, 16 MiB for eco,
# hc and ratio (Kolmo uses adaptive blocks for speed/balanced, 16 MiB for eco/hc, up to 128 MiB for ratio).
S=$1; T=$2; O=$3; ROLES=${4:-"speed balanced huf eco hc ratio"}; NB=build/natbench
: > $O
run() { role=$1; codecs=$2; bk=$3; reps=$4
  case " $ROLES " in *" $role "*) ;; *) return ;; esac
  for set in silesia struct; do
    if [ $set = silesia ]; then files=$(ls $S/* ); else files=$(ls $T/*); fi
    for th in 1 4; do $NB $codecs $th $bk $reps $files | sed "s/^/$role $set /" >> $O; done
  done; }
run speed lz4,klz1,klz2 4096 5
run balanced zstd1,klzh1,zstd3,klzh2,klzh3,klzh5 4096 3
run huf huf,khuf 4096 5
run eco brotli8,klzh7,klzma3 16384 2
run hc zstd19,klzh8,klzma6 16384 2
run ratio lzma9e,klzma8 16384 2
echo done >> $O
