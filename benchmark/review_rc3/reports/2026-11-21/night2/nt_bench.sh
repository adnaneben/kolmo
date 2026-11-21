#!/bin/bash
# nt_bench.sh BIN CODECS BLOCK_KIB REPS : natbench on the night-2 tuning set (screening corpus; holdout reserved)
C=/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/corpus
F="$C/dickens $C/ooffice $C/webster $C/x-ray $C/xml $C/wide_test.csv $C/energy_test.csv $C/api.json $C/loghub_all.log"
"$1" "$2" 1 "$3" "$4" $F
