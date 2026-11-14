#!/bin/sh
# Download exactly the registered E02F publisher files into DIR (needs network). usage: fetch.sh DIR
set -eu
d=$1; mkdir -p "$d"; ua='kolmo-research/1.0 (https://github.com/adnaneben/kolmo; research validation)'
for s in S001 S002 S003 S004; do curl -fsSL --retry 3 -o "$d/${s}R01.edf" "https://physionet.org/files/eegmmidb/1.0.0/$s/${s}R01.edf"; done
for a in Earth Moon Mathematics Water London Music Albert_Einstein 'Python_%28programming_language%29'; do
  f=$(printf '%s' "$a" | sed 's/%28/(/; s/%29/)/')
  curl -fsSL --retry 3 -A "$ua" -o "$d/wiki_$f.json" "https://wikimedia.org/api/rest_v1/metrics/pageviews/per-article/en.wikipedia.org/all-access/user/$a/daily/20160101/20251231"
  sleep 1
done
curl -fsSL --retry 3 -o "$d/fsdd-master.zip" "https://github.com/Jakobovski/free-spoken-digit-dataset/archive/refs/heads/master.zip"
(cd "$d" && sha256sum -- *) > "$d/DOWNLOAD.sha256"; cat "$d/DOWNLOAD.sha256"
