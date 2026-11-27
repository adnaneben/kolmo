#!/bin/sh
# Download exactly the registered E02G publisher files into DIR (needs network). usage: fetch.sh DIR
set -eu
d=$1; mkdir -p "$d"
curl -fsSL --retry 3 -o "$d/data_Q1_2023.zip" "https://f001.backblazeb2.com/file/Backblaze-Hard-Drive-Data/data_Q1_2023.zip"
curl -fsSL --retry 3 -o "$d/owid-covid-data.csv" "https://raw.githubusercontent.com/owid/covid-19-data/master/public/data/owid-covid-data.csv"
for s in S005 S006 S007 S008; do curl -fsSL --retry 3 -o "$d/${s}R01.edf" "https://physionet.org/files/eegmmidb/1.0.0/$s/${s}R01.edf"; done
(cd "$d" && sha256sum -- *) > "$d/DOWNLOAD.sha256"; cat "$d/DOWNLOAD.sha256"
