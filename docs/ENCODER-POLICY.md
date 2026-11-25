# Opt-in encoder policy

`tune/encoder_policy.py` is a local file-to-file wrapper for the stock Kolmo CLI. It learns a small table mapping content signatures and size ranges to existing encoder options at a fixed level and thread count. Saved policies contain choices and evidence fingerprints. An ordinary v12 decoder reads the resulting frames. Run the wrapper only where the measured trade-offs make sense. Kolmo's ordinary CLI remains independent of these files.

## Catalogue v1

`tune/format-catalogue.v1.json` defines stable format IDs and the size cutoffs 64 KiB, 1 MiB and 16 MiB. Signatures identify ELF, 64-bit Mach-O, WebAssembly, NPY, GGUF, PNG, FLAC, RIFF WAVE and Parquet. The SafeTensors probe checks a bounded header and contiguous declared tensor ranges. These classifications are routing hints. Kolmo still interprets the file using its native planner. Ambiguous or unsupported input uses the standard CLI preset. The wrapper caps the SafeTensors header probe at 64 KiB and reads only short signatures for other formats.

The schema describes the catalogue shape and the revision describes its contents. Keep existing IDs stable when adding formats. Update the revision when signatures or size cutoffs change and retrain the policy against a fresh split. The catalogue hash invalidates choices learned from another revision.

## Freeze, learn and validate

Prepare non-overlapping development and holdout directories before trying policies. Each manifest is a JSON array with a path relative to that manifest, the file's lowercase SHA-256 and a stable origin ID. Assign related files, variants of one source, and synthetic data from a shared generator lineage to the same origin. A manifest entry looks like this.

```json
{
  "path": "model-a/weights.gguf",
  "sha256": "a_sha256_for_the_original_file",
  "origin": "model-a-family"
}
```

Provide at least two independent origins in **each split and each format and size bucket** you plan to promote. The split seal rejects matching digests and origin IDs across the splits. It verifies the bytes and records the manifest hashes. Treat the seal and development draft as append-only experiment inputs. If holdout results have guided a rerun, reserve a new independent holdout and seal it before the next run. The seal detects drift and exact overlap. Provenance labels and experiment blinding depend on the corpus owner.

Build Kolmo and obtain a stock binary that reads v12. Use the installed stock decoder or a separately pinned build if checking compatibility across builds. Adjust the example configuration to the targeted workload before training. Its ratios express maximum candidate to baseline cost for each metric. The default example requires at least 2% less stored data. It admits at most 5% more encode latency or encode/decode peak memory, 3% more decode latency and 3% more total encode plus decode latency.

```sh
TOOL=tune/encoder_policy.py
ENCODER=./build/kolmo
DECODER=/path/to/stock/kolmo

python3 "$TOOL" seal --development corpus/dev.json --holdout corpus/holdout.json --output split.json

python3 "$TOOL" learn --kolmo "$ENCODER" --decoder "$DECODER" \
  --manifest corpus/dev.json --seal split.json --config tune/encoder-policy.example.v1.json \
  --output draft.json --receipts development-receipts.json

python3 "$TOOL" validate --kolmo "$ENCODER" --decoder "$DECODER" \
  --manifest corpus/holdout.json --seal split.json --config tune/encoder-policy.example.v1.json \
  --draft draft.json --output accepted.json --receipts holdout-receipts.json
```

Each replay measures the production preset and allow-listed analysis or block-size options in deterministic shuffled order. It checks the v12 header, identical compressed bytes across repetitions and byte-for-byte restoration with the supplied decoder. Metrics are end-to-end process latency and peak process RSS for encode and decode, plus compressed bytes. Linux and macOS are supported. The input size and measured times in the receipts also give combined throughput as input bytes divided by encode plus decode seconds.

A route must clear all gates on **every** file in a bucket. Learning chooses the smallest total bytes among eligible development candidates. Validation measures only the frozen candidate for each bucket and omits candidates that miss even one holdout gate. The resulting policy can have zero routes. Use a quiet host and representative input sizes, because process startup, disk cache and concurrent load affect small differences. A local replay is evidence for the pinned host and binaries. Repeat it on deployment classes before broader promotion.

## Apply and roll back

```sh
python3 tune/encoder_policy.py encode --kolmo ./build/kolmo \
  --level 1 --threads 1 --policy accepted.json model.gguf model.gguf.kolmo

./build/kolmo -d -o restored.gguf -- model.gguf.kolmo
```

Use a policy from the same catalogue, tool revision, encoder, host, OS, level and thread scope. Other combinations use the ordinary preset. The wrapper strips ambient `KOLMO_*` and `CZN_*` settings so measurements can be reproduced. Use stock Kolmo directly when you need its full command-line or environment surface. Compression input remains intact and existing destinations retain the stock CLI's overwrite protection.

To roll back, remove the explicit `--policy` parameter or select an earlier preserved accepted file. Existing v12 output keeps decoding with the stock codec. The tool refuses to overwrite earlier policies or receipts. Artifact hashes detect accidental edits. Deploy trusted local policy files and binaries, since neither the manifest nor the policy is a signature. No network or online learning is involved. Inputs are processed locally and policies contain no raw data. Receipts contain input digests, so keep receipts private when inputs are sensitive.
