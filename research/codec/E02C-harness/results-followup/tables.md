
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
| planes+store | 10747312 | +251.83% | +0.00% | 0/17/0 |
| planes+huf | 3581655 | +17.25% | -2.24% | 5/12/0 |
| planes+rans | 5027190 | +64.58% | -34.67% | 5/12/0 |
| planes+zstd3 | 3037073 | -0.58% | -0.60% | 13/4/0 |
| exist+ent | 3534488 | +15.71% | +0.00% | 1/16/0 |
| planes+ent | 3194676 | +4.58% | -9.61% | 8/9/0 |
| lpp128+ent | 3572463 | +16.95% | +1.07% | 3/14/0 |

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
| planes+store | 4300423 | +399.48% | +0.00% | 0/3/0 |
| planes+huf | 1165655 | +35.39% | -1.99% | 0/3/0 |
| planes+rans | 1354718 | +57.35% | +24.93% | 2/1/0 |
| planes+zstd3 | 860927 | -0.01% | -0.03% | 2/1/0 |
| exist+ent | 1084351 | +25.94% | +0.00% | 1/2/0 |
| planes+ent | 854736 | -0.73% | -21.18% | 2/1/0 |
| lpp128+ent | 842227 | -2.18% | -22.33% | 2/1/0 |

### Regime A per family: change vs production typed payload (fixed configurations)

| family | n | production B | exist+huf | exist+rans | lbp128+rans | gpp+rans | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+rans | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|---|---|
| audio_s16 | 9 | 2514445 | +13.59% | +177.83% | +22.53% | +24.53% | +18.60% | +19.20% | +16.43% | +17.95% | planes+zstd3 |
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
| planes+store | 5398364 | +71.33% | +0.00% | 0/12/5 |
| planes+huf | 3322142 | +5.44% | -0.93% | 3/9/5 |
| planes+rans | 3600967 | +14.29% | +4.88% | 2/10/5 |
| planes+zstd3 | 3139136 | -0.37% | -0.39% | 9/3/5 |
| exist+ent | 3305626 | +4.91% | +0.00% | 1/11/5 |
| planes+ent | 3188259 | +1.19% | -3.55% | 7/5/5 |
| lpp128+ent | 3273051 | +3.88% | -0.99% | 2/10/5 |

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
| planes+store | 2687507 | +212.94% | +0.00% | 0/2/1 |
| planes+huf | 982380 | +14.39% | -1.55% | 0/2/1 |
| planes+rans | 1362005 | +58.59% | +31.87% | 1/1/1 |
| planes+zstd3 | 887219 | +3.31% | +2.02% | 0/2/1 |
| exist+ent | 994971 | +15.86% | +0.00% | 1/1/1 |
| planes+ent | 862021 | +0.38% | -13.36% | 1/1/1 |
| lpp128+ent | 851923 | -0.80% | -14.38% | 1/1/1 |

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
| planes+store | 3930403 | +53.37% | +0.00% | 0/8/9 |
| planes+huf | 2732883 | +6.64% | -0.36% | 2/6/9 |
| planes+rans | 2761374 | +7.75% | -0.18% | 1/7/9 |
| planes+zstd3 | 2584998 | +0.87% | -0.18% | 0/8/9 |
| exist+ent | 2695274 | +5.17% | +0.00% | 0/8/9 |
| planes+ent | 2638665 | +2.96% | -2.10% | 2/6/9 |
| lpp128+ent | 2645650 | +3.24% | -1.84% | 1/7/9 |

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
| planes+store | 4300576 | +400.64% | +0.00% | 0/3/0 |
| planes+huf | 1165808 | +35.71% | -1.99% | 0/3/0 |
| planes+rans | 1354871 | +57.72% | +24.93% | 2/1/0 |
| planes+zstd3 | 861080 | +0.24% | -0.03% | 1/2/0 |
| exist+ent | 1084504 | +26.25% | +0.00% | 1/2/0 |
| planes+ent | 854889 | -0.48% | -21.17% | 2/1/0 |
| lpp128+ent | 842380 | -1.94% | -22.33% | 2/1/0 |

### Regime B per input: production planner selection

