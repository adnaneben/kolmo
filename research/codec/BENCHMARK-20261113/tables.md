
### A. Typed numeric arrays (NPY), development, 50 inputs / 9 families

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 2.903 | 4.132 | 73.2 / 101.3 | 150.6 / 174.7 | 24.2 / 24.4 | — | 3/50 |
| OpenZL typed/csv -9 (archive rival) | 3.607 | 5.831 | 16.8 / 22.1 | 106.7 / 116.0 | 24.2 / 61.3 | 0/50 | — |
| Kolmo fast (production) | 3.795 | 5.472 | 46.7 / 56.9 | 131.1 / 159.0 | 24.2 / 24.4 | 0/50 | 23/50 |
| Kolmo hc (production) | 3.935 | 5.853 | 5.1 / 6.0 | 131.2 / 147.6 | 24.3 / 102.6 | 0/50 | 9/50 |
| Kolmo max (production) | 4.057 | 6.196 | 4.1 / 3.9 | 46.4 / 71.1 | 24.2 / 105.9 | 0/50 | 11/50 |
| tp_arch19 | 3.94 | 5.971 | 7.3 / 7.2 | 122.7 / 138.0 | 24.9 / 118.4 | 0/50 | 9/50 |
| tp_prod3 | 3.867 | 5.619 | 104.0 / 127.2 | 162.4 / 174.8 | 24.9 / 29.8 | 31/50 | 24/50 |
| tp_pent | 3.86 | 5.639 | 84.3 / 100.8 | 102.8 / 118.9 | 24.2 / 29.6 | 23/50 | 31/50 |
| tb_best | 3.834 | 5.502 | 71.2 / 96.3 | 131.7 / 157.1 | 24.2 / 32.8 | 28/50 | 25/50 |
| tp_phuf_pg16 | 3.66 | 5.205 | 150.5 / 176.4 | 158.1 / 190.7 | 24.9 / 25.2 | 41/50 | 16/50 |
| tb_lazy | 3.66 | 5.205 | 149.1 / 175.9 | 157.9 / 192.5 | 24.9 / 25.2 | 39/50 | 16/50 |
| tp_phuf | 3.514 | 4.334 | 151.9 / 175.9 | 155.1 / 175.3 | 25.0 / 25.2 | 37/50 | 16/50 |

### B. Whole WAV (FSDD speech), development, 10

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 1.131 | 1.126 | 1.8 / 2.0 | 1.9 / 2.1 | 24.2 / 24.4 | — | 0/10 |
| OpenZL typed/csv -9 (archive rival) | 1.333 | 1.329 | 1.1 / 1.2 | 1.4 / 1.4 | 24.1 / 24.2 | 0/10 | — |
| Kolmo fast (production) | 1.279 | 1.269 | 1.5 / 1.5 | 2.3 / 2.3 | 24.2 / 24.4 | 0/10 | 1/10 |
| Kolmo hc (production) | 1.316 | 1.306 | 1.2 / 1.2 | 2.1 / 2.3 | 24.3 / 24.3 | 0/10 | 2/10 |
| Kolmo max (production) | 1.407 | 1.398 | 1.0 / 1.0 | 2.1 / 2.3 | 24.2 / 24.3 | 0/10 | 10/10 |
| tp_arch19 | 1.375 | 1.369 | 1.5 / 1.5 | 2.2 / 2.4 | 24.2 / 24.3 | 0/10 | 10/10 |
| tb_best | 1.372 | 1.366 | 1.9 / 2.0 | 2.3 / 2.4 | 24.2 / 24.3 | 5/10 | 10/10 |
| tp_pent | 1.372 | 1.366 | 1.9 / 2.0 | 2.3 / 2.4 | 24.2 / 24.4 | 7/10 | 10/10 |
| tp_phuf | 1.372 | 1.366 | 2.6 / 2.7 | 2.3 / 2.6 | 24.9 / 25.1 | 7/10 | 10/10 |
| tp_phuf_pg16 | 1.372 | 1.366 | 2.5 / 2.6 | 2.6 / 2.8 | 25.0 / 25.2 | 7/10 | 10/10 |
| tb_lazy | 1.372 | 1.366 | 2.3 / 2.6 | 2.6 / 2.9 | 24.9 / 25.1 | 4/10 | 10/10 |
| tp_prod3 | 1.278 | 1.269 | 2.5 / 2.6 | 2.7 / 2.9 | 24.9 / 25.1 | 8/10 | 1/10 |

