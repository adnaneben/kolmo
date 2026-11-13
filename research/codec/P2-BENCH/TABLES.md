
### Typed numeric arrays: all 61 development inputs (11 families) — archive run P2-E17  (n = 61)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.70 | 3.38 | 157.3 | 291.0 | 24 / 24 | — | 3 |
| OpenZL L9 (leader, archive) | 5.77 | 4.62 | 28.4 | 193.7 | 24 / 572 | 0 | — |
| Kolmo fast | 6.30 | 4.36 | 74.9 | 217.0 | 24 / 86 | 0 | 25 |
| Kolmo hc | 6.66 | 4.64 | 3.0 | 245.5 | 24 / 364 | 0 | 11 |
| Kolmo max | 6.86 | 4.92 | 7.4 | 68.2 | 24 / 741 | 0 | 21 |
| type-first best-of-2 leaves | 5.67 | 4.41 | 155.9 | 291.4 | 24 / 93 | 34 | 35 |
| type-first per-plane rANS/Huffman | 5.69 | 4.50 | 160.8 | 232.7 | 24 / 92 | 31 | 41 |
| type-first archive (zstd-19 leaves) | 5.77 | 4.72 | 16.5 | 262.9 | 24 / 118 | 1 | 19 |

### Typed numeric arrays: all 61 development inputs (11 families) — speed run P2-E14  (n = 61)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.70 | 3.38 | 196.6 | 353.7 | 25 / 25 | — | — |
| Kolmo fast | 6.30 | 4.36 | 93.8 | 271.3 | 25 / 86 | 0 | — |
| Kolmo type-first Huffman (speed rule) | 5.51 | 4.22 | 252.2 | 354.8 | 25 / 93 | 49 | — |
| type-first Huffman, unguarded | 5.36 | 3.63 | 251.0 | 330.2 | 25 / 92 | 45 | — |
| type-first lazy | 5.51 | 4.22 | 251.1 | 357.7 | 25 / 93 | 44 | — |
| type-first best-of-2 leaves | 5.67 | 4.41 | 197.3 | 355.1 | 25 / 92 | 35 | — |
| type-first per-plane rANS/Huffman | 5.69 | 4.50 | 202.3 | 286.4 | 25 / 92 | 33 | — |

### audio / biosignals (FSDD WAV, speech, MIT-BIH ECG, EEG, dense EEG) — archive run P2-E17  (n = 18)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 1.58 | 1.38 | 81.6 | 142.3 | 24 / 24 | — | 0 |
| OpenZL L9 (leader, archive) | 1.90 | 1.69 | 16.6 | 92.2 | 24 / 61 | 0 | — |
| Kolmo fast | 2.03 | 1.68 | 45.9 | 116.8 | 24 / 24 | 0 | 8 |
| Kolmo hc | 2.07 | 1.72 | 4.6 | 109.5 | 24 / 103 | 0 | 2 |
| Kolmo max | 2.13 | 1.81 | 2.9 | 45.1 | 24 / 106 | 0 | 10 |
| type-first best-of-2 leaves | 2.03 | 1.75 | 68.2 | 112.4 | 24 / 33 | 9 | 17 |
| type-first per-plane rANS/Huffman | 2.03 | 1.75 | 70.5 | 90.9 | 24 / 30 | 7 | 17 |
| type-first archive (zstd-19 leaves) | 2.07 | 1.77 | 4.7 | 109.6 | 24 / 118 | 0 | 10 |

### audio / biosignals (FSDD WAV, speech, MIT-BIH ECG, EEG, dense EEG) — speed run P2-E14  (n = 18)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 1.58 | 1.38 | 101.8 | 172.2 | 25 / 25 | — | — |
| Kolmo fast | 2.03 | 1.68 | 58.2 | 144.9 | 25 / 25 | 0 | — |
| Kolmo type-first Huffman (speed rule) | 1.95 | 1.70 | 127.1 | 138.7 | 25 / 25 | 13 | — |
| type-first Huffman, unguarded | 1.95 | 1.70 | 128.4 | 136.2 | 25 / 25 | 14 | — |
| type-first lazy | 1.95 | 1.70 | 126.7 | 141.0 | 25 / 25 | 10 | — |
| type-first best-of-2 leaves | 2.03 | 1.75 | 85.3 | 137.1 | 25 / 33 | 10 | — |
| type-first per-plane rANS/Huffman | 2.03 | 1.75 | 88.9 | 111.6 | 25 / 30 | 5 | — |

