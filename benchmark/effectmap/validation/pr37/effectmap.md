# Effect map: pr37 against main

**Verdict: ROUTE (passes on some conditions only: gate it)**

- middle tier: mixed: 49 of 147 cells pass
- gain of 0.5% or more on: container, genomics, img16, img8, json, weights, xml
- suggested gate: on when (level <= 6 and size_class <= S0) OR (not (level <= 6) and kind == video); in any case keep it where it removes an inversion: csv S1 -6>-5, genomics S0 -6>-5, img16 S0 -6>-5, img16 S2 -7>-6, img8 S0 -6>-5, tar S2 -7>-6, text S1 -7>-6, video S0 -6>-5, weights S0 -6>-5
- cells: FAIL 50, KEEP-FREE 48, NEUTRAL 129, PASS 1, TRADE 48

## Builds and method

- A: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-main` sha256 `b67bbddb88d78d26` env {}
- B: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-pr37` sha256 `74110accc6d15430` env {}
- A2: `/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-main` sha256 `b67bbddb88d78d26` env {}
- cost basis: **instr** (instructions retired from proc_pid_rusage, load-independent; sizes-only mode, 1 round)
- 1818 condition rows (file x level x threads); arms A, B and A2 (A again) rotate per round; speed ratios inside the A/A band count as 1.00
- command: `effectmap.py run --out /Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/pr37 --a main=/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-main --b pr37=/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-pr37 --a-impact /Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-impact-main --b-impact /Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD/effectmap/bins/kolmo-impact-pr37 --levels 5,6,7 --threads 1,10 --partners klzma,bwt,stack-klzma,stack-bwt,notf,tf-none`

## Never allowed

- level inversions (threads 1, >0.5% and >16 B): A 13, B 1
  - removed -7>-6 canterbury (tar S2, +1.64% on A)
  - removed -6>-5 energy-130col-16k@16k (csv S1, +0.70% on A)
  - removed -6>-5 foreman@1024 (video S0, +2.30% on A)
  - removed -6>-5 gaia-128k@16k (csv S1, +2.23% on A)
  - removed -6>-5 gpt2-f32@1024 (weights S0, +3.43% on A)
  - removed -6>-5 gpt2-f32@3072 (weights S0, +2.16% on A)
  - removed -6>-5 hubble@1024 (img8 S0, +4.74% on A)
  - removed -7>-6 ld30-128k@16k (text S1, +0.53% on A)
  - removed -7>-6 mr (img16 S2, +1.28% on A)
  - removed -7>-6 reymont-128k@16k (text S1, +0.55% on A)
  - removed -6>-5 srr835775@1024 (genomics S0, +3.88% on A)
  - removed -6>-5 x-ray@1024 (img16 S0, +3.38% on A)
- violations: 0
- to confirm in the timed run (not counted yet): 10
  - decode slower at -6: container S2 T1 (-10.769% instructions, one round)
  - RSS 1.31x (median file 1.31x, n=1) at -6: container S2 T1
  - RSS 1.34x (median file 1.34x, n=1) at -6: container S2 T10
  - RSS 1.18x (median file 1.00x, n=3) at -6: log S2 T10
  - RSS 1.53x (median file 1.43x, n=3) at -7: container S1 T1
  - RSS 1.18x (median file 1.18x, n=1) at -7: container S2 T10
  - RSS 1.14x (median file 1.13x, n=2) at -7: img16 S2 T10
  - RSS 1.17x (median file 1.14x, n=3) at -7: log S2 T10
  - RSS 1.21x (median file 1.00x, n=3) at -7: tar S2 T10
  - RSS 1.48x (median file 1.00x, n=6) at -7: tsv S1 T1

## Memory (median peak footprint of the encoder, MB)

| level | threads | size | files | A | B | A2 |
|---|---|---|---:|---:|---:|---:|
| 5 | 1 | S0 | 188 | 2.5 | 2.5 | 2.5 |
| 5 | 1 | S1 | 73 | 16.9 | 17.6 | 18.2 |
| 5 | 1 | S2 | 42 | 70.0 | 68.9 | 69.5 |
| 5 | 10 | S0 | 188 | 2.5 | 2.5 | 2.5 |
| 5 | 10 | S1 | 73 | 16.1 | 16.7 | 18.1 |
| 5 | 10 | S2 | 42 | 76.2 | 76.3 | 77.0 |
| 6 | 1 | S0 | 188 | 3.6 | 3.7 | 3.7 |
| 6 | 1 | S1 | 73 | 18.9 | 21.2 | 20.5 |
| 6 | 1 | S2 | 42 | 73.6 | 73.4 | 73.1 |
| 6 | 10 | S0 | 188 | 3.6 | 3.7 | 3.7 |
| 6 | 10 | S1 | 73 | 20.0 | 19.5 | 20.0 |
| 6 | 10 | S2 | 42 | 79.9 | 80.0 | 80.3 |
| 7 | 1 | S0 | 188 | 5.8 | 7.9 | 5.8 |
| 7 | 1 | S1 | 73 | 20.6 | 25.3 | 21.2 |
| 7 | 1 | S2 | 42 | 69.9 | 70.4 | 70.1 |
| 7 | 10 | S0 | 188 | 5.8 | 7.9 | 5.8 |
| 7 | 10 | S1 | 73 | 20.7 | 22.3 | 20.0 |
| 7 | 10 | S2 | 42 | 80.4 | 83.2 | 79.8 |