### C1. Fresh audio ESC-50, 40

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 1.364 | 1.464 | 90.2 / 86.7 | 111.1 / 111.9 | 21.7 / 22.0 | — | 1/40 |
| OpenZL typed/csv -9 (archive rival) | 1.718 | 1.844 | 20.2 / 18.4 | 73.7 / 74.1 | 21.8 / 22.0 | 0/40 | — |
| Kolmo fast (production) | 1.647 | 1.761 | 42.5 / 39.5 | 106.5 / 106.7 | 21.8 / 22.0 | 0/40 | 14/40 |
| Kolmo hc (production) | 1.813 | 1.961 | 6.2 / 6.3 | 101.3 / 106.0 | 22.0 / 23.1 | 0/40 | 7/40 |
| Kolmo max (production) | 1.865 | 2.034 | 4.6 / 4.7 | 49.5 / 48.9 | 21.6 / 22.0 | 0/40 | 4/40 |
| tp_arch19 | 1.915 | 2.107 | 8.4 / 8.1 | 98.7 / 95.6 | 21.7 / 24.3 | 0/40 | 23/40 |
| tb_best | 1.839 | 1.973 | 61.9 / 64.1 | 103.8 / 105.8 | 21.7 / 22.0 | 6/40 | 30/40 |
| tp_pent | 1.829 | 1.964 | 70.3 / 70.0 | 90.7 / 90.6 | 21.7 / 22.0 | 3/40 | 27/40 |
| tp_phuf | 1.769 | 1.881 | 93.3 / 92.0 | 104.7 / 102.6 | 21.7 / 22.0 | 24/40 | 22/40 |
| tb_lazy | 1.755 | 1.856 | 91.7 / 85.4 | 104.5 / 107.0 | 21.7 / 22.0 | 23/40 | 23/40 |
| tp_phuf_pg16 | 1.751 | 1.853 | 90.8 / 90.3 | 103.5 / 105.9 | 21.7 / 22.0 | 25/40 | 22/40 |

### C2. Fresh audio MS-SNSD, 40 (speed rival only)

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 1.291 | 1.4 | 41.3 / 103.7 | 44.8 / 125.2 | 19.7 / 20.2 | — | — |
| Kolmo fast (production) | 1.486 | 1.692 | 12.5 / 34.3 | 43.9 / 105.1 | 19.7 / 20.2 | 0/40 | — |
| tb_best | 1.603 | 1.817 | 33.2 / 56.5 | 45.6 / 98.6 | 19.7 / 39.0 | 3/40 | — |
| tb_best_fd | 1.603 | 1.817 | 32.7 / 56.5 | 50.2 / 121.4 | 19.6 / 38.9 | 8/40 | — |
| tp_prod3 | 1.602 | 1.806 | 35.2 / 63.2 | 46.8 / 97.6 | 19.7 / 32.8 | 7/40 | — |
| tp_prod3_fd | 1.602 | 1.806 | 36.1 / 63.3 | 51.1 / 120.3 | 19.7 / 32.7 | 13/40 | — |
| kolmo_fast_fd | 1.486 | 1.692 | 12.4 / 34.2 | 49.2 / 121.9 | 19.7 / 20.1 | 0/40 | — |
| tp_phuf | 1.414 | 1.515 | 42.9 / 93.4 | 46.7 / 97.4 | 19.8 / 25.2 | 20/40 | — |
| tp_phuf_fd | 1.414 | 1.515 | 47.7 / 92.9 | 50.6 / 117.8 | 19.7 / 25.2 | 23/40 | — |
| tp_phuf_pg16 | 1.41 | 1.512 | 42.8 / 91.3 | 47.9 / 99.0 | 19.7 / 25.5 | 22/40 | — |
| tp_phuf_pg16_fd | 1.41 | 1.512 | 43.1 / 91.6 | 50.9 / 117.6 | 19.7 / 25.6 | 22/40 | — |
| tb_lazy | 1.41 | 1.512 | 39.9 / 79.0 | 47.8 / 98.6 | 19.7 / 25.4 | 19/40 | — |
| tb_lazy_fd | 1.41 | 1.512 | 38.4 / 79.1 | 50.5 / 120.4 | 19.7 / 25.5 | 21/40 | — |

