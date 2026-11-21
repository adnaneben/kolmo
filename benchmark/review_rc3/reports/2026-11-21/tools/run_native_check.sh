#!/bin/bash
# run_native_check.sh V NEWBIN OUTNAME
V=$1; NEW=$2; OUT=$3; I=$V/evidence/native-matrix-c1-release/inputs; H=$V/corpus-holdout
read RC2 V110 RC3 C1 < <(python3 -c "import json;s=json.load(open('$V/evidence/config/compat-spec-c1.json'))['gens'];print(s['rc2']['bin'],s['main-1.1.0']['bin'],s['rc3']['bin'],s['c1']['bin'])")
cd $V && python3 -I tools/native_version_check.py $V/evidence/$OUT $NEW $C1 $RC3 $RC2 $V110 $I/* $H/chelsea.ppm $H/esc50-2-100648-A-43.wav $H/reymont > $V/logs/$OUT.out 2>&1
echo rc=$?; tail -30 $V/logs/$OUT.out
