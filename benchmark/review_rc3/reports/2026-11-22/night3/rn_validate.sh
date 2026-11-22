#!/bin/bash
V=$1
cd $V/final-builds/rn && make -j4 CC=clang ARCH=-mcpu=native "CFLAGS=-O3 -mcpu=native" build/kolmo > $V/logs/final-build-rn.log 2>&1; echo release=$?
shasum -a 256 $V/final-builds/rn/build/kolmo | sed "s|\$| $(git -C $V/final-builds/rn rev-parse HEAD) 0.0.1-rc1 release build|" >> $V/final-builds/BINARIES.sha256
bash $V/tools/phase_e_nv.sh $V rn > $V/logs/phase-e-rn.out 2>&1; tail -20 $V/logs/phase-e-rn.out