| input | mode | origin | bytes | frame | zstd -3 | INT blocks | prod residual codec | INT shape | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|
| audio_concat_s16.raw | fast | derived | 2990280 | 1871368 | 1868162 | 0/3 | - | - | - |
| audio_concat_s16.raw | hc | derived | 2990280 | 1425240 | 1868162 | 0/1 | - | - | - |
| audio_curve.wav | fast | real | 134660 | 83539 | 103632 | 1/1 | zstd | w2o1ow2 | planes+zstd3 |
| audio_curve.wav | hc | real | 134660 | 75954 | 103632 | 1/1 | zstd | w2o1ow2 | planes+zstd3 |
| audio_drama.wav | fast | real | 162366 | 93402 | 98330 | 1/1 | zstd | w2o1ow4 | planes+zstd3 |
| audio_drama.wav | hc | real | 162366 | 76273 | 98330 | 0/1 | - | - | - |
| audio_gong.wav | fast | real | 191292 | 120058 | 120006 | 0/1 | - | - | - |
| audio_gong.wav | hc | real | 191292 | 102839 | 120006 | 0/1 | - | - | - |
| audio_roll.wav | fast | real | 242648 | 173740 | 229159 | 1/1 | zstd | w2o2ow4 | lpp128+rans |
| audio_roll.wav | hc | real | 242648 | 169810 | 229159 | 1/1 | zstd | w2o2ow4 | lpp128+rans |
| audio_soft.wav | fast | real | 185752 | 95227 | 105852 | 1/1 | zstd | w2o1ow4 | planes+zstd3 |
| audio_soft.wav | hc | real | 185752 | 75842 | 105852 | 0/1 | - | - | - |
| audio_space2.wav | fast | real | 192686 | 75175 | 101803 | 1/1 | zstd | w2o2ow4 | planes+zstd3 |
| audio_space2.wav | hc | real | 192686 | 55691 | 101803 | 0/1 | - | - | - |
| audio_space3.wav | fast | real | 144748 | 56412 | 64267 | 1/1 | huf | w2o1ow2 | planes+rans |
| audio_space3.wav | hc | real | 144748 | 42289 | 64267 | 0/1 | - | - | - |
| audio_untie.wav | fast | real | 190884 | 115979 | 115927 | 0/1 | - | - | - |
| audio_untie.wav | hc | real | 190884 | 92404 | 115927 | 0/1 | - | - | - |
| dem_jacksboro_i16.npy | fast | real | 277392 | 92978 | 165635 | 0/1 | - | - | - |
| dem_jacksboro_i16.npy | hc | real | 277392 | 93171 | 165635 | 0/1 | - | - | - |
| fs_inode_u64.npy | fast | derived | 1205616 | 38636 | 199898 | 2/2 | zstd | w8o2ow4 | gpp+zstd3 |
| fs_inode_u64.npy | hc | derived | 1205616 | 33213 | 199898 | 1/1 | zstd | w8o2ow4 | gpp+zstd3 |
| fs_mtime_i64.npy | fast | derived | 1205616 | 51727 | 53269 | 0/2 | - | - | - |
| fs_mtime_i64.npy | hc | derived | 1205616 | 47267 | 53269 | 0/1 | - | - | - |
| fs_size_u32.npy | fast | derived | 602872 | 245334 | 281940 | 1/1 | zstd | w4o1ow4 | planes+zstd3 |
| fs_size_u32.npy | hc | derived | 602872 | 236512 | 281940 | 1/1 | zstd | w4o1ow4 | planes+zstd3 |
| goog_date_i64.npy | fast | real | 8504 | 326 | 1199 | 1/1 | zstd | w8o2ow1 | gbp+zstd3 |
| goog_date_i64.npy | hc | real | 8504 | 262 | 1199 | 1/1 | zstd | w8o2ow1 | gbp+zstd3 |
| goog_volume_i64.npy | fast | real | 8504 | 2624 | 3582 | 1/1 | zstd | w8o1ow4 | planes+huf |
| goog_volume_i64.npy | hc | real | 8504 | 2534 | 3582 | 1/1 | zstd | w8o1ow4 | planes+huf |
| mri_s1045_u16.npy | fast | real | 131200 | 21797 | 32903 | 1/1 | zstd | w2o2ow1 | exist+zstd3 |
| mri_s1045_u16.npy | hc | real | 131200 | 21363 | 32903 | 1/1 | zstd | w2o2ow1 | exist+zstd3 |
| syn_clustered_outliers_u32.npy | fast | synthetic | 2000128 | 303878 | 445558 | 2/2 | zstd | w4o0ow4 | lpp128+zstd3 |
| syn_clustered_outliers_u32.npy | hc | synthetic | 2000128 | 303194 | 445558 | 1/1 | zstd | w4o0ow4 | lpp128+zstd3 |
| syn_sparse_outliers_u32.npy | fast | synthetic | 2000128 | 386987 | 539580 | 0/2 | - | - | - |
| syn_sparse_outliers_u32.npy | hc | synthetic | 2000128 | 387951 | 539580 | 1/1 | zstd | w4o0ow4 | gpp+huf |
| syn_walk_i32.npy | fast | synthetic | 1200128 | 167934 | 387240 | 2/2 | huf | w4o1ow1 | exist+rans |
| syn_walk_i32.npy | hc | synthetic | 1200128 | 167878 | 387240 | 1/1 | huf | w4o1ow1 | exist+rans |
| topobathy_i16.npy | fast | real | 21968 | 12530 | 14717 | 1/1 | zstd | w2o1ow2 | planes+huf |
| topobathy_i16.npy | hc | real | 21968 | 12023 | 14717 | 1/1 | zstd | w2o1ow2 | planes+huf |

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
| exist+store | 1.72 | 1.70–1.74 | 6239 | 1.72 | 1.33 | 1.35–1.40 | 8076 | 1.33 |
| exist+huf | 12.20 | 12.23–13.32 | 881 | 12.19 | 10.19 | 10.35–11.12 | 1055 | 10.17 |
| exist+rans | 44.37 | 43.41–50.12 | 242 | 43.50 | 13.58 | 13.39–14.22 | 791 | 13.46 |
| exist+zstd3 | 34.84 | 34.95–37.44 | 308 | 34.79 | 9.41 | 9.39–11.38 | 1142 | 9.40 |
| gbp+store | 15.28 | 15.27–18.15 | 703 | 15.19 | 12.19 | 12.22–12.69 | 881 | 12.07 |
| gbp+huf | 22.30 | 22.59–23.22 | 482 | 22.23 | 20.31 | 20.13–20.96 | 529 | 20.21 |
| gbp+rans | 42.49 | 42.78–43.72 | 253 | 42.45 | 34.35 | 34.37–35.12 | 313 | 34.33 |
| gbp+zstd3 | 53.93 | 55.06–67.85 | 199 | 53.85 | 20.84 | 21.47–23.46 | 516 | 20.79 |
| lbp128+store | 14.78 | 14.92–15.44 | 727 | 14.76 | 11.34 | 11.37–12.15 | 947 | 11.32 |
| lbp128+huf | 19.68 | 19.91–20.78 | 546 | 19.67 | 17.53 | 17.90–18.70 | 613 | 17.36 |
| lbp128+rans | 35.09 | 34.70–41.38 | 306 | 34.63 | 29.30 | 28.47–32.16 | 367 | 28.45 |
| lbp128+zstd3 | 34.38 | 34.86–37.68 | 313 | 34.32 | 17.11 | 17.19–20.01 | 628 | 17.07 |
| gpp+store | 23.75 | 23.80–25.96 | 452 | 23.74 | 12.12 | 12.22–12.88 | 886 | 12.12 |
| gpp+huf | 29.04 | 28.95–30.64 | 370 | 29.00 | 18.42 | 18.33–18.91 | 583 | 18.36 |
| gpp+rans | 44.49 | 44.81–48.44 | 242 | 44.41 | 28.76 | 28.84–29.60 | 374 | 28.56 |
| gpp+zstd3 | 49.21 | 49.87–50.77 | 218 | 48.90 | 18.66 | 18.45–19.55 | 576 | 18.55 |
| lpp128+store | 27.18 | 27.40–28.06 | 395 | 27.15 | 11.72 | 11.77–12.27 | 916 | 11.64 |
| lpp128+huf | 33.80 | 32.35–39.72 | 318 | 32.85 | 17.60 | 17.35–20.07 | 610 | 17.45 |
| lpp128+rans | 45.03 | 45.58–47.09 | 239 | 45.00 | 26.14 | 26.06–28.41 | 411 | 26.08 |
| lpp128+zstd3 | 46.73 | 42.76–51.50 | 230 | 45.93 | 17.35 | 16.75–19.56 | 619 | 17.23 |
| sel+store | 83.93 | 85.45–96.54 | 128 | 83.80 | 11.64 | 11.77–12.10 | 923 | 11.57 |
| sel+huf | 117.60 | 118.01–121.49 | 91 | 116.59 | 11.92 | 11.98–14.21 | 901 | 11.86 |
| sel+rans | 212.94 | 212.95–250.67 | 50 | 211.74 | 25.98 | 26.13–26.54 | 414 | 25.90 |
| sel+zstd3 | 213.94 | 212.89–257.10 | 50 | 213.41 | 10.27 | 10.32–10.58 | 1046 | 10.26 |
| planes+store | 1.86 | 1.86–2.00 | 5785 | 1.82 | 1.28 | 1.29–1.49 | 8362 | 1.28 |
| planes+huf | 12.28 | 12.30–13.72 | 875 | 12.28 | 9.38 | 9.42–10.50 | 1146 | 9.38 |
| planes+rans | 41.79 | 40.61–43.64 | 257 | 40.52 | 23.69 | 23.94–24.28 | 454 | 23.68 |
| planes+zstd3 | 32.96 | 32.51–40.58 | 326 | 32.85 | 9.14 | 9.08–10.38 | 1175 | 9.07 |
| exist+ent | 52.07 | 52.13–62.11 | 206 | 51.83 | 12.76 | 12.78–14.74 | 842 | 12.75 |
| planes+ent | 50.74 | 51.88–54.62 | 212 | 50.58 | 17.67 | 18.08–19.31 | 608 | 17.59 |
| lpp128+ent | 51.27 | 50.25–60.89 | 210 | 50.49 | 20.35 | 20.29–22.24 | 528 | 20.29 |

