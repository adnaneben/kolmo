#!/bin/sh
# Wave-4 LZ lane component runs (results/*.tsv). usage: sh benchmark/w4/lz/run_all.sh DATA_DIR [speed balanced ratio]
# DATA_DIR holds silesia/, struct/ and enwik/ (benchmark/w4/lz/RESULTS.md lists the files).
D=$1; shift; ROLES=${*:-"speed balanced ratio"}; R=benchmark/w4/lz/results; P="python3 benchmark/w4/lz/perfile.py build/natbench"
F=$(ls $D/silesia/* $D/struct/* $D/enwik/* | tr '\n' ' ')
S="$D/silesia/dickens $D/silesia/osdb $D/silesia/ooffice $D/silesia/x-ray $D/silesia/samba $D/struct/owid-co2-data.csv $D/struct/logs.log"
for role in $ROLES; do for T in 1 4; do
  case $role in
    speed) $P $R/speed.tsv 4096 5 $T lz4:klz1 -- $F ;;
    balanced) $P $R/balanced.tsv 4096 3 $T zstd1:klzh1 zstd3:klzh3 -- $F ;;
    ratio) $P $R/ratio.tsv 16384 2 $T lzma9e:klzma8 -- $S ;;
  esac
done; done
echo DONE
