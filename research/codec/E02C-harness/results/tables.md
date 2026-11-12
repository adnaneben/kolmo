
### Regime A (typed, kg_encode_num at zstd level 3), real inputs: total stored bytes (n=17; input 7896988 B; production typed payload 3054649 B; zstd -3 whole input 3560281 B)

| config | bytes | vs production payload | vs exist, same backend | inputs smaller/larger/equal vs production |
|---|---|---|---|---|
| exist+store | 10747079 | +251.83% | +0.00% | 0/17/0 |
| exist+huf | 3663608 | +19.94% | +0.00% | 0/17/0 |
| exist+rans | 7694594 | +151.90% | +0.00% | 1/16/0 |
| exist+zstd3 | 3055253 | +0.02% | +0.00% | 0/17/0 |
| gbp+store | 7104269 | +132.57% | -33.90% | 0/17/0 |
| gbp+huf | 4183149 | +36.94% | +14.18% | 0/17/0 |
| gbp+rans | 4105470 | +34.40% | -46.64% | 1/16/0 |
| gbp+zstd3 | 3825113 | +25.22% | +25.20% | 3/14/0 |
| lbp128+store | 4840918 | +58.48% | -54.96% | 0/17/0 |
| lbp128+huf | 3713962 | +21.58% | +1.37% | 0/17/0 |
| lbp128+rans | 3721165 | +21.82% | -51.64% | 0/17/0 |
| lbp128+zstd3 | 3562730 | +16.63% | +16.61% | 1/16/0 |
| gpp+store | 4736514 | +55.06% | -55.93% | 1/16/0 |
| gpp+huf | 3684635 | +20.62% | +0.57% | 3/14/0 |
| gpp+rans | 3705824 | +21.32% | -51.84% | 2/15/0 |
| gpp+zstd3 | 3462239 | +13.34% | +13.32% | 3/14/0 |
| lpp128+store | 4230023 | +38.48% | -60.64% | 1/16/0 |
| lpp128+huf | 3575836 | +17.06% | -2.40% | 3/14/0 |
| lpp128+rans | 3593403 | +17.64% | -53.30% | 3/14/0 |
| lpp128+zstd3 | 3452234 | +13.02% | +12.99% | 3/14/0 |
| sel+store | 4228891 | +38.44% | -60.65% | 1/16/0 |
| sel+huf | 3417582 | +11.88% | -6.72% | 4/13/0 |
| sel+rans | 3538895 | +15.85% | -54.01% | 4/13/0 |
| sel+zstd3 | 2989480 | -2.13% | -2.15% | 6/11/0 |

### Regime A (typed, kg_encode_num at zstd level 3), synthetic inputs: total stored bytes (n=3; input 5200384 B; production typed payload 860984 B; zstd -3 whole input 1372378 B)

| config | bytes | vs production payload | vs exist, same backend | inputs smaller/larger/equal vs production |
|---|---|---|---|---|
| exist+store | 4300381 | +399.47% | +0.00% | 0/3/0 |
| exist+huf | 1189315 | +38.13% | +0.00% | 0/3/0 |
| exist+rans | 1084351 | +25.94% | +0.00% | 1/2/0 |
| exist+zstd3 | 861177 | +0.02% | +0.00% | 0/3/0 |
| gbp+store | 3625385 | +321.07% | -15.70% | 0/3/0 |
| gbp+huf | 1424482 | +65.45% | +19.77% | 0/3/0 |
| gbp+rans | 1368710 | +58.97% | +26.22% | 0/3/0 |
| gbp+zstd3 | 1234630 | +43.40% | +43.37% | 0/3/0 |
| lbp128+store | 1049327 | +21.88% | -75.60% | 1/2/0 |
| lbp128+huf | 938676 | +9.02% | -21.07% | 1/2/0 |
| lbp128+rans | 938457 | +9.00% | -13.45% | 1/2/0 |
| lbp128+zstd3 | 895679 | +4.03% | +4.01% | 1/2/0 |
| gpp+store | 862483 | +0.17% | -79.94% | 2/1/0 |
| gpp+huf | 845797 | -1.76% | -28.88% | 2/1/0 |
| gpp+rans | 843219 | -2.06% | -22.24% | 2/1/0 |
| gpp+zstd3 | 844444 | -1.92% | -1.94% | 2/1/0 |
| lpp128+store | 868864 | +0.92% | -79.80% | 2/1/0 |
| lpp128+huf | 843480 | -2.03% | -29.08% | 2/1/0 |
| lpp128+rans | 851181 | -1.14% | -21.50% | 2/1/0 |
| lpp128+zstd3 | 841602 | -2.25% | -2.27% | 2/1/0 |
| sel+store | 856787 | -0.49% | -80.08% | 2/1/0 |
| sel+huf | 829849 | -3.62% | -30.22% | 2/1/0 |
| sel+rans | 826564 | -4.00% | -23.77% | 3/0/0 |
| sel+zstd3 | 828087 | -3.82% | -3.84% | 2/1/0 |

### Regime A per family: change vs production typed payload (fixed configurations)

| family | n | production B | exist+huf | exist+rans | lbp128+rans | gpp+rans | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+rans | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|---|---|
| audio_s16 | 9 | 2514445 | +13.59% | +177.83% | +22.53% | +24.53% | +18.60% | +19.20% | +16.43% | +17.95% | exist+zstd3 |
| fs_meta | 3 | 406698 | +59.52% | +32.85% | +19.14% | +6.36% | +10.36% | +10.65% | -5.18% | +6.08% | gpp+zstd3 |
| raster_16 | 3 | 130660 | +18.96% | +26.07% | +16.89% | +6.40% | +8.63% | +9.57% | +4.35% | +6.22% | exist+zstd3 |
| series_64 | 2 | 2846 | +16.80% | +28.18% | +7.91% | +3.20% | +0.11% | +4.29% | -1.51% | +0.32% | gpp+zstd3 |
| synthetic | 3 | 860984 | +38.13% | +25.94% | +9.00% | -2.06% | -2.03% | -1.14% | -2.25% | -4.00% | lpp128+zstd3 |