### Native kernel timing, regime B planner fast, real inputs (n=12, 3144353 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.27 | 0.27–0.28 | 11831 | 0.27 | 0.21 | 0.21–0.24 | 14764 | 0.21 |
| exist+huf | 3.70 | 3.66–4.00 | 850 | 3.65 | 3.01 | 2.98–3.34 | 1046 | 3.00 |
| exist+rans | 12.87 | 13.20–13.69 | 244 | 12.85 | 7.91 | 7.87–8.38 | 398 | 7.88 |
| exist+zstd3 | 10.03 | 10.11–11.07 | 314 | 9.88 | 2.50 | 2.48–2.70 | 1258 | 2.47 |
| gbp+store | 4.76 | 4.87–5.23 | 661 | 4.73 | 3.81 | 3.85–3.94 | 826 | 3.74 |
| gbp+huf | 6.92 | 7.07–8.22 | 454 | 6.93 | 6.23 | 6.30–6.51 | 504 | 6.19 |
| gbp+rans | 14.39 | 15.51–16.38 | 219 | 14.24 | 10.74 | 10.84–11.83 | 293 | 10.68 |
| gbp+zstd3 | 17.91 | 18.38–22.27 | 176 | 17.80 | 6.61 | 6.73–7.62 | 476 | 6.61 |
| lbp128+store | 4.80 | 4.84–5.17 | 655 | 4.78 | 3.57 | 3.60–3.63 | 881 | 3.57 |
| lbp128+huf | 6.22 | 6.41–6.87 | 505 | 6.19 | 5.33 | 5.34–5.50 | 590 | 5.32 |
| lbp128+rans | 11.68 | 12.18–12.42 | 269 | 11.67 | 8.38 | 8.86–9.51 | 375 | 8.32 |
| lbp128+zstd3 | 11.98 | 12.05–13.07 | 262 | 11.91 | 5.45 | 5.48–5.96 | 576 | 5.45 |
| gpp+store | 8.10 | 8.41–8.55 | 388 | 8.09 | 3.70 | 3.76–3.86 | 851 | 3.69 |
| gpp+huf | 9.39 | 9.23–10.29 | 335 | 9.36 | 5.61 | 5.60–5.92 | 560 | 5.61 |
| gpp+rans | 16.36 | 16.09–17.68 | 192 | 16.29 | 8.68 | 8.81–9.24 | 362 | 8.68 |
| gpp+zstd3 | 14.69 | 14.58–16.53 | 214 | 14.29 | 5.44 | 5.49–5.83 | 578 | 5.40 |
| lpp128+store | 8.74 | 9.16–9.67 | 360 | 8.68 | 3.46 | 3.61–3.83 | 909 | 3.46 |
| lpp128+huf | 10.63 | 10.34–12.50 | 296 | 10.63 | 5.20 | 5.18–5.95 | 604 | 5.19 |
| lpp128+rans | 16.57 | 17.02–18.21 | 190 | 16.04 | 7.94 | 7.96–8.80 | 396 | 7.92 |
| lpp128+zstd3 | 15.42 | 15.12–17.05 | 204 | 15.15 | 5.30 | 5.26–5.63 | 593 | 5.29 |
| sel+store | 26.54 | 27.53–30.94 | 118 | 26.51 | 3.53 | 3.54–4.05 | 892 | 3.52 |
| sel+huf | 36.82 | 37.38–39.26 | 85 | 36.61 | 4.30 | 4.32–4.54 | 731 | 4.30 |
| sel+rans | 69.66 | 72.99–77.48 | 45 | 69.28 | 7.78 | 7.82–8.15 | 404 | 7.71 |
| sel+zstd3 | 67.72 | 68.04–73.44 | 46 | 67.64 | 3.21 | 3.29–3.49 | 979 | 3.21 |
| planes+store | 0.34 | 0.33–0.37 | 9117 | 0.34 | 0.21 | 0.20–0.22 | 15309 | 0.20 |
| planes+huf | 3.63 | 3.73–4.11 | 867 | 3.59 | 2.74 | 2.79–2.96 | 1148 | 2.74 |
| planes+rans | 12.64 | 12.98–13.45 | 249 | 12.54 | 6.74 | 6.76–7.00 | 467 | 6.73 |
| planes+zstd3 | 8.69 | 8.52–10.44 | 362 | 8.68 | 2.47 | 2.57–3.21 | 1275 | 2.46 |
| exist+ent | 16.70 | 16.75–18.58 | 188 | 16.60 | 4.12 | 4.17–4.34 | 764 | 4.11 |
| planes+ent | 16.49 | 16.40–18.07 | 191 | 16.45 | 6.80 | 6.78–7.08 | 462 | 6.79 |
| lpp128+ent | 18.35 | 18.23–20.40 | 171 | 17.14 | 7.27 | 7.41–7.78 | 433 | 7.23 |