### weather (GHCN daily) — archive run P2-E17  (n = 9)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.62 | 5.11 | 29.5 | 37.6 | 24 / 24 | — | 0 |
| OpenZL L9 (leader, archive) | 7.36 | 8.34 | 15.5 | 25.9 | 24 / 24 | 0 | — |
| Kolmo fast | 6.61 | 7.23 | 15.8 | 38.2 | 24 / 24 | 0 | 1 |
| Kolmo hc | 6.81 | 7.58 | 1.7 | 39.0 | 24 / 24 | 0 | 0 |
| Kolmo max | 6.70 | 7.46 | 1.5 | 32.8 | 24 / 24 | 0 | 0 |
| type-first best-of-2 leaves | 6.65 | 7.28 | 29.3 | 39.4 | 24 / 24 | 3 | 1 |
| type-first per-plane rANS/Huffman | 7.10 | 7.90 | 33.5 | 38.1 | 24 / 24 | 6 | 2 |
| type-first archive (zstd-19 leaves) | 7.10 | 7.90 | 6.6 | 38.1 | 24 / 24 | 0 | 0 |

### weather (GHCN daily) — speed run P2-E14  (n = 9)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.62 | 5.11 | 36.4 | 44.0 | 25 / 25 | — | — |
| Kolmo fast | 6.61 | 7.23 | 19.7 | 45.9 | 25 / 25 | 0 | — |
| Kolmo type-first Huffman (speed rule) | 5.91 | 6.30 | 41.7 | 47.2 | 25 / 25 | 7 | — |
| type-first Huffman, unguarded | 5.91 | 6.30 | 44.8 | 49.5 | 25 / 25 | 7 | — |
| type-first lazy | 5.91 | 6.30 | 43.6 | 49.3 | 25 / 25 | 7 | — |
| type-first best-of-2 leaves | 6.65 | 7.28 | 37.1 | 48.5 | 25 / 25 | 3 | — |
| type-first per-plane rANS/Huffman | 7.10 | 7.90 | 40.1 | 45.1 | 25 / 25 | 8 | — |

### counts / telemetry (pageviews, Backblaze, OWID, controls) — archive run P2-E17  (n = 34)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 6.57 | 4.86 | 193.0 | 378.0 | 24 / 24 | — | 3 |
| OpenZL L9 (leader, archive) | 8.14 | 6.73 | 32.0 | 253.2 | 24 / 572 | 0 | — |
| Kolmo fast | 9.04 | 6.33 | 86.7 | 264.4 | 24 / 86 | 0 | 16 |
| Kolmo hc | 9.80 | 6.89 | 2.9 | 323.8 | 24 / 364 | 0 | 9 |
| Kolmo max | 10.10 | 7.48 | 10.1 | 74.6 | 24 / 741 | 0 | 11 |
| type-first best-of-2 leaves | 7.64 | 6.31 | 204.2 | 419.3 | 24 / 93 | 22 | 17 |
| type-first per-plane rANS/Huffman | 7.68 | 6.40 | 208.7 | 325.7 | 24 / 92 | 18 | 22 |
| type-first archive (zstd-19 leaves) | 7.75 | 6.93 | 26.9 | 360.7 | 25 / 102 | 1 | 9 |

### counts / telemetry (pageviews, Backblaze, OWID, controls) — speed run P2-E14  (n = 34)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 6.57 | 4.86 | 241.6 | 462.3 | 25 / 25 | — | — |
| Kolmo fast | 9.04 | 6.33 | 108.1 | 332.4 | 25 / 86 | 0 | — |
| Kolmo type-first Huffman (speed rule) | 7.47 | 6.12 | 315.9 | 507.9 | 25 / 93 | 29 | — |
| type-first Huffman, unguarded | 7.17 | 4.68 | 310.6 | 453.8 | 25 / 92 | 24 | — |
| type-first lazy | 7.47 | 6.12 | 313.0 | 507.9 | 25 / 93 | 27 | — |
| type-first best-of-2 leaves | 7.64 | 6.31 | 259.8 | 510.1 | 25 / 92 | 22 | — |
| type-first per-plane rANS/Huffman | 7.68 | 6.40 | 263.4 | 403.0 | 25 / 92 | 20 | — |