### C3. Fresh audio AudioMNIST + audiomentations, 40 (speed rival only; _fd = research fast-inverse decoder)

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 1.688 | 2.199 | 22.3 / 30.4 | 26.2 / 36.2 | 17.7 / 17.9 | — | — |
| Kolmo fast (production) | 2.347 | 3.495 | 9.8 / 13.8 | 28.8 / 39.6 | 17.7 / 17.9 | 0/40 | — |
| ta_rep | 2.408 | 3.335 | 22.6 / 31.5 | 29.6 / 40.4 | 17.7 / 17.9 | 34/40 | — |
| ta_rep_fd | 2.408 | 3.335 | 22.9 / 31.1 | 30.3 / 41.6 | 17.7 / 17.9 | 30/40 | — |
| tp_phuf_pg16 | 2.389 | 3.283 | 24.6 / 33.6 | 29.6 / 40.7 | 17.7 / 17.9 | 33/40 | — |
| tp_phuf_pg16_fd | 2.389 | 3.283 | 24.5 / 33.4 | 30.2 / 41.4 | 17.7 / 17.9 | 34/40 | — |
| ta_phuf | 2.389 | 3.283 | 24.4 / 34.4 | 29.6 / 40.6 | 17.7 / 17.9 | 35/40 | — |
| ta_phuf_fd | 2.389 | 3.283 | 25.2 / 34.3 | 30.5 / 41.6 | 17.7 / 17.9 | 35/40 | — |
| ta_rep_fb | 2.352 | 3.191 | 22.8 / 31.5 | 29.9 / 40.5 | 17.7 / 17.9 | 29/40 | — |
| ta_rep_fb_fd | 2.352 | 3.191 | 22.8 / 31.5 | 30.1 / 41.6 | 17.7 / 17.9 | 33/40 | — |

### D. Regular numeric CSV tables (Covertype, CollegeMsg), development, 2

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 2.669 | 2.814 | 95.8 / 102.6 | 256.6 / 289.2 | 17.9 / 17.9 | — | 0/2 |
| zstd -19 | 3.737 | 3.845 | 3.5 / 2.8 | 229.5 / 244.7 | 64.2 / 90.3 | 0/2 | 0/2 |
| OpenZL typed/csv -9 (archive rival) | 5.666 | 4.801 | 15.4 / 15.5 | 99.3 / 104.9 | 65.7 / 97.3 | 0/2 | — |
| Kolmo fast (production) | 8.163 | 6.836 | 96.4 / 104.4 | 200.4 / 221.2 | 20.1 / 22.2 | 1/2 | 2/2 |
| Kolmo fast -q | 8.163 | 6.836 | 99.1 / 107.5 | 200.3 / 219.6 | 20.1 / 22.2 | 1/2 | 2/2 |
| Kolmo eco (production) | 8.957 | 7.345 | 32.2 / 33.9 | 148.3 / 157.8 | 46.9 / 65.4 | 0/2 | 2/2 |
| Kolmo hc (production) | 9.133 | 7.47 | 13.9 / 11.8 | 190.3 / 206.6 | 75.2 / 123.3 | 0/2 | 2/2 |
| Kolmo max (production) | 9.43 | 7.667 | 7.5 / 5.2 | 102.0 / 113.1 | 60.3 / 101.0 | 0/2 | 1/2 |
| Kolmo ultra -l1 | 10.4 | 8.66 | 0.3 / 0.3 | 0.6 / 0.6 | 2831.1 / 3909.5 | 0/2 | 0/2 |
| mcsv_prod19 | 9.027 | 7.384 | 12.3 / 10.3 | 189.8 / 206.7 | 24.6 / 31.4 | 0/2 | 2/2 |
| mcsv_prod3 | 8.116 | 6.694 | 103.8 / 108.7 | 167.8 / 185.2 | 22.8 / 27.6 | 2/2 | 2/2 |
| mcsv_planes_huf | 6.872 | 6.011 | 132.1 / 139.0 | 175.1 / 191.3 | 21.5 / 25.2 | 2/2 | 2/2 |
| tf_planes_huf | 2.673 | 2.816 | 105.7 / 111.4 | 276.0 / 305.1 | 17.9 / 17.9 | 1/2 | 0/2 |
| tf_planes_huf_pg16 | 2.673 | 2.816 | 106.1 / 112.4 | 276.2 / 306.8 | 17.9 / 17.9 | 1/2 | 0/2 |