### Native kernel timing, regime B planner hc, real inputs (n=8, 1918023 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.17 | 0.17–0.17 | 11219 | 0.17 | 0.14 | 0.14–0.15 | 13367 | 0.14 |
| exist+huf | 2.37 | 2.21–2.62 | 811 | 2.37 | 2.01 | 1.95–2.15 | 956 | 2.01 |
| exist+rans | 8.88 | 8.86–9.31 | 216 | 8.86 | 4.97 | 4.91–5.09 | 386 | 4.94 |
| exist+zstd3 | 5.00 | 4.98–5.15 | 384 | 4.99 | 1.36 | 1.35–1.50 | 1406 | 1.36 |
| gbp+store | 3.13 | 3.09–3.33 | 613 | 3.13 | 2.42 | 2.37–2.49 | 791 | 2.41 |
| gbp+huf | 5.02 | 4.87–5.14 | 382 | 5.02 | 4.24 | 4.14–4.40 | 452 | 4.24 |
| gbp+rans | 10.39 | 10.34–10.60 | 185 | 10.37 | 7.24 | 7.10–7.36 | 265 | 7.24 |
| gbp+zstd3 | 10.76 | 10.70–11.04 | 178 | 10.72 | 4.23 | 4.16–4.48 | 453 | 4.23 |
| lbp128+store | 3.01 | 3.02–3.08 | 637 | 3.00 | 2.22 | 2.20–2.27 | 865 | 2.21 |
| lbp128+huf | 4.01 | 3.99–4.17 | 479 | 4.00 | 3.37 | 3.34–3.44 | 570 | 3.36 |
| lbp128+rans | 7.08 | 7.10–7.15 | 271 | 7.09 | 5.13 | 5.11–5.19 | 374 | 5.13 |
| lbp128+zstd3 | 6.11 | 5.88–6.24 | 314 | 6.10 | 3.35 | 3.28–3.40 | 573 | 3.35 |
| gpp+store | 4.89 | 4.88–4.97 | 393 | 4.89 | 2.22 | 2.20–2.26 | 863 | 2.22 |
| gpp+huf | 5.84 | 5.84–6.04 | 328 | 5.84 | 3.38 | 3.42–3.45 | 567 | 3.38 |
| gpp+rans | 9.84 | 9.76–10.15 | 195 | 9.83 | 5.01 | 5.03–5.13 | 383 | 5.01 |
| gpp+zstd3 | 8.02 | 8.03–8.41 | 239 | 8.01 | 3.30 | 3.28–3.37 | 580 | 3.30 |
| lpp128+store | 5.87 | 5.69–5.92 | 327 | 5.88 | 2.19 | 2.16–2.23 | 877 | 2.19 |
| lpp128+huf | 6.72 | 6.75–6.81 | 285 | 6.70 | 3.20 | 3.13–3.26 | 599 | 3.20 |
| lpp128+rans | 10.14 | 9.52–10.24 | 189 | 10.10 | 4.49 | 4.31–4.55 | 428 | 4.49 |
| lpp128+zstd3 | 8.41 | 7.74–8.62 | 228 | 8.42 | 2.93 | 2.97–3.11 | 655 | 2.93 |
| sel+store | 17.42 | 17.57–17.90 | 110 | 17.40 | 2.18 | 2.14–2.26 | 879 | 2.18 |
| sel+huf | 24.20 | 23.67–24.99 | 79 | 24.14 | 3.25 | 3.23–3.26 | 591 | 3.25 |
| sel+rans | 45.73 | 46.26–47.09 | 42 | 45.68 | 4.63 | 4.63–4.80 | 414 | 4.62 |
| sel+zstd3 | 38.42 | 38.44–41.64 | 50 | 38.40 | 2.05 | 2.02–2.12 | 934 | 2.05 |
| planes+store | 0.22 | 0.21–0.22 | 8901 | 0.22 | 0.13 | 0.13–0.14 | 14794 | 0.13 |
| planes+huf | 2.59 | 2.55–2.70 | 742 | 2.58 | 1.96 | 1.96–2.06 | 979 | 1.96 |
| planes+rans | 9.08 | 9.09–9.28 | 211 | 9.05 | 4.39 | 4.22–4.43 | 437 | 4.34 |
| planes+zstd3 | 4.39 | 4.29–4.62 | 437 | 4.39 | 1.44 | 1.45–1.49 | 1328 | 1.44 |
| exist+ent | 11.19 | 11.32–11.49 | 171 | 11.19 | 2.97 | 2.89–3.14 | 646 | 2.97 |
| planes+ent | 11.57 | 11.53–11.67 | 166 | 11.57 | 4.28 | 4.24–4.35 | 448 | 4.28 |
| lpp128+ent | 10.91 | 10.84–11.00 | 176 | 10.88 | 3.94 | 3.86–4.00 | 486 | 3.94 |