### Regime B (complete planner, fast), real inputs: complete-input stored bytes (n=17, 12 with INT blocks; input 7896988 B; production frame 3150852 B; zstd -3 3560281 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 5398185 | +71.32% | +0.00% | 0/12/5 |
| exist+huf | 3353365 | +6.43% | +0.00% | 0/12/5 |
| exist+rans | 3433358 | +8.97% | +0.00% | 1/11/5 |
| exist+zstd3 | 3151304 | +0.01% | +0.00% | 0/12/5 |
| gbp+store | 4490782 | +42.53% | -16.81% | 0/12/5 |
| gbp+huf | 3477502 | +10.37% | +3.70% | 0/12/5 |
| gbp+rans | 3439416 | +9.16% | +0.18% | 1/11/5 |
| gbp+zstd3 | 3385222 | +7.44% | +7.42% | 2/10/5 |
| lbp128+store | 3629854 | +15.20% | -32.76% | 0/12/5 |
| lbp128+huf | 3319145 | +5.34% | -1.02% | 0/12/5 |
| lbp128+rans | 3314018 | +5.18% | -3.48% | 0/12/5 |
| lbp128+zstd3 | 3286039 | +4.29% | +4.28% | 0/12/5 |
| gpp+store | 3569846 | +13.30% | -33.87% | 1/11/5 |
| gpp+huf | 3282283 | +4.17% | -2.12% | 2/10/5 |
| gpp+rans | 3283179 | +4.20% | -4.37% | 1/11/5 |
| gpp+zstd3 | 3224562 | +2.34% | +2.32% | 2/10/5 |
| lpp128+store | 3444632 | +9.32% | -36.19% | 1/11/5 |
| lpp128+huf | 3275405 | +3.95% | -2.32% | 2/10/5 |
| lpp128+rans | 3277991 | +4.04% | -4.53% | 2/10/5 |
| lpp128+zstd3 | 3242694 | +2.91% | +2.90% | 2/10/5 |
| sel+store | 3444611 | +9.32% | -36.19% | 1/11/5 |
| sel+huf | 3243915 | +2.95% | -3.26% | 3/9/5 |
| sel+rans | 3250605 | +3.17% | -5.32% | 3/9/5 |
| sel+zstd3 | 3144442 | -0.20% | -0.22% | 5/7/5 |

### Regime B (complete planner, fast), synthetic inputs: complete-input stored bytes (n=3, 2 with INT blocks; input 5200384 B; production frame 858799 B; zstd -3 1372378 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 2687465 | +212.93% | +0.00% | 0/2/1 |
| exist+huf | 997826 | +16.19% | +0.00% | 0/2/1 |
| exist+rans | 1032813 | +20.26% | +0.00% | 1/1/1 |
| exist+zstd3 | 869645 | +1.26% | +0.00% | 0/2/1 |
| gbp+store | 2074971 | +141.61% | -22.79% | 0/2/1 |
| gbp+huf | 1049539 | +22.21% | +5.18% | 0/2/1 |
| gbp+rans | 1019248 | +18.68% | -1.31% | 0/2/1 |
| gbp+zstd3 | 969366 | +12.87% | +11.47% | 0/2/1 |
| lbp128+store | 874102 | +1.78% | -67.47% | 1/1/1 |
| lbp128+huf | 859191 | +0.05% | -13.89% | 1/1/1 |
| lbp128+rans | 861837 | +0.35% | -16.55% | 1/1/1 |
| lbp128+zstd3 | 852252 | -0.76% | -2.00% | 1/1/1 |
| gpp+store | 871655 | +1.50% | -67.57% | 1/1/1 |
| gpp+huf | 855254 | -0.41% | -14.29% | 1/1/1 |
| gpp+rans | 853338 | -0.64% | -17.38% | 1/1/1 |
| gpp+zstd3 | 853876 | -0.57% | -1.81% | 1/1/1 |
| lpp128+store | 870678 | +1.38% | -67.60% | 1/1/1 |
| lpp128+huf | 852794 | -0.70% | -14.53% | 1/1/1 |
| lpp128+rans | 857138 | -0.19% | -17.01% | 1/1/1 |
| lpp128+zstd3 | 851042 | -0.90% | -2.14% | 1/1/1 |
| sel+store | 865960 | +0.83% | -67.78% | 1/1/1 |
| sel+huf | 839235 | -2.28% | -15.89% | 1/1/1 |
| sel+rans | 835899 | -2.67% | -19.07% | 2/0/1 |
| sel+zstd3 | 848259 | -1.23% | -2.46% | 1/1/1 |

### Regime B (complete planner, hc), real inputs: complete-input stored bytes (n=17, 8 with INT blocks; input 7896988 B; production frame 2562687 B; zstd -3 3560281 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 3930315 | +53.37% | +0.00% | 0/8/9 |
| exist+huf | 2742674 | +7.02% | +0.00% | 0/8/9 |
| exist+rans | 2766388 | +7.95% | +0.00% | 0/8/9 |
| exist+zstd3 | 2589680 | +1.05% | +0.00% | 0/8/9 |
| gbp+store | 3544609 | +38.32% | -9.81% | 0/8/9 |
| gbp+huf | 2809515 | +9.63% | +2.44% | 0/8/9 |
| gbp+rans | 2774515 | +8.27% | +0.29% | 0/8/9 |
| gbp+zstd3 | 2750667 | +7.34% | +6.22% | 0/8/9 |
| lbp128+store | 2851989 | +11.29% | -27.44% | 0/8/9 |
| lbp128+huf | 2675123 | +4.39% | -2.46% | 0/8/9 |
| lbp128+rans | 2672050 | +4.27% | -3.41% | 0/8/9 |
| lbp128+zstd3 | 2656553 | +3.66% | +2.58% | 0/8/9 |
| gpp+store | 2792218 | +8.96% | -28.96% | 0/8/9 |
| gpp+huf | 2648195 | +3.34% | -3.44% | 1/7/9 |
| gpp+rans | 2648946 | +3.37% | -4.25% | 0/8/9 |
| gpp+zstd3 | 2608320 | +1.78% | +0.72% | 0/8/9 |
| lpp128+store | 2706268 | +5.60% | -31.14% | 0/8/9 |
| lpp128+huf | 2646623 | +3.28% | -3.50% | 1/7/9 |
| lpp128+rans | 2648770 | +3.36% | -4.25% | 0/8/9 |
| lpp128+zstd3 | 2624023 | +2.39% | +1.33% | 0/8/9 |
| sel+store | 2706247 | +5.60% | -31.14% | 0/8/9 |
| sel+huf | 2634034 | +2.78% | -3.96% | 1/7/9 |
| sel+rans | 2635012 | +2.82% | -4.75% | 0/8/9 |
| sel+zstd3 | 2585341 | +0.88% | -0.17% | 0/8/9 |