### E1. Fresh real-world CSV, FiveThirtyEight, 24

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 4.666 | 4.878 | 74.4 / 137.9 | 106.7 / 230.5 | 21.7 / 22.0 | — | 0/23 |
| OpenZL typed/csv -9 (archive rival) | 4.975 | 5.839 | 17.2 / 26.9 | 57.7 / 110.4 | 21.7 / 55.1 | 0/23 | — |
| Kolmo fast (production) | 5.02 | 5.368 | 47.2 / 107.2 | 105.1 / 217.8 | 21.7 / 22.0 | 0/24 | 0/23 |
| Kolmo hc (production) | 6.362 | 6.894 | 3.8 / 3.8 | 104.9 / 210.3 | 21.9 / 94.3 | 0/24 | 0/23 |
| Kolmo max (production) | 7.147 | 7.892 | 4.0 / 4.4 | 45.9 / 65.5 | 22.0 / 68.1 | 0/24 | 0/23 |
| mcsv_prod19 | 6.156 | 6.527 | 3.1 / 3.1 | 122.9 / 254.4 | 22.0 / 88.5 | 0/24 | 0/23 |
| mcsv_planes_huf | 4.814 | 4.914 | 72.7 / 127.9 | 125.7 / 250.4 | 21.7 / 22.0 | 4/24 | 1/23 |
| tp_phuf_pg16 | 4.615 | 4.842 | 84.5 / 158.1 | 131.0 / 277.0 | 21.6 / 22.0 | 1/24 | 0/23 |
| tp_phuf | 4.615 | 4.842 | 88.3 / 156.8 | 117.6 / 272.9 | 21.8 / 22.0 | 1/24 | 0/23 |
| tb_lazy | 4.615 | 4.842 | 82.7 / 156.5 | 116.2 / 269.3 | 21.7 / 22.0 | 1/24 | 0/23 |
| tb_best | 4.615 | 4.842 | 87.0 / 157.9 | 127.8 / 274.1 | 21.8 / 22.0 | 1/24 | 0/23 |
| tp_pent | 4.615 | 4.842 | 88.2 / 157.2 | 125.2 / 272.8 | 21.7 / 22.0 | 1/24 | 0/23 |
| tp_arch19 | 4.615 | 4.842 | 82.9 / 154.9 | 121.6 / 273.8 | 21.8 / 22.0 | 1/24 | 0/23 |

### E2. Fresh real-world CSV, TidyTuesday + selva86/dsdojo, 158 (P2-E22/23/24/26/27)

| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |
|---|---|---|---|---|---|---|---|
| zstd -3 (speed rival) | 4.713 | 5.088 | 92.0 / 131.9 | 165.9 / 244.2 | 17.6 / 18.2 | — | 13/145 |
| OpenZL typed/csv -9 (archive rival) | 6.392 | 6.864 | 22.4 / 25.8 | 86.8 / 117.9 | 19.4 / 102.0 | 0/145 | — |
| Kolmo fast (production) | 5.592 | 6.289 | 70.3 / 93.9 | 149.8 / 217.0 | 17.6 / 18.2 | 2/148 | 39/145 |
| Kolmo eco (production) | 6.721 | 7.579 | 16.8 / 20.7 | 122.3 / 167.4 | 24.1 / 60.2 | 0/148 | 113/145 |
| Kolmo hc (production) | 6.776 | 7.829 | 4.5 / 5.0 | 165.7 / 229.7 | 25.6 / 114.4 | 0/84 | 4/82 |
| pick | 7.406 | 7.939 | 20.8 / 27.7 | 104.7 / 149.3 | 18.8 / 62.8 | 0/64 | 59/63 |
| tt_bsc0 | 7.011 | 8.242 | 28.0 / 36.4 | 38.1 / 50.8 | 17.7 / 24.0 | 0/84 | 48/82 |
| tt_zstd12 | 6.469 | 7.339 | 29.1 / 34.1 | 132.2 / 175.5 | 17.7 / 55.9 | 0/148 | 109/145 |
| tt_brotli9 | 6.244 | 7.355 | 13.4 / 17.7 | 129.2 / 157.3 | 37.2 / 52.8 | 0/84 | 38/82 |