## A/A noise band (p95 of |A/A2 - 1| per threads and size class, pooled over levels)

| level | threads | size | enc | dec | comb | rss |
|---|---|---|---:|---:|---:|---:|
| all | 1 | S0 | 17.26% | 16.99% | 12.88% | 2.00% |
| all | 1 | S1 | 5.66% | 15.04% | 6.14% | 13.45% |
| all | 1 | S2 | 0.42% | 5.16% | 0.42% | 6.13% |
| all | 10 | S0 | 15.00% | 18.25% | 10.91% | 2.00% |
| all | 10 | S1 | 6.62% | 15.62% | 5.39% | 20.61% |
| all | 10 | S2 | 0.64% | 6.10% | 0.59% | 3.89% |

## Effect per type, size class and level (threads 1)

bytes: B/A - 1 (sum over files; negative = smaller). enc: B's encode speed change (positive = faster; the raw geometric mean, with the A/A band in brackets).

| type | size | files | -5 bytes | -5 enc | -6 bytes | -6 enc | -7 bytes | -7 enc |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| audio | S0 | 8 | +0.000% | +2.6% (±17.3) | -0.020% | +0.4% (±17.3) | -0.020% | -7.6% (±17.3) |
| audio | S1 | 4 | +0.000% | +0.2% (±5.7) | +0.000% | -33.0% (±5.7) | +0.000% | -21.2% (±5.7) |
| code | S0 | 18 | +0.000% | +3.9% (±17.3) | -0.478% | -4.5% (±17.3) | -0.478% | -11.1% (±17.3) |
| code | S1 | 9 | +0.000% | +0.5% (±5.7) | +0.000% | -26.4% (±5.7) | +0.000% | -39.4% (±5.7) |
| code | S2 | 3 | +0.000% | -0.1% (±0.4) | +0.000% | -11.1% (±0.4) | +0.000% | -11.8% (±0.4) |
| container | S0 | 8 | +0.000% | -0.7% (±17.3) | -0.082% | -2.8% (±17.3) | -0.082% | -9.9% (±17.3) |
| container | S1 | 3 | +0.000% | -0.2% (±5.7) | +0.000% | -39.2% (±5.7) | +0.000% | -51.7% (±5.7) |
| container | S2 | 1 | +0.000% | +0.6% (±0.4) | +0.000% | -37.1% (±0.4) | +0.000% | -30.0% (±0.4) |
| csv | S0 | 30 | +0.000% | +1.3% (±17.3) | -0.398% | -3.9% (±17.3) | -0.382% | -13.4% (±17.3) |
| csv | S1 | 15 | +0.000% | +1.6% (±5.7) | -0.123% | -22.5% (±5.7) | -0.092% | -19.7% (±5.7) |
| csv | S2 | 5 | +0.000% | -0.1% (±0.4) | +0.000% | +0.0% (±0.4) | +0.000% | -0.0% (±0.4) |
| exe | S0 | 6 | +0.000% | +5.7% (±17.3) | -0.136% | -3.6% (±17.3) | -0.136% | -8.5% (±17.3) |
| exe | S2 | 3 | +0.000% | +0.1% (±0.4) | +0.000% | +0.1% (±0.4) | +0.000% | +0.0% (±0.4) |
| genomics | S0 | 4 | +0.000% | -4.6% (±17.3) | -0.531% | +2.8% (±17.3) | -0.374% | -11.2% (±17.3) |
| genomics | S2 | 2 | +0.000% | +0.1% (±0.4) | +0.000% | +0.1% (±0.4) | +0.000% | -0.0% (±0.4) |
| html | S0 | 4 | +0.000% | +3.7% (±17.3) | -0.467% | +0.1% (±17.3) | -0.467% | -5.5% (±17.3) |
| html | S1 | 3 | +0.000% | -0.1% (±5.7) | +0.000% | -23.4% (±5.7) | +0.000% | -28.2% (±5.7) |
| img16 | S0 | 4 | +0.000% | +0.0% (±17.3) | -0.564% | -0.4% (±17.3) | -0.564% | -11.9% (±17.3) |
| img16 | S2 | 2 | +0.000% | +0.0% (±0.4) | +0.000% | -0.0% (±0.4) | -0.429% | -12.6% (±0.4) |
| img8 | S0 | 8 | +0.000% | +4.7% (±17.3) | -0.605% | -0.2% (±17.3) | -0.605% | -9.6% (±17.3) |
| img8 | S1 | 2 | +0.000% | +0.1% (±5.7) | -0.015% | -33.6% (±5.7) | +0.000% | -29.2% (±5.7) |
| img8 | S2 | 2 | +0.000% | +0.1% (±0.4) | +0.000% | -10.6% (±0.4) | +0.000% | -7.1% (±0.4) |
| json | S0 | 12 | +0.000% | +0.1% (±17.3) | -0.715% | -2.2% (±17.3) | -0.715% | -12.2% (±17.3) |
| json | S1 | 6 | +0.000% | +1.6% (±5.7) | +0.000% | -23.3% (±5.7) | +0.000% | -25.9% (±5.7) |
| json | S2 | 2 | +0.000% | +0.1% (±0.4) | +0.000% | +0.1% (±0.4) | +0.000% | +0.0% (±0.4) |
| log | S0 | 18 | +0.000% | +0.9% (±17.3) | -0.373% | -0.8% (±17.3) | -0.322% | -9.2% (±17.3) |
| log | S1 | 9 | +0.000% | +0.3% (±5.7) | +0.000% | -19.9% (±5.7) | -0.012% | -24.9% (±5.7) |
| log | S2 | 3 | +0.000% | -0.1% (±0.4) | +0.000% | -30.1% (±0.4) | +0.000% | -28.1% (±0.4) |
| numeric | S0 | 8 | +0.000% | +10.8% (±17.3) | -0.123% | -2.2% (±17.3) | -0.160% | -13.4% (±17.3) |
| numeric | S1 | 1 | +0.000% | +0.3% (±5.7) | -0.036% | -34.4% (±5.7) | -0.036% | -11.0% (±5.7) |
| numeric | S2 | 3 | +0.000% | +0.1% (±0.4) | -0.083% | -19.5% (±0.4) | -0.080% | -19.5% (±0.4) |
| tar | S0 | 6 | +0.000% | +4.2% (±17.3) | -0.118% | -8.9% (±17.3) | -0.118% | -7.5% (±17.3) |
| tar | S2 | 3 | +0.000% | -0.3% (±0.4) | +0.000% | -12.8% (±0.4) | -0.203% | -14.9% (±0.4) |
| text | S0 | 18 | +0.000% | +3.3% (±17.3) | -0.443% | -1.0% (±17.3) | -0.443% | -12.5% (±17.3) |
| text | S1 | 9 | +0.000% | +1.0% (±5.7) | -0.006% | -28.6% (±5.7) | -0.046% | -45.0% (±5.7) |
| text | S2 | 3 | +0.000% | +0.1% (±0.4) | +0.000% | -0.0% (±0.4) | +0.000% | -0.0% (±0.4) |
| tsv | S0 | 12 | +0.000% | +3.7% (±17.3) | -0.495% | -2.1% (±17.3) | -0.495% | -14.1% (±17.3) |
| tsv | S1 | 6 | +0.000% | +0.8% (±5.7) | +0.000% | -22.0% (±5.7) | +0.000% | -19.4% (±5.7) |
| tsv | S2 | 2 | +0.000% | +0.0% (±0.4) | +0.000% | +0.0% (±0.4) | +0.000% | -0.1% (±0.4) |
| video | S0 | 4 | +0.000% | +3.8% (±17.3) | -0.432% | +0.2% (±17.3) | -0.432% | -8.2% (±17.3) |
| video | S2 | 2 | +0.000% | +0.0% (±0.4) | +0.000% | +0.1% (±0.4) | +0.000% | +0.2% (±0.4) |
| weights | S0 | 8 | +0.000% | +0.3% (±17.3) | -0.766% | -5.7% (±17.3) | -0.685% | -9.5% (±17.3) |
| weights | S2 | 4 | +0.000% | +0.1% (±0.4) | +0.000% | +0.1% (±0.4) | +0.000% | +0.2% (±0.4) |
| xml | S0 | 12 | +0.000% | +1.4% (±17.3) | -0.756% | +0.2% (±17.3) | -0.756% | -11.4% (±17.3) |
| xml | S1 | 6 | +0.000% | +1.4% (±5.7) | +0.000% | -24.9% (±5.7) | +0.000% | -41.2% (±5.7) |
| xml | S2 | 2 | +0.000% | +0.1% (±0.4) | +0.000% | +0.2% (±0.4) | +0.000% | -0.0% (±0.4) |