### Regime B (complete planner, hc), synthetic inputs: complete-input stored bytes (n=3, 3 with INT blocks; input 5200384 B; production frame 859023 B; zstd -3 1372378 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 4300534 | +400.63% | +0.00% | 0/3/0 |
| exist+huf | 1189468 | +38.47% | +0.00% | 0/3/0 |
| exist+rans | 1084504 | +26.25% | +0.00% | 1/2/0 |
| exist+zstd3 | 861330 | +0.27% | +0.00% | 1/2/0 |
| gbp+store | 3625538 | +322.05% | -15.70% | 0/3/0 |
| gbp+huf | 1424635 | +65.84% | +19.77% | 0/3/0 |
| gbp+rans | 1368863 | +59.35% | +26.22% | 0/3/0 |
| gbp+zstd3 | 1234783 | +43.74% | +43.36% | 0/3/0 |
| lbp128+store | 1049480 | +22.17% | -75.60% | 1/2/0 |
| lbp128+huf | 938829 | +9.29% | -21.07% | 1/2/0 |
| lbp128+rans | 938610 | +9.26% | -13.45% | 1/2/0 |
| lbp128+zstd3 | 895832 | +4.28% | +4.01% | 1/2/0 |
| gpp+store | 862636 | +0.42% | -79.94% | 2/1/0 |
| gpp+huf | 845950 | -1.52% | -28.88% | 2/1/0 |
| gpp+rans | 843372 | -1.82% | -22.23% | 2/1/0 |
| gpp+zstd3 | 844597 | -1.68% | -1.94% | 2/1/0 |
| lpp128+store | 869017 | +1.16% | -79.79% | 2/1/0 |
| lpp128+huf | 843633 | -1.79% | -29.07% | 2/1/0 |
| lpp128+rans | 851334 | -0.90% | -21.50% | 2/1/0 |
| lpp128+zstd3 | 841755 | -2.01% | -2.27% | 2/1/0 |
| sel+store | 856940 | -0.24% | -80.07% | 2/1/0 |
| sel+huf | 830002 | -3.38% | -30.22% | 2/1/0 |
| sel+rans | 826717 | -3.76% | -23.77% | 3/0/0 |
| sel+zstd3 | 828240 | -3.58% | -3.84% | 2/1/0 |

### Regime B per input: production planner selection

| input | mode | origin | bytes | frame | zstd -3 | INT blocks | prod residual codec | INT shape | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|
| audio_concat_s16.raw | fast | derived | 2990280 | 1871368 | 1868162 | 0/3 | - | - | - |
| audio_concat_s16.raw | hc | derived | 2990280 | 1425240 | 1868162 | 0/1 | - | - | - |
| audio_curve.wav | fast | real | 134660 | 83539 | 103632 | 1/1 | zstd | w2o1ow2 | exist+zstd3 |
| audio_curve.wav | hc | real | 134660 | 75954 | 103632 | 1/1 | zstd | w2o1ow2 | exist+zstd3 |
| audio_drama.wav | fast | real | 162366 | 93402 | 98330 | 1/1 | zstd | w2o1ow4 | exist+zstd3 |
| audio_drama.wav | hc | real | 162366 | 76273 | 98330 | 0/1 | - | - | - |
| audio_gong.wav | fast | real | 191292 | 120058 | 120006 | 0/1 | - | - | - |
| audio_gong.wav | hc | real | 191292 | 102839 | 120006 | 0/1 | - | - | - |
| audio_roll.wav | fast | real | 242648 | 173740 | 229159 | 1/1 | zstd | w2o2ow4 | lpp128+rans |
| audio_roll.wav | hc | real | 242648 | 169810 | 229159 | 1/1 | zstd | w2o2ow4 | lpp128+rans |
| audio_soft.wav | fast | real | 185752 | 95227 | 105852 | 1/1 | zstd | w2o1ow4 | exist+zstd3 |
| audio_soft.wav | hc | real | 185752 | 75842 | 105852 | 0/1 | - | - | - |
| audio_space2.wav | fast | real | 192686 | 75175 | 101803 | 1/1 | zstd | w2o2ow4 | exist+zstd3 |
| audio_space2.wav | hc | real | 192686 | 55691 | 101803 | 0/1 | - | - | - |
| audio_space3.wav | fast | real | 144748 | 56412 | 64267 | 1/1 | huf | w2o1ow2 | gbp+zstd3 |
| audio_space3.wav | hc | real | 144748 | 42289 | 64267 | 0/1 | - | - | - |
| audio_untie.wav | fast | real | 190884 | 115979 | 115927 | 0/1 | - | - | - |
| audio_untie.wav | hc | real | 190884 | 92404 | 115927 | 0/1 | - | - | - |
| dem_jacksboro_i16.npy | fast | real | 277392 | 92978 | 165635 | 0/1 | - | - | - |
| dem_jacksboro_i16.npy | hc | real | 277392 | 93171 | 165635 | 0/1 | - | - | - |
| fs_inode_u64.npy | fast | derived | 1205616 | 38636 | 199898 | 2/2 | zstd | w8o2ow4 | gpp+zstd3 |
| fs_inode_u64.npy | hc | derived | 1205616 | 33213 | 199898 | 1/1 | zstd | w8o2ow4 | gpp+zstd3 |
| fs_mtime_i64.npy | fast | derived | 1205616 | 51727 | 53269 | 0/2 | - | - | - |
| fs_mtime_i64.npy | hc | derived | 1205616 | 47267 | 53269 | 0/1 | - | - | - |
| fs_size_u32.npy | fast | derived | 602872 | 245334 | 281940 | 1/1 | zstd | w4o1ow4 | exist+zstd3 |
| fs_size_u32.npy | hc | derived | 602872 | 236512 | 281940 | 1/1 | zstd | w4o1ow4 | exist+zstd3 |
| goog_date_i64.npy | fast | real | 8504 | 326 | 1199 | 1/1 | zstd | w8o2ow1 | gbp+zstd3 |
| goog_date_i64.npy | hc | real | 8504 | 262 | 1199 | 1/1 | zstd | w8o2ow1 | gbp+zstd3 |
| goog_volume_i64.npy | fast | real | 8504 | 2624 | 3582 | 1/1 | zstd | w8o1ow4 | gpp+huf |
| goog_volume_i64.npy | hc | real | 8504 | 2534 | 3582 | 1/1 | zstd | w8o1ow4 | gpp+huf |
| mri_s1045_u16.npy | fast | real | 131200 | 21797 | 32903 | 1/1 | zstd | w2o2ow1 | exist+zstd3 |
| mri_s1045_u16.npy | hc | real | 131200 | 21363 | 32903 | 1/1 | zstd | w2o2ow1 | exist+zstd3 |
| syn_clustered_outliers_u32.npy | fast | synthetic | 2000128 | 303878 | 445558 | 2/2 | zstd | w4o0ow4 | lpp128+zstd3 |
| syn_clustered_outliers_u32.npy | hc | synthetic | 2000128 | 303194 | 445558 | 1/1 | zstd | w4o0ow4 | lpp128+zstd3 |
| syn_sparse_outliers_u32.npy | fast | synthetic | 2000128 | 386987 | 539580 | 0/2 | - | - | - |
| syn_sparse_outliers_u32.npy | hc | synthetic | 2000128 | 387951 | 539580 | 1/1 | zstd | w4o0ow4 | gpp+huf |
| syn_walk_i32.npy | fast | synthetic | 1200128 | 167934 | 387240 | 2/2 | huf | w4o1ow1 | exist+rans |
| syn_walk_i32.npy | hc | synthetic | 1200128 | 167878 | 387240 | 1/1 | huf | w4o1ow1 | exist+rans |
| topobathy_i16.npy | fast | real | 21968 | 12530 | 14717 | 1/1 | zstd | w2o1ow2 | gpp+huf |
| topobathy_i16.npy | hc | real | 21968 | 12023 | 14717 | 1/1 | zstd | w2o1ow2 | gpp+huf |