### End-to-end, complete planner fast, real inputs with INT blocks (n=12, 3041524 B). Encode for experimental rows = production compress - production residual-leaf re-encode (8.9 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 898742 | 112.9 | 7.75 | 7.79 |
| zstd -3 whole input (direct control) | 1237282 | 15.8 | 4.37 | - |
| exist+store | 3146075 | 104.4 | - | 5.99 |
| exist+huf | 1101255 | 107.6 | - | 8.71 |
| exist+rans | 1181248 | 116.9 | - | 13.41 |
| exist+zstd3 | 899194 | 114.2 | - | 8.02 |
| gbp+store | 2238672 | 108.3 | - | 9.34 |
| gbp+huf | 1225392 | 110.9 | - | 11.80 |
| gbp+rans | 1187306 | 118.1 | - | 16.34 |
| gbp+zstd3 | 1133112 | 121.4 | - | 12.29 |
| lbp128+store | 1377744 | 108.4 | - | 9.01 |
| lbp128+huf | 1067035 | 110.0 | - | 10.78 |
| lbp128+rans | 1061908 | 114.9 | - | 13.87 |
| lbp128+zstd3 | 1033929 | 115.2 | - | 10.95 |
| gpp+store | 1317736 | 111.5 | - | 9.15 |
| gpp+huf | 1030173 | 112.9 | - | 10.99 |
| gpp+rans | 1031069 | 118.7 | - | 13.74 |
| gpp+zstd3 | 972452 | 118.6 | - | 11.19 |
| lpp128+store | 1192522 | 112.8 | - | 9.06 |
| lpp128+huf | 1023295 | 114.1 | - | 10.76 |
| lpp128+rans | 1025881 | 119.4 | - | 13.11 |
| lpp128+zstd3 | 990584 | 118.1 | - | 10.81 |
| sel+store | 1192501 | 130.4 | - | 9.11 |
| sel+huf | 991805 | 140.1 | - | 9.96 |
| sel+rans | 998495 | 173.1 | - | 13.01 |
| sel+zstd3 | 892332 | 171.0 | - | 8.75 |
| planes+store | 3146254 | 104.4 | - | 5.85 |
| planes+huf | 1070032 | 107.7 | - | 8.36 |
| planes+rans | 1348857 | 116.6 | - | 12.21 |
| planes+zstd3 | 887026 | 112.7 | - | 8.07 |
| exist+ent | 1053516 | 120.5 | - | 9.58 |
| planes+ent | 936149 | 120.4 | - | 12.08 |
| lpp128+ent | 1020941 | 120.8 | - | 12.45 |