## Cells (threads 1)

| level | type | size | n | bytes | enc | dec | comb | rss | class | worst file | why |
|---|---|---|---:|---:|---:|---:|---:|---:|---|---|---|
| 5 | audio | S0 | 8 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | esc50-1-100032@1024 +0.000% |  |
| 5 | audio | S1 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | esc50-1-100032 +0.000% |  |
| 5 | code | S0 | 18 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | flac-c@1024 +0.000% |  |
| 5 | code | S1 | 9 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | flac-c-16k@16k +0.000% |  |
| 5 | code | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | flac-c +0.000% |  |
| 5 | container | S0 | 8 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | kennedy-xls@1024 +0.000% |  |
| 5 | container | S1 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | kennedy-xls +0.000% |  |
| 5 | container | S2 | 1 | +0.000% | +0.000% | +0.000% | +0.629% | 1.00 | NEUTRAL | highway-pdf +0.000% |  |
| 5 | csv | S0 | 30 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | energy-130col@1024 +0.000% |  |
| 5 | csv | S1 | 15 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | energy-130col-16k@16k +0.000% |  |
| 5 | csv | S2 | 5 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | energy-130col +0.000% |  |
| 5 | exe | S0 | 6 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ooffice-x86@1024 +0.000% |  |
| 5 | exe | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ooffice-x86 +0.000% |  |
| 5 | genomics | S0 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | srr835775@1024 +0.000% |  |
| 5 | genomics | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | srr835775 +0.000% |  |
| 5 | html | S0 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | bashref@1024 +0.000% |  |
| 5 | html | S1 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | bashref +0.000% |  |
| 5 | img16 | S0 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | x-ray@1024 +0.000% |  |
| 5 | img16 | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | x-ray +0.000% |  |
| 5 | img8 | S0 | 8 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | retina@1024 +0.000% |  |
| 5 | img8 | S1 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | coffee +0.000% |  |
| 5 | img8 | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | retina +0.000% |  |
| 5 | json | S0 | 12 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | api@1024 +0.000% |  |
| 5 | json | S1 | 6 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | api-16k@16k +0.000% |  |
| 5 | json | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | api +0.000% |  |
| 5 | log | S0 | 18 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | loghub@1024 +0.000% |  |
| 5 | log | S1 | 9 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | loghub-16k@16k +0.000% |  |
| 5 | log | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | loghub +0.000% |  |
| 5 | numeric | S0 | 8 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | population-i64@1024 +0.000% |  |
| 5 | numeric | S1 | 1 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | population-i64 +0.000% |  |
| 5 | numeric | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | owid-f64 +0.000% |  |
| 5 | tar | S0 | 6 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | canterbury@1024 +0.000% |  |
| 5 | tar | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | canterbury +0.000% |  |
| 5 | text | S0 | 18 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | dickens@1024 +0.000% |  |
| 5 | text | S1 | 9 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | dickens-16k@16k +0.000% |  |
| 5 | text | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | dickens +0.000% |  |
| 5 | tsv | S0 | 12 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ghcn-rows@1024 +0.000% |  |
| 5 | tsv | S1 | 6 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ghcn-rows-16k@16k +0.000% |  |
| 5 | tsv | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ghcn-rows +0.000% |  |
| 5 | video | S0 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | akiyo@1024 +0.000% |  |
| 5 | video | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | akiyo +0.000% |  |
| 5 | weights | S0 | 8 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | bf16-10mb@1024 +0.000% |  |
| 5 | weights | S2 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | bf16-10mb +0.000% |  |
| 5 | xml | S0 | 12 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | silesia-xml@1024 +0.000% |  |
| 5 | xml | S1 | 6 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | silesia-xml-16k@16k +0.000% |  |
| 5 | xml | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | silesia-xml +0.000% |  |
| 6 | audio | S0 | 8 | -0.020% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | esc50-1-28808@1024 +0.000% |  |
| 6 | audio | S1 | 4 | +0.000% | -33.048% | +0.000% | -31.622% | 1.00 | FAIL | esc50-1-100032 +0.000% | encode 0.670 < 0.80; 0.000% smaller for 49.4% more encode time (needs 4.936%) |
| 6 | code | S0 | 18 | -0.478% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | flac-c@3072 +0.000% |  |
| 6 | code | S1 | 9 | +0.000% | -26.422% | +0.000% | -24.615% | 1.00 | FAIL | flac-c-16k@16k +0.000% | encode 0.736 < 0.80; 0.000% smaller for 35.9% more encode time (needs 3.591%) |
| 6 | code | S2 | 3 | +0.000% | -11.059% | +0.000% | -10.871% | 1.00 | FAIL | flac-c +0.000% | 0.000% smaller for 12.4% more encode time (needs 1.243%) |
| 6 | container | S0 | 8 | -0.082% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | kennedy-xls@3072 +0.000% |  |
| 6 | container | S1 | 3 | +0.000% | -39.240% | +0.000% | -38.460% | 1.00 | FAIL | kennedy-xls +0.000% | encode 0.608 < 0.80; 0.000% smaller for 64.6% more encode time (needs 6.458%) |
| 6 | container | S2 | 1 | +0.000% | -37.052% | -10.769% | -36.601% | 1.00 | FAIL | highway-pdf +0.000% | decode 0.892 < 0.95; encode 0.629 < 0.80; 0.000% smaller for 58.9% more encode time (needs 5.886%) |
| 6 | csv | S0 | 30 | -0.398% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | ghcn@1024 +0.000% |  |
| 6 | csv | S1 | 15 | -0.123% | -22.459% | +0.000% | -20.550% | 1.00 | TRADE | energy-130col-128k@16k +0.000% | encode 0.775 < 0.80; 0.123% smaller for 29.0% more encode time (needs 2.896%) |
| 6 | csv | S2 | 5 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | energy-130col +0.000% |  |
| 6 | exe | S0 | 6 | -0.136% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | ooffice-x86@1024 +0.000% |  |
| 6 | exe | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ooffice-x86 +0.000% |  |
| 6 | genomics | S0 | 4 | -0.531% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | srr835775@3072 +0.000% |  |
| 6 | genomics | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | srr835775 +0.000% |  |
| 6 | html | S0 | 4 | -0.467% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | bashref@3072 +0.000% |  |
| 6 | html | S1 | 3 | +0.000% | -23.379% | +0.000% | -20.990% | 1.00 | FAIL | bashref +0.000% | encode 0.766 < 0.80; 0.000% smaller for 30.5% more encode time (needs 3.051%) |
| 6 | img16 | S0 | 4 | -0.564% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | x-ray@3072 +0.000% |  |
| 6 | img16 | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | x-ray +0.000% |  |
| 6 | img8 | S0 | 8 | -0.605% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | retina@1024 +0.000% |  |
| 6 | img8 | S1 | 2 | -0.015% | -33.636% | +0.000% | -33.128% | 1.00 | TRADE | coffee +0.000% | encode 0.664 < 0.80; 0.015% smaller for 50.7% more encode time (needs 5.068%) |
| 6 | img8 | S2 | 2 | +0.000% | -10.570% | +0.000% | -10.546% | 1.00 | FAIL | retina +0.000% | 0.000% smaller for 11.8% more encode time (needs 1.182%) |
| 6 | json | S0 | 12 | -0.715% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | api@3072 +0.000% |  |
| 6 | json | S1 | 6 | +0.000% | -23.265% | +0.000% | -21.517% | 1.00 | FAIL | api-16k@16k +0.000% | encode 0.767 < 0.80; 0.000% smaller for 30.3% more encode time (needs 3.032%) |
| 6 | json | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | api +0.000% |  |
| 6 | log | S0 | 18 | -0.373% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | loghub@3072 +0.000% |  |
| 6 | log | S1 | 9 | +0.000% | -19.921% | +0.000% | -16.892% | 1.00 | FAIL | loghub-16k@16k +0.000% | 0.000% smaller for 24.9% more encode time (needs 2.488%) |
| 6 | log | S2 | 3 | +0.000% | -30.110% | +0.000% | -29.221% | 1.00 | FAIL | loghub +0.000% | encode 0.699 < 0.80; 0.000% smaller for 43.1% more encode time (needs 4.308%) |
| 6 | numeric | S0 | 8 | -0.123% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | owid-f64@3072 +0.000% |  |
| 6 | numeric | S1 | 1 | -0.036% | -34.411% | +0.000% | -33.993% | 1.00 | TRADE | population-i64 -0.036% | encode 0.656 < 0.80; 0.036% smaller for 52.5% more encode time (needs 5.247%) |
| 6 | numeric | S2 | 3 | -0.083% | -19.514% | +0.000% | -19.196% | 1.00 | TRADE | ncep-nc +0.000% | 0.083% smaller for 24.2% more encode time (needs 2.424%) |
| 6 | tar | S0 | 6 | -0.118% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | samba@1024 +0.000% |  |
| 6 | tar | S2 | 3 | +0.000% | -12.767% | +0.000% | -12.554% | 1.00 | FAIL | canterbury +0.000% | 0.000% smaller for 14.6% more encode time (needs 1.464%) |
| 6 | text | S0 | 18 | -0.443% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | dickens@3072 +0.000% |  |
| 6 | text | S1 | 9 | -0.006% | -28.618% | +0.000% | -26.467% | 1.00 | TRADE | dickens-16k@16k +0.000% | encode 0.714 < 0.80; 0.006% smaller for 40.1% more encode time (needs 4.009%) |
| 6 | text | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | dickens +0.000% |  |
| 6 | tsv | S0 | 12 | -0.495% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | ghcn-rows@3072 +0.000% |  |
| 6 | tsv | S1 | 6 | +0.000% | -22.032% | +0.000% | -20.759% | 1.00 | FAIL | ghcn-rows-16k@16k +0.000% | encode 0.780 < 0.80; 0.000% smaller for 28.3% more encode time (needs 2.826%) |
| 6 | tsv | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ghcn-rows +0.000% |  |
| 6 | video | S0 | 4 | -0.432% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | akiyo@3072 +0.000% |  |
| 6 | video | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | akiyo +0.000% |  |
| 6 | weights | S0 | 8 | -0.766% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | magika-onnx@1024 +0.000% |  |
| 6 | weights | S2 | 4 | +0.000% | +0.000% | -4.735% | +0.000% | 1.00 | PASS | bf16-10mb +0.000% |  |
| 6 | xml | S0 | 12 | -0.756% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | silesia-xml@3072 +0.000% |  |
| 6 | xml | S1 | 6 | +0.000% | -24.911% | +0.000% | -23.572% | 1.00 | FAIL | silesia-xml-16k@16k +0.000% | encode 0.751 < 0.80; 0.000% smaller for 33.2% more encode time (needs 3.318%) |
| 6 | xml | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | silesia-xml +0.000% |  |
| 7 | audio | S0 | 8 | -0.020% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | esc50-1-28808@1024 +0.000% |  |
| 7 | audio | S1 | 4 | +0.000% | -21.156% | +0.000% | -20.496% | 1.00 | FAIL | esc50-1-100032 +0.000% | encode 0.788 < 0.80; 0.000% smaller for 26.8% more encode time (needs 2.683%) |
| 7 | code | S0 | 18 | -0.478% | -11.100% | +0.000% | -6.011% | 1.00 | TRADE | flac-c@3072 +0.000% | 0.478% smaller for 12.5% more encode time (needs 1.249%) |
| 7 | code | S1 | 9 | +0.000% | -39.371% | +0.000% | -37.293% | 1.00 | FAIL | flac-c-16k@16k +0.000% | encode 0.606 < 0.80; 0.000% smaller for 64.9% more encode time (needs 6.494%) |
| 7 | code | S2 | 3 | +0.000% | -11.813% | +0.000% | -11.734% | 1.00 | FAIL | flac-c +0.000% | 0.000% smaller for 13.4% more encode time (needs 1.339%) |
| 7 | container | S0 | 8 | -0.082% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | kennedy-xls@3072 +0.000% |  |
| 7 | container | S1 | 3 | +0.000% | -51.711% | +0.000% | -51.047% | 1.00 | FAIL | kennedy-xls +0.000% | encode 0.483 < 0.80; 0.000% smaller for 107.1% more encode time (needs 10.709%) |
| 7 | container | S2 | 1 | +0.000% | -30.011% | +0.000% | -29.855% | 1.00 | FAIL | highway-pdf +0.000% | encode 0.700 < 0.80; 0.000% smaller for 42.9% more encode time (needs 4.288%) |
| 7 | csv | S0 | 30 | -0.382% | -13.390% | +0.000% | -7.564% | 1.00 | TRADE | ghcn@1024 +0.000% | 0.382% smaller for 15.5% more encode time (needs 1.546%) |
| 7 | csv | S1 | 15 | -0.092% | -19.669% | +0.000% | -17.735% | 1.00 | TRADE | energy-130col-128k@16k +0.000% | 0.092% smaller for 24.5% more encode time (needs 2.448%) |
| 7 | csv | S2 | 5 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ghcn +0.000% |  |
| 7 | exe | S0 | 6 | -0.136% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | ooffice-x86@1024 +0.000% |  |
| 7 | exe | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ooffice-x86 +0.000% |  |
| 7 | genomics | S0 | 4 | -0.374% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | srr835775@3072 +0.000% |  |
| 7 | genomics | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | srr835775 +0.000% |  |
| 7 | html | S0 | 4 | -0.467% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | bashref@3072 +0.000% |  |
| 7 | html | S1 | 3 | +0.000% | -28.229% | +0.000% | -26.703% | 1.00 | FAIL | bashref +0.000% | encode 0.718 < 0.80; 0.000% smaller for 39.3% more encode time (needs 3.933%) |
| 7 | img16 | S0 | 4 | -0.564% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | x-ray@3072 +0.000% |  |
| 7 | img16 | S2 | 2 | -0.429% | -12.577% | +0.000% | -12.461% | 1.00 | TRADE | x-ray +0.000% | 0.429% smaller for 14.4% more encode time (needs 1.439%) |
| 7 | img8 | S0 | 8 | -0.605% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | retina@1024 +0.000% |  |
| 7 | img8 | S1 | 2 | +0.000% | -29.204% | +0.000% | -28.671% | 1.00 | FAIL | coffee +0.000% | encode 0.708 < 0.80; 0.000% smaller for 41.3% more encode time (needs 4.125%) |
| 7 | img8 | S2 | 2 | +0.000% | -7.147% | +0.000% | -7.117% | 1.00 | FAIL | retina +0.000% | 0.000% smaller for 7.7% more encode time (needs 0.770%) |
| 7 | json | S0 | 12 | -0.715% | -12.211% | +0.000% | +0.000% | 1.00 | TRADE | api@3072 +0.000% | 0.715% smaller for 13.9% more encode time (needs 1.391%) |
| 7 | json | S1 | 6 | +0.000% | -25.873% | +0.000% | -24.838% | 1.00 | FAIL | api-16k@16k +0.000% | encode 0.741 < 0.80; 0.000% smaller for 34.9% more encode time (needs 3.490%) |
| 7 | json | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | api +0.000% |  |
| 7 | log | S0 | 18 | -0.322% | -9.247% | +0.000% | +0.000% | 1.00 | TRADE | loghub@3072 +0.000% | 0.322% smaller for 10.2% more encode time (needs 1.019%) |
| 7 | log | S1 | 9 | -0.012% | -24.913% | +0.000% | -23.439% | 1.00 | TRADE | loghub-16k@16k +0.000% | encode 0.751 < 0.80; 0.012% smaller for 33.2% more encode time (needs 3.318%) |
| 7 | log | S2 | 3 | +0.000% | -28.109% | +0.000% | -27.877% | 1.00 | FAIL | loghub +0.000% | encode 0.719 < 0.80; 0.000% smaller for 39.1% more encode time (needs 3.910%) |
| 7 | numeric | S0 | 8 | -0.160% | -13.427% | +0.000% | -9.083% | 1.00 | TRADE | sao@1024 +0.000% | 0.160% smaller for 15.5% more encode time (needs 1.551%) |
| 7 | numeric | S1 | 1 | -0.036% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | population-i64 -0.036% |  |
| 7 | numeric | S2 | 3 | -0.080% | -19.532% | +0.000% | -19.204% | 1.00 | TRADE | ncep-nc +0.000% | 0.080% smaller for 24.3% more encode time (needs 2.427%) |
| 7 | tar | S0 | 6 | -0.118% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | canterbury@1024 +0.000% |  |
| 7 | tar | S2 | 3 | -0.203% | -14.941% | +0.000% | -14.832% | 1.00 | TRADE | samba +0.000% | 0.203% smaller for 17.6% more encode time (needs 1.757%) |
| 7 | text | S0 | 18 | -0.443% | -12.458% | +0.000% | -7.023% | 1.00 | TRADE | dickens@3072 +0.000% | 0.443% smaller for 14.2% more encode time (needs 1.423%) |
| 7 | text | S1 | 9 | -0.046% | -44.951% | +0.000% | -42.392% | 1.00 | TRADE | dickens-16k@16k +0.000% | encode 0.550 < 0.80; 0.046% smaller for 81.7% more encode time (needs 8.166%) |
| 7 | text | S2 | 3 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | dickens +0.000% |  |
| 7 | tsv | S0 | 12 | -0.495% | -14.071% | +0.000% | -8.775% | 1.00 | TRADE | ghcn-rows@3072 +0.000% | 0.495% smaller for 16.4% more encode time (needs 1.638%) |
| 7 | tsv | S1 | 6 | +0.000% | -19.430% | +0.000% | -18.156% | 1.00 | FAIL | ghcn-rows-16k@16k +0.000% | 0.000% smaller for 24.1% more encode time (needs 2.412%) |
| 7 | tsv | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | ghcn-rows +0.000% |  |
| 7 | video | S0 | 4 | -0.432% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | akiyo@3072 +0.000% |  |
| 7 | video | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | foreman +0.000% |  |
| 7 | weights | S0 | 8 | -0.685% | +0.000% | +0.000% | +0.000% | 1.00 | KEEP-FREE | magika-onnx@1024 +0.000% |  |
| 7 | weights | S2 | 4 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | bf16-10mb +0.000% |  |
| 7 | xml | S0 | 12 | -0.756% | -11.353% | +0.000% | +0.000% | 1.00 | TRADE | silesia-xml@3072 +0.000% | 0.756% smaller for 12.8% more encode time (needs 1.281%) |
| 7 | xml | S1 | 6 | +0.000% | -41.247% | +0.000% | -38.829% | 1.00 | FAIL | silesia-xml-16k@16k +0.000% | encode 0.588 < 0.80; 0.000% smaller for 70.2% more encode time (needs 7.020%) |
| 7 | xml | S2 | 2 | +0.000% | +0.000% | +0.000% | +0.000% | 1.00 | NEUTRAL | silesia-xml +0.000% |  |