### Regime B per input with INT blocks: complete-input change vs production frame

| input | mode | origin | production frame | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| audio_curve.wav | fast | real | 83539 | +0.01% | +14.27% | +13.31% | +9.67% | +10.16% | +8.72% | +9.67% | +10.16% | +0.01% |
| audio_curve.wav | hc | real | 75954 | +10.00% | +25.69% | +24.63% | +20.62% | +21.16% | +19.58% | +20.62% | +21.16% | +10.00% |
| audio_drama.wav | fast | real | 93402 | +0.01% | +22.54% | +18.92% | +20.13% | +19.85% | +19.51% | +20.13% | +19.85% | +0.01% |
| audio_roll.wav | fast | real | 173740 | +0.00% | +3.76% | +4.34% | -1.66% | -1.73% | -1.19% | -1.66% | -1.73% | -1.19% |
| audio_roll.wav | hc | real | 169810 | +2.32% | +6.16% | +6.76% | +0.62% | +0.55% | +1.09% | +0.62% | +0.55% | +1.09% |
| audio_soft.wav | fast | real | 95227 | +0.01% | +26.43% | +21.80% | +22.14% | +22.01% | +21.26% | +18.06% | +22.01% | +0.01% |
| audio_space2.wav | fast | real | 75175 | +0.01% | +20.70% | +11.89% | +24.66% | +24.54% | +16.60% | +17.02% | +20.44% | +0.01% |
| audio_space3.wav | fast | real | 56412 | +0.64% | +19.90% | +13.00% | +16.07% | +16.59% | +12.72% | +0.01% | -1.11% | -4.36% |
| fs_inode_u64.npy | fast | derived | 38636 | +0.04% | +12.06% | -4.74% | +34.56% | +38.01% | +11.55% | +12.06% | +12.58% | -4.74% |
| fs_inode_u64.npy | hc | derived | 33213 | +16.61% | +31.07% | +10.15% | +56.58% | +58.74% | +32.10% | +31.07% | +30.84% | +10.15% |
| fs_size_u32.npy | fast | derived | 245334 | +0.00% | +12.84% | +0.63% | +12.48% | +12.48% | +8.94% | +12.48% | +12.48% | +0.00% |
| fs_size_u32.npy | hc | derived | 236512 | +3.73% | +17.05% | +4.38% | +16.68% | +16.67% | +13.00% | +16.68% | +16.67% | +3.73% |
| goog_date_i64.npy | fast | real | 326 | +1.84% | +27.61% | +4.29% | +32.21% | +61.04% | +10.74% | +16.87% | +29.45% | -5.83% |
| goog_date_i64.npy | hc | real | 262 | +26.34% | +58.40% | +29.39% | +64.12% | +100.00% | +37.40% | +45.04% | +60.69% | +16.79% |
| goog_volume_i64.npy | fast | real | 2624 | +0.23% | -4.42% | -3.24% | -3.89% | -2.93% | -2.97% | -4.42% | -3.32% | -3.24% |
| goog_volume_i64.npy | hc | real | 2534 | +3.75% | -1.07% | +0.16% | -0.51% | +0.47% | +0.43% | -1.07% | +0.08% | +0.16% |
| mri_s1045_u16.npy | fast | real | 21797 | +0.04% | +17.64% | +2.79% | +35.91% | +38.83% | +8.92% | +17.64% | +19.05% | +0.04% |
| mri_s1045_u16.npy | hc | real | 21363 | +2.06% | +20.03% | +4.87% | +38.66% | +41.64% | +11.12% | +20.03% | +21.46% | +2.06% |
| syn_clustered_outliers_u32.npy | fast | synthetic | 303878 | +0.01% | -5.62% | -6.07% | -6.44% | -6.60% | -7.03% | -6.44% | -6.60% | -7.03% |
| syn_clustered_outliers_u32.npy | hc | synthetic | 303194 | +0.81% | -5.44% | -5.89% | -6.27% | -6.45% | -6.91% | -6.27% | -6.45% | -6.91% |
| syn_sparse_outliers_u32.npy | hc | synthetic | 387951 | -0.08% | -2.59% | -2.58% | -2.54% | -1.56% | -2.53% | -2.59% | -2.54% | -2.58% |
| syn_walk_i32.npy | fast | synthetic | 167934 | +6.45% | +8.07% | +8.05% | +8.08% | +10.96% | +8.11% | +0.01% | -1.69% | +6.45% |
| syn_walk_i32.npy | hc | synthetic | 167878 | +0.11% | +8.02% | +8.00% | +8.03% | +10.68% | +8.03% | +0.00% | -1.74% | +0.11% |
| topobathy_i16.npy | fast | real | 12530 | +0.06% | -0.03% | +0.74% | +0.52% | +3.66% | +1.61% | -0.03% | +3.10% | +0.06% |
| topobathy_i16.npy | hc | real | 12023 | +4.27% | +4.18% | +4.98% | +4.75% | +8.02% | +5.89% | +4.18% | +7.44% | +4.27% |

