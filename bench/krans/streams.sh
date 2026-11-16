#!/bin/sh
# Silesia inputs for the KRANS benchmark: <out>/raw/<file> (the corpus files), <out>/lo/<file> and <out>/hi/<file>
# (low and high byte planes of x[i] - x[i-1] over 16-bit little-endian words; an odd last byte is dropped).
# usage: bench/krans/streams.sh <silesia dir> <out dir>    (needs build/krans)
set -eu
S=${1:?silesia dir}; O=${2:?out dir}; K=$(dirname "$0")/../../build/krans
mkdir -p "$O/raw" "$O/lo" "$O/hi"
( cd "$S" && sha256sum -c "$OLDPWD/$(dirname "$0")/silesia.sha256" >/dev/null ) || { echo "silesia hashes differ"; exit 1; }
for f in dickens mozilla mr nci ooffice osdb reymont samba sao webster x-ray xml; do
  cp "$S/$f" "$O/raw/$f"
  "$K" -P "$S/$f" "$O/tmp"; mv "$O/tmp.lo" "$O/lo/$f"; mv "$O/tmp.hi" "$O/hi/$f"
done