## Threads 4 and 10: cells whose class differs from threads 1 (11)

| level | type | size | threads | enc | dec | class (T1) | class | why |
|---|---|---|---:|---:|---:|---|---|---|
| 6 | csv | S0 | 10 | -5.165% | +0.000% | KEEP-FREE | TRADE | 0.398% smaller for 5.4% more encode time (needs 0.545%) |
| 6 | weights | S2 | 10 | +0.000% | +0.000% | PASS | NEUTRAL |  |
| 7 | audio | S0 | 10 | -10.598% | +0.000% | KEEP-FREE | TRADE | 0.020% smaller for 11.9% more encode time (needs 1.185%) |
| 7 | container | S0 | 10 | -14.995% | +0.000% | KEEP-FREE | TRADE | 0.082% smaller for 17.6% more encode time (needs 1.764%) |
| 7 | exe | S0 | 10 | -12.869% | +0.000% | KEEP-FREE | TRADE | 0.136% smaller for 14.8% more encode time (needs 1.477%) |
| 7 | genomics | S0 | 10 | -12.990% | +0.000% | KEEP-FREE | TRADE | 0.374% smaller for 14.9% more encode time (needs 1.493%) |
| 7 | html | S0 | 10 | -16.274% | +0.000% | KEEP-FREE | TRADE | 0.467% smaller for 19.4% more encode time (needs 1.944%) |
| 7 | img16 | S0 | 10 | -14.493% | +0.000% | KEEP-FREE | TRADE | 0.564% smaller for 16.9% more encode time (needs 1.695%) |
| 7 | img8 | S0 | 10 | -15.282% | +0.000% | KEEP-FREE | TRADE | 0.605% smaller for 18.0% more encode time (needs 1.804%) |
| 7 | tar | S0 | 10 | -10.905% | +0.000% | KEEP-FREE | TRADE | 0.118% smaller for 12.2% more encode time (needs 1.224%) |
| 7 | weights | S0 | 10 | -14.978% | +0.000% | KEEP-FREE | TRADE | 0.685% smaller for 17.6% more encode time (needs 1.762%) |