### Regime A per input: typed payload change vs production typed payload

| input | origin | production payload | INT shape | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| audio_concat_s16.raw | derived | 1684209 | o1ow4 | +0.00% | +27.07% | +20.08% | +20.93% | +21.80% | +18.12% | +15.23% | +21.80% | +0.00% |
| audio_curve.wav | real | 83489 | o1ow2 | +0.01% | +14.29% | +13.24% | +9.68% | +10.15% | +8.78% | +9.68% | +10.15% | +0.01% |
| audio_drama.wav | real | 93650 | o1ow4 | +0.01% | +22.13% | +18.64% | +19.74% | +19.42% | +19.71% | +19.74% | +19.42% | +0.01% |
| audio_gong.wav | real | 130622 | o2ow4 | +0.01% | +21.93% | +21.59% | +19.21% | +19.33% | +19.12% | +18.89% | +19.33% | +0.01% |
| audio_roll.wav | real | 175945 | o2ow2 | +0.00% | +2.42% | +2.99% | -2.94% | -3.02% | -2.47% | -2.94% | -3.02% | -2.47% |
| audio_soft.wav | real | 95059 | o1ow4 | +0.01% | +26.55% | +23.35% | +22.23% | +22.13% | +22.01% | +18.14% | +22.13% | +0.01% |
| audio_space2.wav | real | 76545 | o2ow2 | +0.01% | +18.44% | +9.74% | +22.07% | +21.95% | +14.36% | +2.89% | +7.49% | +0.01% |
| audio_space3.wav | real | 56236 | o1ow2 | +0.85% | +20.08% | +13.17% | +16.17% | +16.63% | +14.34% | +0.01% | -0.97% | -4.24% |
| audio_untie.wav | real | 118690 | o2ow2 | +0.01% | +20.75% | +19.56% | +18.24% | +18.36% | +18.08% | +7.19% | +9.53% | +0.01% |
| dem_jacksboro_i16.npy | real | 96437 | o2ow2 | +0.01% | +3.60% | +3.60% | +3.51% | +3.72% | +3.67% | +3.51% | +3.72% | +0.01% |
| fs_inode_u64.npy | derived | 38670 | o2ow4 | +0.02% | +12.44% | -5.52% | +34.35% | +36.20% | +13.32% | +12.44% | +12.24% | -5.52% |
| fs_mtime_i64.npy | derived | 122746 | o1ow8 | +0.01% | -8.40% | -45.86% | -1.45% | -1.07% | -39.23% | -8.40% | -8.65% | -45.86% |
| fs_size_u32.npy | derived | 245282 | o1ow4 | +0.00% | +12.84% | +0.63% | +12.49% | +12.48% | +8.94% | +12.49% | +12.48% | +0.00% |
| goog_date_i64.npy | real | 274 | o2ow1 | +2.19% | +32.85% | +5.11% | +38.32% | +72.63% | +12.77% | +20.07% | +35.04% | -6.93% |
| goog_volume_i64.npy | real | 2572 | o1ow4 | +0.23% | -4.51% | -3.30% | -3.97% | -2.99% | -3.03% | -4.51% | -3.38% | -3.30% |
| mri_s1045_u16.npy | real | 21745 | o2ow1 | +0.04% | +17.69% | +2.80% | +36.00% | +38.92% | +8.94% | +17.69% | +19.10% | +0.04% |
| syn_clustered_outliers_u32.npy | synthetic | 305584 | o0ow4 | +0.00% | -6.20% | -6.64% | -7.02% | -7.20% | -7.65% | -7.02% | -7.20% | -7.65% |
| syn_sparse_outliers_u32.npy | synthetic | 387573 | o0ow4 | +0.00% | -2.50% | -2.50% | -2.46% | -1.48% | -2.44% | -2.50% | -2.45% | -2.50% |
| syn_walk_i32.npy | synthetic | 167827 | o1ow1 | +0.11% | +8.02% | +8.00% | +8.03% | +10.68% | +8.03% | +0.00% | -1.74% | +0.11% |
| topobathy_i16.npy | real | 12478 | o1ow2 | +0.06% | -0.03% | +0.75% | +0.52% | +3.67% | +1.62% | -0.03% | +3.12% | +0.06% |

### Native kernel timing, regime A typed, real inputs (n=17, 10745303 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 1.68 | 1.68–1.70 | 6390 | 1.68 | 1.31 | 1.31–1.35 | 8179 | 1.31 |
| exist+huf | 12.30 | 12.29–12.98 | 874 | 12.27 | 9.99 | 10.09–10.51 | 1075 | 9.99 |
| exist+rans | 41.21 | 41.34–42.30 | 261 | 41.18 | 13.21 | 13.44–13.90 | 814 | 13.20 |
| exist+zstd3 | 35.48 | 36.50–44.39 | 303 | 35.30 | 9.34 | 9.35–11.88 | 1151 | 9.33 |
| gbp+store | 15.35 | 15.58–18.47 | 700 | 15.15 | 12.17 | 12.29–12.88 | 883 | 12.09 |
| gbp+huf | 23.91 | 23.72–30.10 | 449 | 23.79 | 20.22 | 20.53–23.73 | 531 | 20.20 |
| gbp+rans | 43.87 | 43.56–49.88 | 245 | 43.84 | 35.14 | 35.24–40.35 | 306 | 35.06 |
| gbp+zstd3 | 52.86 | 54.17–54.63 | 203 | 52.63 | 20.46 | 20.48–21.92 | 525 | 20.45 |
| lbp128+store | 14.74 | 14.86–16.74 | 729 | 14.73 | 11.15 | 11.66–13.06 | 964 | 11.14 |
| lbp128+huf | 19.67 | 20.16–25.72 | 546 | 19.65 | 17.36 | 17.56–20.63 | 619 | 17.35 |
| lbp128+rans | 34.37 | 34.77–35.97 | 313 | 34.35 | 28.96 | 29.09–29.53 | 371 | 28.95 |
| lbp128+zstd3 | 34.97 | 34.94–35.97 | 307 | 34.95 | 16.93 | 17.02–17.81 | 635 | 16.88 |
| gpp+store | 25.50 | 26.21–27.21 | 421 | 25.46 | 11.98 | 12.10–12.50 | 897 | 11.96 |
| gpp+huf | 31.53 | 32.36–40.21 | 341 | 31.23 | 18.57 | 18.76–21.20 | 579 | 18.48 |
| gpp+rans | 46.96 | 47.59–53.80 | 229 | 46.35 | 29.38 | 29.68–31.09 | 366 | 29.37 |
| gpp+zstd3 | 55.50 | 52.74–65.04 | 194 | 54.08 | 20.80 | 19.18–21.39 | 516 | 19.97 |
| lpp128+store | 27.02 | 27.92–28.28 | 398 | 26.78 | 11.49 | 11.55–11.81 | 935 | 11.49 |
| lpp128+huf | 33.62 | 32.48–37.06 | 320 | 33.60 | 17.73 | 17.73–19.38 | 606 | 17.60 |
| lpp128+rans | 46.71 | 46.93–55.76 | 230 | 45.28 | 27.43 | 27.37–31.15 | 392 | 27.41 |
| lpp128+zstd3 | 41.59 | 41.17–52.80 | 258 | 41.36 | 16.60 | 16.63–18.55 | 647 | 16.45 |
| sel+store | 85.45 | 86.26–87.81 | 126 | 83.92 | 11.57 | 11.64–11.83 | 929 | 11.50 |
| sel+huf | 118.29 | 121.99–144.59 | 91 | 116.44 | 12.39 | 12.20–13.70 | 867 | 11.93 |
| sel+rans | 212.11 | 217.75–247.10 | 51 | 210.90 | 27.18 | 27.35–29.27 | 395 | 27.10 |
| sel+zstd3 | 215.83 | 214.36–232.77 | 50 | 212.59 | 10.31 | 10.33–10.70 | 1042 | 10.31 |