### End-to-end, complete planner hc, real inputs with INT blocks (n=8, 2355972 B). Encode for experimental rows = production compress - production residual-leaf re-encode (283.2 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 551671 | 440.8 | 5.06 | 5.24 |
| zstd -3 whole input (direct control) | 867030 | 10.2 | 3.22 | - |
| exist+store | 1919299 | 157.9 | - | 3.80 |
| exist+huf | 731658 | 160.0 | - | 5.59 |
| exist+rans | 755372 | 165.8 | - | 8.34 |
| exist+zstd3 | 578664 | 162.9 | - | 4.92 |
| gbp+store | 1533593 | 160.4 | - | 5.94 |
| gbp+huf | 798499 | 162.1 | - | 7.59 |
| gbp+rans | 763499 | 167.1 | - | 10.52 |
| gbp+zstd3 | 739651 | 167.8 | - | 7.62 |
| lbp128+store | 840973 | 160.3 | - | 5.68 |
| lbp128+huf | 664107 | 161.3 | - | 6.87 |
| lbp128+rans | 661034 | 164.3 | - | 8.66 |
| lbp128+zstd3 | 645537 | 163.4 | - | 6.84 |
| gpp+store | 781202 | 162.2 | - | 5.85 |
| gpp+huf | 637179 | 163.3 | - | 7.25 |
| gpp+rans | 637930 | 166.8 | - | 8.47 |
| gpp+zstd3 | 597304 | 165.4 | - | 6.83 |
| lpp128+store | 695252 | 163.0 | - | 5.68 |
| lpp128+huf | 635607 | 163.8 | - | 6.74 |
| lpp128+rans | 637754 | 167.0 | - | 8.00 |
| lpp128+zstd3 | 613007 | 165.7 | - | 6.56 |
| sel+store | 695231 | 173.6 | - | 5.64 |
| sel+huf | 623018 | 180.0 | - | 6.76 |
| sel+rans | 623996 | 200.3 | - | 7.97 |
| sel+zstd3 | 574325 | 193.9 | - | 5.56 |
| planes+store | 1919387 | 158.0 | - | 3.86 |
| planes+huf | 721867 | 160.0 | - | 5.42 |
| planes+rans | 750358 | 165.9 | - | 7.62 |
| planes+zstd3 | 573982 | 162.3 | - | 4.98 |
| exist+ent | 684258 | 168.1 | - | 6.54 |
| planes+ent | 627649 | 168.2 | - | 7.57 |
| lpp128+ent | 634634 | 167.8 | - | 7.44 |

