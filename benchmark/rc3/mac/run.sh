#!/bin/sh
# rc3 against rc2 on the Apple M5: every mode at 1 and 10 threads, same files as the multithreaded run of rc2.
T=tools.json; R=results; K=.
run() { g=$1; shift; nice -n 0 python3 -I $K/benchmark/rc2/run_cli2.py --tools $T --group $g --budget 20 --files "$@" --work $R/w-$g --out $R/$g.jsonl >> $R/$g.log 2>&1; echo "$g $?"; }
run structured data/owid-co2-data.csv data/owid_test.csv data/energy_test.csv data/wide_test.csv data/api.json data/owid_arr.json data/loghub_all.log data/owid_energy.pkl data/sao data/covid/countries-aggregated.csv
run numeric data/x-ray data/mr data/year.i32 data/population.i64 data/log_timestamps_ms.i64 data/derived/mitdb-100.i16
run numeric_npy data/typed/*.npy data/owid_energy_f64.npy data/lfw_subset.npy
run weights data/llm_bf16_10mb.safetensors data/llm_bf16_47mb.safetensors data/llm_nvfp4_17mb.safetensors
run audio data/esc50/audio/1-100032-A-0.wav data/esc50/audio/1-16746-A-15.wav data/esc50/audio/1-208757-B-2.wav data/esc50/audio/1-28808-A-43.wav data/esc50/audio/1-40621-A-28.wav
run video data/video/akiyo_cif.y4m data/video/foreman_cif.y4m data/video/mobile_cif.y4m
run genomics data/fastq/SRR835775.fastq data/fastq/SRR2584863.fastq data/chr22.fa
run general data/dickens data/mozilla data/ooffice data/samba data/nci data/osdb data/webster