### Native kernel timing, regime B planner fast, real inputs (n=12, 3144353 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.29 | 0.28–0.30 | 11014 | 0.29 | 0.22 | 0.21–0.23 | 14058 | 0.22 |
| exist+huf | 3.87 | 3.93–4.20 | 813 | 3.87 | 3.07 | 2.97–3.24 | 1023 | 3.07 |
| exist+rans | 13.66 | 13.72–14.52 | 230 | 13.63 | 8.19 | 8.20–8.38 | 384 | 8.19 |
| exist+zstd3 | 9.79 | 10.02–10.59 | 321 | 9.79 | 2.50 | 2.48–2.67 | 1259 | 2.49 |
| gbp+store | 4.88 | 4.85–5.13 | 645 | 4.87 | 3.76 | 3.78–3.85 | 836 | 3.76 |
| gbp+huf | 7.52 | 7.61–7.86 | 418 | 7.53 | 6.50 | 6.51–6.71 | 484 | 6.50 |
| gbp+rans | 15.01 | 15.12–15.70 | 209 | 14.95 | 11.06 | 11.10–11.21 | 284 | 11.03 |
| gbp+zstd3 | 18.25 | 18.79–19.03 | 172 | 18.24 | 6.69 | 6.82–6.94 | 470 | 6.67 |
| lbp128+store | 4.61 | 4.58–4.77 | 682 | 4.61 | 3.42 | 3.44–3.49 | 919 | 3.42 |
| lbp128+huf | 6.25 | 6.50–6.62 | 503 | 6.26 | 5.29 | 5.30–5.59 | 594 | 5.29 |
| lbp128+rans | 11.50 | 11.58–12.20 | 273 | 11.44 | 8.60 | 8.66–9.00 | 366 | 8.60 |
| lbp128+zstd3 | 11.70 | 11.78–12.37 | 269 | 11.68 | 5.48 | 5.44–5.68 | 574 | 5.48 |
| gpp+store | 8.30 | 8.22–8.51 | 379 | 8.30 | 3.65 | 3.63–3.69 | 862 | 3.64 |
| gpp+huf | 9.84 | 9.98–10.35 | 320 | 9.81 | 5.50 | 5.48–5.64 | 572 | 5.50 |
| gpp+rans | 15.96 | 16.20–16.79 | 197 | 15.96 | 8.61 | 8.60–8.80 | 365 | 8.60 |
| gpp+zstd3 | 15.25 | 15.39–16.08 | 206 | 15.22 | 5.59 | 5.54–5.69 | 562 | 5.59 |
| lpp128+store | 9.42 | 9.12–9.76 | 334 | 9.38 | 3.59 | 3.51–3.63 | 877 | 3.58 |
| lpp128+huf | 10.53 | 10.17–11.23 | 299 | 10.47 | 5.20 | 5.13–5.42 | 605 | 5.19 |
| lpp128+rans | 16.21 | 16.13–16.61 | 194 | 16.21 | 8.05 | 7.93–8.21 | 391 | 7.97 |
| lpp128+zstd3 | 14.37 | 14.07–15.22 | 219 | 14.33 | 5.12 | 5.00–5.32 | 614 | 5.12 |
| sel+store | 27.48 | 27.68–28.88 | 114 | 27.44 | 3.47 | 3.50–3.60 | 906 | 3.47 |
| sel+huf | 39.15 | 39.36–40.80 | 80 | 39.01 | 4.42 | 4.46–4.59 | 712 | 4.41 |
| sel+rans | 72.85 | 73.87–74.81 | 43 | 72.60 | 7.91 | 7.84–8.25 | 397 | 7.90 |
| sel+zstd3 | 70.17 | 69.16–70.56 | 45 | 69.66 | 3.34 | 3.33–3.48 | 940 | 3.34 |

