# Fetch the public inputs into the current directory (run it in a new, empty directory); then make_set.py builds set/.
# Sources: Silesia corpus, Our World in Data CO2/energy CSV, MIT-BIH arrhythmia records 100 and 101 (PhysioNet),
# all-MiniLM-L6-v2 weights (Hugging Face), xiph.org CIF clips. ESC-50 clips: bench/kaud/fetch_data.sh (its music part needs flac and was not run).
set -x
curl -sSL -o silesia.zip https://sun.aei.polsl.pl/~sdeor/corpus/silesia.zip
curl -sSL -o owid-co2-data.csv https://raw.githubusercontent.com/owid/co2-data/master/owid-co2-data.csv
curl -sSL -o owid-energy-data.csv https://raw.githubusercontent.com/owid/energy-data/master/owid-energy-data.csv
curl -sSL -o mitdb100.dat https://physionet.org/files/mitdb/1.0.0/100.dat
curl -sSL -o mitdb101.dat https://physionet.org/files/mitdb/1.0.0/101.dat
curl -sSL -o esc50.csv https://raw.githubusercontent.com/karolpiczak/ESC-50/master/meta/esc50.csv
curl -sSL -o w.safetensors https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/resolve/main/model.safetensors
curl -sSL -o akiyo_cif.y4m https://media.xiph.org/video/derf/y4m/akiyo_cif.y4m
curl -sSL -o foreman_cif.y4m https://media.xiph.org/video/derf/y4m/foreman_cif.y4m
curl -sSL -o mobile_cif.y4m https://media.xiph.org/video/derf/y4m/mobile_cif.y4m
ls -la