### Fresh audio: ESC-50 (40 clips, P2-E20)  (n = 40)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 1.36 | 1.46 | 86.7 | 111.9 | 22 / 22 | — | 1 |
| OpenZL L9 (leader, archive) | 1.72 | 1.84 | 18.4 | 74.1 | 22 / 22 | 0 | — |
| Kolmo fast | 1.65 | 1.76 | 39.5 | 106.7 | 22 / 22 | 0 | 14 |
| Kolmo hc | 1.81 | 1.96 | 6.3 | 106.0 | 22 / 23 | 0 | 7 |
| Kolmo max | 1.86 | 2.03 | 4.7 | 48.9 | 22 / 22 | 0 | 4 |
| Kolmo type-first Huffman (speed rule) | 1.75 | 1.85 | 90.3 | 105.9 | 22 / 22 | 25 | 22 |
| type-first best-of-2 leaves | 1.84 | 1.97 | 64.1 | 105.8 | 22 / 22 | 6 | 30 |
| type-first per-plane rANS/Huffman | 1.83 | 1.96 | 70.0 | 90.6 | 22 / 22 | 3 | 27 |
| type-first archive (zstd-19 leaves) | 1.92 | 2.11 | 8.1 | 95.6 | 22 / 24 | 0 | 23 |

### ESC-50 with the research fast-inverse decoder (P2-E28)  (n = 40)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 1.36 | 1.46 | 119.0 | 152.5 | 18 / 18 | — | — |
| Kolmo fast | 1.65 | 1.76 | 41.2 | 139.1 | 18 / 18 | 0 | — |
| Kolmo type-first Huffman (speed rule) | 1.75 | 1.85 | 117.2 | 141.1 | 18 / 18 | 24 | — |
| type-first Huffman + fast-inverse decoder* | 1.75 | 1.85 | 116.8 | 166.4 | 18 / 18 | 35 | — |
| type-first lazy + fast-inverse decoder* | 1.75 | 1.86 | 107.8 | 166.0 | 18 / 18 | 29 | — |

### Fresh audio: MS-SNSD noise + speech (40 files, P2-E29)  (n = 40)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 1.29 | 1.40 | 103.7 | 125.2 | 20 / 20 | — | — |
| Kolmo fast | 1.49 | 1.69 | 34.3 | 105.1 | 20 / 20 | 0 | — |
| Kolmo type-first Huffman (speed rule) | 1.41 | 1.51 | 91.3 | 99.0 | 20 / 26 | 22 | — |
| type-first Huffman + fast-inverse decoder* | 1.41 | 1.51 | 91.6 | 117.6 | 20 / 26 | 22 | — |
| type-first lazy + fast-inverse decoder* | 1.41 | 1.51 | 79.1 | 120.4 | 20 / 26 | 21 | — |

### Real-world CSV: FiveThirtyEight (23, P2-E20)  (n = 23)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.23 | 4.73 | 124.6 | 208.7 | 22 / 22 | — | 0 |
| OpenZL L9 (leader, archive) | 4.97 | 5.84 | 26.9 | 110.4 | 22 / 55 | 0 | — |
| Kolmo fast | 4.60 | 5.24 | 94.7 | 193.1 | 22 / 22 | 0 | 0 |
| Kolmo hc | 5.72 | 6.66 | 3.9 | 186.3 | 22 / 94 | 0 | 0 |
| Kolmo max | 6.43 | 7.64 | 3.7 | 61.4 | 22 / 68 | 0 | 0 |
| typed-CSV container (Huffman) | 4.37 | 4.76 | 113.9 | 224.9 | 22 / 22 | 3 | 1 |
| typed-CSV container (zstd-19) | 5.52 | 6.29 | 3.1 | 228.7 | 22 / 88 | 0 | 0 |

### Real-world CSV: TidyTuesday 35 (P2-E24)  (n = 35)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.90 | 5.19 | 160.1 | 301.4 | 18 / 18 | — | 0 |
| OpenZL L9 (leader, archive) | 6.58 | 7.37 | 29.7 | 145.0 | 26 / 84 | 0 | — |
| Kolmo fast | 5.84 | 6.69 | 117.1 | 264.2 | 18 / 18 | 1 | 4 |
| Kolmo eco | 6.90 | 7.85 | 23.1 | 197.9 | 28 / 51 | 0 | 27 |
| table frame, zstd-12 leaves | 6.91 | 7.94 | 36.9 | 207.1 | 18 / 55 | 0 | 28 |
| Kolmo hc | 7.44 | 8.42 | 5.6 | 254.0 | 39 / 95 | 0 | 2 |
| table frame, BSC leaves | 8.11 | 8.87 | 38.8 | 54.7 | 18 / 24 | 0 | 20 |