### Native kernel timing, regime B planner hc, real inputs (n=8, 1918023 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.17 | 0.17–0.17 | 11378 | 0.17 | 0.14 | 0.14–0.14 | 14141 | 0.14 |
| exist+huf | 2.27 | 2.26–2.57 | 846 | 2.25 | 1.94 | 1.95–2.10 | 988 | 1.93 |
| exist+rans | 8.11 | 8.24–8.85 | 236 | 8.08 | 4.91 | 4.92–5.14 | 391 | 4.85 |
| exist+zstd3 | 4.86 | 4.87–5.57 | 395 | 4.85 | 1.29 | 1.29–1.45 | 1487 | 1.29 |
| gbp+store | 2.82 | 2.85–2.99 | 679 | 2.82 | 2.24 | 2.24–2.29 | 857 | 2.23 |
| gbp+huf | 4.49 | 4.73–5.26 | 428 | 4.49 | 4.00 | 4.07–4.38 | 480 | 3.98 |
| gbp+rans | 9.57 | 9.72–10.08 | 200 | 9.54 | 6.98 | 7.26–7.91 | 275 | 6.98 |
| gbp+zstd3 | 9.91 | 9.91–12.06 | 194 | 9.88 | 3.93 | 3.93–4.30 | 488 | 3.93 |
| lbp128+store | 2.68 | 2.71–3.05 | 716 | 2.65 | 2.02 | 2.04–2.17 | 950 | 2.02 |
| lbp128+huf | 3.58 | 3.58–4.21 | 535 | 3.58 | 3.24 | 3.20–3.56 | 591 | 3.16 |
| lbp128+rans | 6.62 | 6.70–7.42 | 290 | 6.61 | 5.04 | 5.11–5.73 | 380 | 5.04 |
| lbp128+zstd3 | 5.59 | 5.67–6.46 | 343 | 5.54 | 3.06 | 3.13–3.29 | 626 | 3.06 |
| gpp+store | 4.77 | 4.86–5.21 | 402 | 4.74 | 2.14 | 2.13–2.41 | 895 | 2.11 |
| gpp+huf | 5.67 | 5.77–6.42 | 338 | 5.67 | 3.22 | 3.22–3.63 | 596 | 3.20 |
| gpp+rans | 9.39 | 9.73–10.77 | 204 | 9.38 | 4.95 | 5.00–5.52 | 388 | 4.94 |
| gpp+zstd3 | 7.65 | 7.61–8.11 | 251 | 7.59 | 3.07 | 3.10–3.16 | 624 | 3.07 |
| lpp128+store | 5.97 | 5.72–6.62 | 321 | 5.35 | 2.00 | 2.04–2.31 | 961 | 1.99 |
| lpp128+huf | 5.96 | 6.00–7.41 | 322 | 5.96 | 2.94 | 2.95–3.49 | 652 | 2.94 |
| lpp128+rans | 9.16 | 9.30–10.40 | 209 | 9.12 | 4.45 | 4.48–4.71 | 431 | 4.44 |
| lpp128+zstd3 | 7.65 | 7.65–8.11 | 251 | 7.60 | 2.78 | 2.82–3.03 | 689 | 2.78 |
| sel+store | 15.88 | 16.83–17.95 | 121 | 15.81 | 2.02 | 2.13–2.26 | 951 | 2.01 |
| sel+huf | 22.34 | 22.80–27.11 | 86 | 22.28 | 3.05 | 3.03–3.65 | 629 | 3.01 |
| sel+rans | 42.71 | 43.30–47.31 | 45 | 42.54 | 4.51 | 4.53–4.70 | 425 | 4.50 |
| sel+zstd3 | 35.97 | 35.57–39.02 | 53 | 35.27 | 1.85 | 1.87–2.06 | 1037 | 1.83 |

### End-to-end, complete planner fast, real inputs with INT blocks (n=12, 3041524 B). Encode for experimental rows = production compress - production residual-leaf re-encode (9.2 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 898742 | 120.5 | 8.52 | 8.32 |
| zstd -3 whole input (direct control) | 1237282 | 15.9 | 4.58 | - |
| exist+store | 3146075 | 111.7 | - | 6.40 |
| exist+huf | 1101255 | 115.2 | - | 9.07 |
| exist+rans | 1181248 | 124.9 | - | 14.38 |
| exist+zstd3 | 899194 | 121.8 | - | 8.60 |
| gbp+store | 2238672 | 116.1 | - | 9.84 |
| gbp+huf | 1225392 | 118.8 | - | 12.46 |
| gbp+rans | 1187306 | 126.3 | - | 16.87 |
| gbp+zstd3 | 1133112 | 129.7 | - | 12.78 |
| lbp128+store | 1377744 | 115.9 | - | 9.46 |
| lbp128+huf | 1067035 | 117.7 | - | 11.61 |
| lbp128+rans | 1061908 | 123.2 | - | 14.94 |
| lbp128+zstd3 | 1033929 | 123.1 | - | 11.39 |
| gpp+store | 1317736 | 119.9 | - | 9.63 |
| gpp+huf | 1030173 | 121.4 | - | 11.45 |
| gpp+rans | 1031069 | 128.0 | - | 14.70 |
| gpp+zstd3 | 972452 | 126.9 | - | 11.65 |
| lpp128+store | 1192522 | 120.3 | - | 9.43 |
| lpp128+huf | 1023295 | 121.9 | - | 11.21 |
| lpp128+rans | 1025881 | 127.8 | - | 14.44 |
| lpp128+zstd3 | 990584 | 126.0 | - | 11.22 |
| sel+store | 1192501 | 139.1 | - | 9.63 |
| sel+huf | 991805 | 149.9 | - | 10.47 |
| sel+rans | 998495 | 184.3 | - | 13.71 |
| sel+zstd3 | 892332 | 181.1 | - | 9.26 |

### End-to-end, complete planner hc, real inputs with INT blocks (n=8, 2355972 B). Encode for experimental rows = production compress - production residual-leaf re-encode (278.4 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 551671 | 439.3 | 5.04 | 5.17 |
| zstd -3 whole input (direct control) | 867030 | 9.9 | 3.17 | - |
| exist+store | 1919299 | 161.2 | - | 3.90 |
| exist+huf | 731658 | 163.3 | - | 5.59 |
| exist+rans | 755372 | 169.2 | - | 8.50 |
| exist+zstd3 | 578664 | 166.4 | - | 5.07 |
| gbp+store | 1533593 | 163.8 | - | 6.00 |
| gbp+huf | 798499 | 165.5 | - | 7.65 |
| gbp+rans | 763499 | 170.4 | - | 10.80 |
| gbp+zstd3 | 739651 | 171.4 | - | 7.65 |
| lbp128+store | 840973 | 163.6 | - | 5.73 |
| lbp128+huf | 664107 | 164.6 | - | 6.84 |
| lbp128+rans | 661034 | 167.6 | - | 8.69 |
| lbp128+zstd3 | 645537 | 166.8 | - | 6.80 |
| gpp+store | 781202 | 165.9 | - | 5.75 |
| gpp+huf | 637179 | 166.7 | - | 6.86 |
| gpp+rans | 637930 | 170.3 | - | 8.47 |
| gpp+zstd3 | 597304 | 169.1 | - | 6.85 |
| lpp128+store | 695252 | 166.1 | - | 5.58 |
| lpp128+huf | 635607 | 167.3 | - | 6.76 |
| lpp128+rans | 637754 | 170.1 | - | 8.04 |
| lpp128+zstd3 | 613007 | 168.8 | - | 6.49 |
| sel+store | 695231 | 177.1 | - | 5.79 |
| sel+huf | 623018 | 183.4 | - | 6.64 |
| sel+rans | 623996 | 203.5 | - | 8.06 |
| sel+zstd3 | 574325 | 197.4 | - | 5.53 |