Full table: `cells.csv` (every level, type, size class and thread count).

## Stacking (sizes, threads 1)

alone: B/A - 1 without the partner; with: (B+partner)/(A+partner) - 1. Rows: each partner's total, then every type that is not plain `stacks`/`none`.

| partner | type | n | alone | with | label |
|---|---|---:|---:|---:|---|
| bwt | (all) | 345 | -0.020% | -0.006% | absorbed |
| bwt | csv | 60 | -0.016% | +0.000% | absorbed |
| bwt | img16 | 6 | -0.143% | +0.000% | absorbed |
| bwt | tar | 9 | -0.067% | +0.000% | absorbed |
| klzma | (all) | 345 | -0.020% | -0.013% | stacks |
| klzma | csv | 60 | -0.016% | +0.000% | absorbed |
| klzma | tar | 9 | -0.067% | +0.000% | absorbed |
| klzma | text | 36 | -0.004% | +0.000% | absorbed |
| notf | (all) | 345 | -0.020% | -0.013% | stacks |
| notf | img16 | 6 | -0.143% | +0.000% | absorbed |
| notf | log | 36 | -0.001% | +0.000% | absorbed |
| notf | numeric | 12 | -0.054% | -0.001% | absorbed |
| stack-bwt | (all) | 345 | -0.020% | -0.006% | absorbed |
| stack-bwt | csv | 60 | -0.016% | +0.000% | absorbed |
| stack-bwt | img16 | 6 | -0.143% | +0.000% | absorbed |
| stack-bwt | log | 36 | -0.001% | -0.000% | absorbed |
| stack-bwt | tar | 9 | -0.067% | +0.000% | absorbed |
| stack-klzma | (all) | 345 | -0.020% | -0.000% | absorbed |
| stack-klzma | csv | 60 | -0.016% | +0.000% | absorbed |
| stack-klzma | img16 | 6 | -0.143% | +0.000% | absorbed |
| stack-klzma | log | 36 | -0.001% | +0.000% | absorbed |
| stack-klzma | numeric | 12 | -0.054% | -0.001% | absorbed |
| stack-klzma | tar | 9 | -0.067% | +0.000% | absorbed |
| stack-klzma | text | 36 | -0.004% | +0.000% | absorbed |
| tf-none | (all) | 345 | -0.020% | -0.008% | absorbed |
| tf-none | img16 | 6 | -0.143% | +0.000% | absorbed |
| tf-none | img8 | 12 | -0.001% | +0.000% | absorbed |
| tf-none | log | 36 | -0.001% | +0.000% | absorbed |
| tf-none | numeric | 12 | -0.054% | -0.000% | absorbed |
| tf-none | tar | 9 | -0.067% | +0.000% | absorbed |

## Rules (depth-2 tree on dev, checked on holdout)

Samples: condition rows (file x level x threads) labelled with their cell's class, NEUTRAL left out; win = PASS/KEEP. Features: kind, size class, level. Holdout accuracy: 92%.

| rule | dev rows | dev wins | holdout rows | holdout wins | predicts |
|---|---:|---:|---:|---:|---|
| level <= 6 and size_class <= S0 | 152 | 140 | 224 | 206 | on |
| level <= 6 and not (size_class <= S0) | 70 | 2 | 110 | 2 | off |
| not (level <= 6) and kind == video | 4 | 4 | 4 | 4 | on |
| not (level <= 6) and not (kind == video) | 218 | 24 | 330 | 34 | off |

**Suggested gate:** on when (level <= 6 and size_class <= S0) OR (not (level <= 6) and kind == video); in any case keep it where it removes an inversion: csv S1 -6>-5, genomics S0 -6>-5, img16 S0 -6>-5, img16 S2 -7>-6, img8 S0 -6>-5, tar S2 -7>-6, text S1 -7>-6, video S0 -6>-5, weights S0 -6>-5

A gate that changes output bytes must not use thread count; it must be explained (why the change wins there) before it is implemented, and its files added to the CI ledger.