### Peak memory, fast mode, real inputs with INT blocks: process RSS high-water above the RSS just before the operation (KiB; /proc clear_refs reset after malloc_trim; input, frame and output buffers pre-touched)

| operation | config | max over inputs | median over inputs |
|---|---|---|---|
| prod_compress | - | 3328 | 2230.0 |
| prod_decompress_api | - | 1652 | 432.0 |
| prod_decode_seq | - | 1656 | 438.0 |
| exp_residual_encode | exist+store | 1180 | 464.0 |
| exp_residual_decode | exist+store | 1188 | 464.0 |
| exp_decode_seq | exist+store | 1796 | 470.0 |
| exp_residual_encode | exist+huf | 888 | 328.0 |
| exp_residual_decode | exist+huf | 1188 | 464.0 |
| exp_decode_seq | exist+huf | 1796 | 470.0 |
| exp_residual_encode | exist+rans | 1228 | 450.0 |
| exp_residual_decode | exist+rans | 1248 | 524.0 |
| exp_decode_seq | exist+rans | 1856 | 530.0 |
| exp_residual_encode | exist+zstd3 | 1800 | 1220.0 |
| exp_residual_decode | exist+zstd3 | 1220 | 550.0 |
| exp_decode_seq | exist+zstd3 | 1812 | 562.0 |
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
| exp_residual_decode | gbp+zstd3 | 2400 | 1116.0 |
| exp_decode_seq | gbp+zstd3 | 2404 | 1120.0 |
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
| exp_residual_decode | lbp128+zstd3 | 2188 | 1142.0 |
| exp_decode_seq | lbp128+zstd3 | 2704 | 1150.0 |
| exp_residual_encode | gpp+store | 1848 | 860.0 |
| exp_residual_decode | gpp+store | 2104 | 964.0 |
| exp_decode_seq | gpp+store | 2120 | 996.0 |
| exp_residual_encode | gpp+huf | 1796 | 826.0 |
| exp_residual_decode | gpp+huf | 2104 | 964.0 |
| exp_decode_seq | gpp+huf | 2120 | 996.0 |
| exp_residual_encode | gpp+rans | 2056 | 926.0 |
| exp_residual_decode | gpp+rans | 2168 | 1080.0 |
| exp_decode_seq | gpp+rans | 2184 | 1112.0 |
| exp_residual_encode | gpp+zstd3 | 2752 | 1440.0 |
| exp_residual_decode | gpp+zstd3 | 2192 | 1122.0 |
| exp_decode_seq | gpp+zstd3 | 2700 | 1146.0 |
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
| exp_residual_decode | lpp128+zstd3 | 2156 | 1096.0 |
| exp_decode_seq | lpp128+zstd3 | 2676 | 1152.0 |
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
| exp_residual_decode | sel+zstd3 | 1664 | 714.0 |
| exp_decode_seq | sel+zstd3 | 1908 | 722.0 |
| exp_residual_encode | planes+store | 1204 | 476.0 |
| exp_residual_decode | planes+store | 740 | 312.0 |
| exp_decode_seq | planes+store | 1668 | 392.0 |
| exp_residual_encode | planes+huf | 904 | 328.0 |
| exp_residual_decode | planes+huf | 748 | 312.0 |
| exp_decode_seq | planes+huf | 1716 | 392.0 |
| exp_residual_encode | planes+rans | 1136 | 420.0 |
| exp_residual_decode | planes+rans | 848 | 372.0 |
| exp_decode_seq | planes+rans | 1816 | 452.0 |
| exp_residual_encode | planes+zstd3 | 1736 | 956.0 |
| exp_residual_decode | planes+zstd3 | 828 | 486.0 |
| exp_decode_seq | planes+zstd3 | 1732 | 568.0 |
| exp_residual_encode | exist+ent | 1528 | 550.0 |
| exp_residual_decode | exist+ent | 1248 | 494.0 |
| exp_decode_seq | exist+ent | 1856 | 500.0 |
| exp_residual_encode | planes+ent | 1384 | 442.0 |
| exp_residual_decode | planes+ent | 848 | 372.0 |
| exp_decode_seq | planes+ent | 1816 | 452.0 |
| exp_residual_encode | lpp128+ent | 2408 | 1062.0 |
| exp_residual_decode | lpp128+ent | 2128 | 1010.0 |
| exp_decode_seq | lpp128+ent | 2148 | 1060.0 |
