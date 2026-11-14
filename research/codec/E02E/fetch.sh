#!/bin/sh
# Download the registered E02E publisher files into DIR (needs network; nothing else is fetched). usage: fetch.sh DIR
set -eu
d=$1; mkdir -p "$d"
for s in USW00094728 USW00023174 USW00014739; do curl -fsSL --retry 3 -o "$d/$s.dly" "https://www.ncei.noaa.gov/pub/data/ghcn/daily/all/$s.dly"; done
for r in 100 101 200 201; do for e in hea dat; do curl -fsSL --retry 3 -o "$d/$r.$e" "https://physionet.org/files/mitdb/1.0.0/$r.$e"; done; done
(cd "$d" && sha256sum *) > "$d/DOWNLOAD.sha256"; cat "$d/DOWNLOAD.sha256"