### Real-world CSV: TidyTuesday 35 (P2-E26, picker)  (n = 35)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 5.04 | 5.24 | 153.7 | 289.2 | 17 / 17 | — | 6 |
| OpenZL L9 (leader, archive) | 6.22 | 6.67 | 31.0 | 134.9 | 17 / 102 | 0 | — |
| Kolmo fast | 5.72 | 6.46 | 114.7 | 245.5 | 17 / 17 | 0 | 14 |
| Kolmo eco | 7.18 | 7.70 | 22.8 | 187.3 | 23 / 60 | 0 | 29 |
| table frame, zstd-12 leaves | 6.89 | 7.24 | 33.3 | 198.2 | 17 / 56 | 0 | 26 |
| Kolmo CSV picker | 7.40 | 7.82 | 26.1 | 184.8 | 21 / 60 | 0 | 33 |

### Real-world CSV: selva86/dsdojo 28 (P2-E27, picker)  (n = 28)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (leader, speed) | 4.63 | 5.12 | 102.2 | 177.0 | 18 / 18 | — | 4 |
| OpenZL L9 (leader, archive) | 6.18 | 6.72 | 18.1 | 83.7 | 23 / 93 | 0 | — |
| Kolmo fast | 6.38 | 6.50 | 63.2 | 147.7 | 18 / 18 | 0 | 12 |
| Kolmo eco | 7.40 | 7.94 | 19.7 | 122.5 | 22 / 52 | 0 | 26 |
| table frame, zstd-12 leaves | 7.32 | 7.77 | 30.8 | 120.7 | 18 / 54 | 0 | 26 |
| Kolmo CSV picker | 7.33 | 7.79 | 31.5 | 119.3 | 18 / 56 | 0 | 26 |

### Regular numeric CSV tables: Covertype + CollegeMsg (P2-E8)  (n = 10)

| Pipeline | Ratio (total) | Ratio (geo-mean) | Encode MB/s | Decode MB/s | Peak RSS median / max (MiB) | Smaller & within zstd-3 speed cost | Smaller & within OpenZL-9 archive cost |
|---|---|---|---|---|---|---|---|
| Kolmo eco | 8.52 | 7.69 | 21.3 | 144.6 | 23 / 65 | 0 | 2 |
| Kolmo fast | 7.86 | 6.95 | 46.7 | 184.1 | 18 / 22 | 1 | 2 |
| Kolmo fast -q | 7.86 | 6.94 | 47.6 | 183.0 | 18 / 22 | 1 | 2 |
| Kolmo hc | 8.73 | 7.91 | 8.9 | 177.0 | 18 / 123 | 0 | 2 |
| Kolmo max | 9.13 | 8.52 | 4.5 | 102.8 | 18 / 101 | 0 | 1 |
| Kolmo ultra -l1 | 9.68 | 9.05 | 0.5 | 0.9 | 265 / 3910 | 0 | 0 |
| typed-CSV container (zstd-19) | 6.99 | 6.03 | 5.9 | 193.9 | 34 / 37 | 0 | 2 |
| OpenZL L6 | 5.39 | 4.51 | 30.1 | 105.5 | 84 / 140 | 0 | 0 |
| OpenZL L9 (leader, archive) | 5.67 | 4.80 | 15.5 | 104.9 | 66 / 97 | 0 | — |
| OpenZL serial L6 | 3.50 | 4.27 | 42.6 | 169.9 | 18 / 30 | 0 | 0 |
| OpenZL serial L9 | 3.54 | 4.34 | 32.5 | 173.0 | 18 / 32 | 0 | 0 |
| zstd -19 | 4.44 | 5.29 | 3.2 | 193.6 | 38 / 90 | 0 | 0 |
| zstd -3 (leader, speed) | 3.29 | 4.08 | 97.7 | 208.1 | 18 / 18 | — | 0 |
