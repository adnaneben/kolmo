# kolmo vs OpenZL, every OpenZL profile

`run.py` / `run2.py` compress and decompress each panel file with kolmo (`fast -q`, `fast`, `hc -l 9`, `hc`,
`eco`, `max -l 1`, `max`, and `ultra -l 1` for files up to 11 MB) and OpenZL 0.3.0's CLI (`zli`, levels 6 and 9,
every applicable profile, keeping OpenZL's best; plus a trained compressor where one was trained). Both are timed as
CLI processes on one thread, median of three (one for multi-minute settings), file I/O and start-up included. Every
round trip is verified byte for byte. Combined speed = original bytes / (compress time + decompress time).

Data: Silesia members (generic, 16-bit images, SAO); OWID CO2 and energy CSV rows held out from OpenZL's training
quarter; a 316-column table pivoted from OWID energy; plain uncompressed Parquet written by `pq.py`; raw RGB/grey
images and NumPy arrays from scikit-image; OWID numbers as int32/int64/float64 arrays, NumPy and a pandas pickle;
millisecond timestamps parsed from Loghub; a real ONNX model (magika) and its float32 weights repacked by
`prepare_weights.py` as safetensors and GGUF. PyTorch checkpoints were not tested (no PyTorch in this environment).

`results.json` holds every measurement; `verdicts.json` the per-file comparison.
