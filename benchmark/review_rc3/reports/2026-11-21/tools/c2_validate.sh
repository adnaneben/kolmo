#!/bin/bash
V=$1
cd $V/final-builds/c2 && make -j4 CC=clang ARCH=-mcpu=native "CFLAGS=-O3 -mcpu=native" build/kolmo > $V/logs/final-build-c2.log 2>&1; echo release=$?
shasum -a 256 $V/final-builds/c2/build/kolmo | sed "s|\$| $(git -C $V/final-builds/c2 rev-parse HEAD) $(file -b $V/final-builds/c2/build/kolmo | cut -c1-40)|" >> $V/final-builds/BINARIES.sha256
bash $V/tools/phase_e.sh $V c2 > $V/logs/phase-e-c2.out 2>&1; tail -20 $V/logs/phase-e-c2.out
