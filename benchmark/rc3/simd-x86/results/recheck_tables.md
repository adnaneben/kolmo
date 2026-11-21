
### compression

| input | MB | mode | T | base ms | final ms | final/base | A/A (base2) |
|---|---:|---|---:|---:|---:|---:|---:|
| log_access.log | 4.2 | -m hc | 1 | 953.4 | 944.7 | 1.01× (0.99) | 1.002 |
| log_access.log | 4.2 | -m eco | 1 | 283.3 | 277.5 | 1.02× (0.99) | 1.015 |
| num_f64.npy | 4.2 | -m hc | 1 | 409.0 | 408.2 | 1.00× (1.02) | 0.989 |
| text_dickens.txt | 4.2 | -m eco | 1 | 446.5 | 448.8 | 1.00× (1.02) | 0.952 |
| text_dickens.txt | 4.2 | -m hc | 1 | 1258.6 | 1228.8 | 1.02× (1.02) | 1.039 |
| video_akiyo.y4m | 4.2 | -m eco | 1 | 441.2 | 439.0 | 1.00× (0.97) | 0.982 |
| video_akiyo.y4m | 4.2 | -m hc | 1 | 1766.5 | 1831.1 | 0.96× (1.00) | 0.980 |
| img8_grey.npy | 1.6 | -m ratio | 1 | 643.3 | 653.3 | 0.98× (1.01) | 0.990 |
| text_webster.txt | 4.2 | -m ratio | 1 | 237.2 | 236.0 | 1.01× (0.99) | 0.997 |

### decompression

| input | MB | mode | T | base ms | final ms | final/base | A/A (base2) |
|---|---:|---|---:|---:|---:|---:|---:|
| log_access.log | 4.2 | -m hc | 1 | 12.7 | 12.7 | 1.00× (1.34) | 1.013 |
| log_access.log | 4.2 | -m eco | 1 | 20.4 | 21.1 | 0.96× (1.00) | 0.971 |
| num_f64.npy | 4.2 | -m hc | 1 | 12.0 | 11.0 | 1.09× (1.47) | 1.040 |
| text_dickens.txt | 4.2 | -m eco | 1 | 25.2 | 24.9 | 1.01× (1.25) | 1.029 |
| text_dickens.txt | 4.2 | -m hc | 1 | 16.1 | 15.1 | 1.06× (0.87) | 1.011 |
| video_akiyo.y4m | 4.2 | -m eco | 1 | 23.2 | 21.1 | 1.10× (1.33) | 1.006 |
| video_akiyo.y4m | 4.2 | -m hc | 1 | 13.5 | 11.2 | 1.20× (1.16) | 0.974 |
| img8_grey.npy | 1.6 | -m ratio | 1 | 79.4 | 73.5 | 1.08× (1.09) | 0.997 |
| text_webster.txt | 4.2 | -m ratio | 1 | 122.9 | 118.3 | 1.04× (1.02) | 0.999 |

Noise: |A/A - 1| on the fastest run: median 1.5%, 90th percentile 4.0%, max 4.8% (18 cases). Second-fastest over fastest of base: median 1.3%, 90th percentile 4.9%.
Geometric mean final/base, compression: 1.001× over 9 cases
Geometric mean final/base, decompression: 1.058× over 9 cases