### Peak memory, fast mode, real inputs with INT blocks: process RSS high-water above the RSS just before the operation (KiB; /proc clear_refs reset after malloc_trim; input, frame and output buffers pre-touched)

| operation | config | max over inputs | median over inputs |
|---|---|---|---|
| prod_compress | - | 3328 | 2230.0 |
| prod_decompress_api | - | 1652 | 432.0 |
| prod_decode_seq | - | 1656 | 438.0 |
| exp_residual_encode | exist+store | 1180 | 464.0 |
| exp_residual_decode | exist+store | 1188 | 464.0 |
| exp_decode_seq | exist+store | 1796 | 486.0 |
| exp_residual_encode | exist+huf | 888 | 328.0 |
| exp_residual_decode | exist+huf | 1188 | 464.0 |
| exp_decode_seq | exist+huf | 1796 | 470.0 |
| exp_residual_encode | exist+rans | 1228 | 450.0 |
| exp_residual_decode | exist+rans | 1248 | 524.0 |
| exp_decode_seq | exist+rans | 1856 | 530.0 |
| exp_residual_encode | exist+zstd3 | 1800 | 1220.0 |
| exp_residual_decode | exist+zstd3 | 1220 | 564.0 |
| exp_decode_seq | exist+zstd3 | 1812 | 570.0 |
| exp_residual_encode | gbp+store | 2284 | 912.0 |
| exp_residual_decode | gbp+store | 2320 | 992.0 |
| exp_decode_seq | gbp+store | 2340 | 994.0 |
| exp_residual_encode | gbp+huf | 2072 | 848.0 |
| exp_residual_decode | gbp+huf | 2320 | 992.0 |
| exp_decode_seq | gbp+huf | 2340 | 994.0 |
| exp_residual_encode | gbp+rans | 2412 | 952.0 |
| exp_residual_decode | gbp+rans | 2380 | 1052.0 |
| exp_decode_seq | gbp+rans | 2400 | 1054.0 |
| exp_residual_encode | gbp+zstd3 | 3092 | 1618.0 |
| exp_residual_decode | gbp+zstd3 | 2400 | 1122.0 |
| exp_decode_seq | gbp+zstd3 | 2404 | 1124.0 |
| exp_residual_encode | lbp128+store | 1892 | 944.0 |
| exp_residual_decode | lbp128+store | 2104 | 960.0 |
| exp_decode_seq | lbp128+store | 2668 | 988.0 |
| exp_residual_encode | lbp128+huf | 1852 | 912.0 |
| exp_residual_decode | lbp128+huf | 2104 | 960.0 |
| exp_decode_seq | lbp128+huf | 2668 | 988.0 |
| exp_residual_encode | lbp128+rans | 2144 | 1016.0 |
| exp_residual_decode | lbp128+rans | 2168 | 1054.0 |
| exp_decode_seq | lbp128+rans | 2668 | 1050.0 |
| exp_residual_encode | lbp128+zstd3 | 2824 | 1590.0 |
| exp_residual_decode | lbp128+zstd3 | 2188 | 1114.0 |
| exp_decode_seq | lbp128+zstd3 | 2704 | 1150.0 |
| exp_residual_encode | gpp+store | 1848 | 860.0 |
| exp_residual_decode | gpp+store | 2104 | 964.0 |
| exp_decode_seq | gpp+store | 2120 | 996.0 |
| exp_residual_encode | gpp+huf | 1796 | 826.0 |
| exp_residual_decode | gpp+huf | 2104 | 964.0 |
| exp_decode_seq | gpp+huf | 2120 | 1010.0 |
| exp_residual_encode | gpp+rans | 2056 | 926.0 |
| exp_residual_decode | gpp+rans | 2168 | 1080.0 |
| exp_decode_seq | gpp+rans | 2184 | 1112.0 |
| exp_residual_encode | gpp+zstd3 | 2752 | 1440.0 |
| exp_residual_decode | gpp+zstd3 | 2192 | 1114.0 |
| exp_decode_seq | gpp+zstd3 | 2700 | 1154.0 |
| exp_residual_encode | lpp128+store | 1912 | 928.0 |
| exp_residual_decode | lpp128+store | 2064 | 946.0 |
| exp_decode_seq | lpp128+store | 2084 | 996.0 |
| exp_residual_encode | lpp128+huf | 1896 | 904.0 |
| exp_residual_decode | lpp128+huf | 2064 | 946.0 |
| exp_decode_seq | lpp128+huf | 2084 | 996.0 |
| exp_residual_encode | lpp128+rans | 2148 | 978.0 |
| exp_residual_decode | lpp128+rans | 2128 | 1054.0 |
| exp_decode_seq | lpp128+rans | 2148 | 1060.0 |
| exp_residual_encode | lpp128+zstd3 | 2596 | 1468.0 |
| exp_residual_decode | lpp128+zstd3 | 2156 | 1100.0 |
| exp_decode_seq | lpp128+zstd3 | 2676 | 1162.0 |
| exp_residual_encode | sel+store | 3288 | 1374.0 |
| exp_residual_decode | sel+store | 2064 | 948.0 |
| exp_decode_seq | sel+store | 2084 | 950.0 |
| exp_residual_encode | sel+huf | 2988 | 1100.0 |
| exp_residual_decode | sel+huf | 2064 | 748.0 |
| exp_decode_seq | sel+huf | 2084 | 748.0 |
| exp_residual_encode | sel+rans | 4180 | 1374.0 |
| exp_residual_decode | sel+rans | 2140 | 1020.0 |
| exp_decode_seq | sel+rans | 2192 | 1028.0 |
| exp_residual_encode | sel+zstd3 | 3780 | 2102.0 |
| exp_residual_decode | sel+zstd3 | 1664 | 720.0 |
| exp_decode_seq | sel+zstd3 | 1908 | 722.0 |
