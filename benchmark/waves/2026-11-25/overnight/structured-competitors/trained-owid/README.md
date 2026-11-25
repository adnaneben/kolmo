# Country-disjoint validation against a trained OpenZL graph

Each OWID dataset was split by the literal country field before training. Three quarters of the hash buckets went to one validation file in the original row order. The remaining buckets were split into four training files. Every training file and validation file retained the original header. No country label appeared on both sides of the split within a table. Separate models were trained on energy and CO₂. These tables contain related countries, years, regions and indicator columns from a shared publisher. The split tests transfer across country labels and does not establish transfer to a new schema or a later time period.

## Training and validation sizes

| Dataset | Train bytes | Train country labels | Held-out bytes | Held-out labels | Train wall seconds | Encoder-only graph bytes |
|---|---:|---:|---:|---:|---:|---:|
| energy | 1,734,835 | 57 | 5,199,066 | 180 | 214.8 | 12,539 |
| co2 | 3,133,954 | 55 | 11,250,672 | 199 | 243.2 | 27,597 |

Both training commands used the built-in CSV profile, one trainer worker and the same 120-second budget. Measured wall time includes initialization, selection and graph output. The look-ahead and search quality of a longer training budget remain untested. The two generated graphs are preserved so exact compressed size and readback can be reproduced independently of training variability. The encoder received the trained graph for the trained rows. The decoder used the built-in CSV profile for every OpenZL row. No dictionary bundle was used.

The CO₂ strict graph returned OpenZL error 76, “Stream parameter invalid,” for each tested level (1, 6 and 12). All three encoder diagnostics are saved. Its ordinary fallback-enabled encoder succeeded at those levels on the same held-out input. Strict CO₂ compressed bytes and throughput are unavailable. The final matrix labels the supported fallback-enabled measurements explicitly. The energy trained graph succeeded in strict mode throughout.

## Measured file commands

| Dataset | Encoder | Level | Bytes | Encode MB/s | Decode MB/s | Combined MB/s |
|---|---|---:|---:|---:|---:|---:|
| energy | Kolmo main | 1 | 2,416,107 | 420.0 | 877.4 | 283.5 |
| energy | Kolmo main | 2 | 1,497,611 | 370.9 | 757.4 | 248.4 |
| energy | Kolmo typed | 1 | 913,144 | 308.9 | 638.3 | 207.9 |
| energy | Kolmo typed | 2 | 794,570 | 283.2 | 581.1 | 189.4 |
| energy | OpenZL CSV | 1 | 1,187,113 | 115.4 | 224.7 | 76.0 |
| energy | Trained OpenZL | 1 | 1,160,807 | 91.0 | 169.7 | 59.2 |
| energy | Trained OpenZL | 6 | 1,156,017 | 82.0 | 168.9 | 55.3 |
| energy | Trained OpenZL | 12 | 1,160,375 | 42.0 | 164.6 | 33.5 |
| co2 | Kolmo main | 1 | 5,254,732 | 469.2 | 1381.4 | 350.3 |
| co2 | Kolmo main | 2 | 3,423,364 | 387.9 | 1013.6 | 280.5 |
| co2 | Kolmo typed | 1 | 1,722,801 | 367.2 | 866.8 | 257.9 |
| co2 | Kolmo typed | 2 | 1,498,333 | 337.7 | 756.6 | 233.5 |
| co2 | OpenZL CSV | 1 | 2,395,369 | 130.4 | 274.8 | 88.9 |
| co2 | Trained OpenZL, fallback enabled | 1 | 2,274,101 | 97.8 | 211.0 | 66.9 |
| co2 | Trained OpenZL, fallback enabled | 6 | 2,266,083 | 75.7 | 210.7 | 55.7 |
| co2 | Trained OpenZL, fallback enabled | 12 | 2,259,350 | 30.6 | 201.2 | 26.5 |

Each arm used one warmup and seven scored runs in seeded shuffled order. All 128 measured archives decoded to the original SHA-256. New input and archive hashes, original and compressed sizes, wall and CPU time and peak RSS are in openzl-owid-trained.jsonl. Stock main decoded all Kolmo archives. Measurements used normal file-to-file commands on the same shared ARM64 host. Reproduction starts with the two original whole files documented in competitor-results.md. Run train_owid.py CORPUS OUT ZLI energy and the same command with co2. Then run csv_competitor.py OUT MAIN CANDIDATE ZLI RESULTS --owid-trained --trained-fallback co2. To reuse the exact measured models, replace each generated graph.zlc with the corresponding .zlc from this directory before timing.

Kolmo typed L2 used 31.3% fewer bytes than the smallest tested strict OpenZL model result on energy, at 3.43 times its combined speed. On CO₂, typed L2 used 33.7% fewer bytes than the smallest fallback-enabled trained OpenZL result, at 8.81 times its combined speed. Against pinned main at L2, the typed route reduced bytes by 46.9% and 56.2% while lowering combined throughput by 23.8% and 16.8%, respectively. Retain the typed route as an opt-in storage or transfer experiment. This comparison cannot qualify a faster default.
