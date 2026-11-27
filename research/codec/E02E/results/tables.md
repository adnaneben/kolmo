
### Regime A (typed, kg_encode_num at zstd level 3), real inputs: total stored bytes (n=17; input 11852688 B; production typed payload 2714056 B; zstd -3 whole input 4206612 B)

| config | bytes | vs production payload | vs exist, same backend | inputs smaller/larger/equal vs production |
|---|---|---|---|---|
| exist+store | 5897402 | +117.29% | +0.00% | 0/17/0 |
| exist+huf | 2758501 | +1.64% | +0.00% | 0/17/0 |
| exist+rans | 2709740 | -0.16% | +0.00% | 12/5/0 |
| exist+zstd3 | 2897559 | +6.76% | +0.00% | 0/17/0 |
| gbp+store | 5481348 | +101.96% | -7.05% | 0/17/0 |
| gbp+huf | 2945008 | +8.51% | +6.76% | 0/17/0 |
| gbp+rans | 2905502 | +7.05% | +7.22% | 8/9/0 |
| gbp+zstd3 | 3066105 | +12.97% | +5.82% | 0/17/0 |
| lbp128+store | 3894301 | +43.49% | -33.97% | 0/17/0 |
| lbp128+huf | 2998242 | +10.47% | +8.69% | 2/15/0 |
| lbp128+rans | 2978386 | +9.74% | +9.91% | 3/14/0 |
| lbp128+zstd3 | 2996148 | +10.39% | +3.40% | 0/17/0 |
| gpp+store | 3484356 | +28.38% | -40.92% | 0/17/0 |
| gpp+huf | 2658467 | -2.05% | -3.63% | 12/5/0 |
| gpp+rans | 2636195 | -2.87% | -2.71% | 12/5/0 |
| gpp+zstd3 | 2651463 | -2.31% | -8.49% | 12/5/0 |
| lpp128+store | 3444749 | +26.92% | -41.59% | 0/17/0 |
| lpp128+huf | 2867719 | +5.66% | +3.96% | 2/15/0 |
| lpp128+rans | 2858854 | +5.34% | +5.50% | 1/16/0 |
| lpp128+zstd3 | 2768552 | +2.01% | -4.45% | 6/11/0 |
| sel+store | 3430895 | +26.41% | -41.82% | 0/17/0 |
| sel+huf | 2614987 | -3.65% | -5.20% | 12/5/0 |
| sel+rans | 2589547 | -4.59% | -4.44% | 14/3/0 |
| sel+zstd3 | 2605384 | -4.00% | -10.08% | 12/5/0 |
| planes+store | 5897458 | +117.29% | +0.00% | 0/17/0 |
| planes+huf | 2739775 | +0.95% | -0.68% | 2/15/0 |
| planes+rans | 2663356 | -1.87% | -1.71% | 17/0/0 |
| planes+zstd3 | 2893948 | +6.63% | -0.12% | 8/9/0 |
| exist+ent | 2709740 | -0.16% | +0.00% | 12/5/0 |
| planes+ent | 2663351 | -1.87% | -1.71% | 17/0/0 |
| lpp128+ent | 2853846 | +5.15% | +5.32% | 2/15/0 |
| exist+zstd19 | 2703778 | -0.38% | +0.00% | 8/9/0 |
| planes+zstd19 | 2698848 | -0.56% | -0.18% | 12/5/0 |
| exist+prod3 | 2714275 | +0.01% | +0.00% | 0/17/0 |
| planes+prod3 | 2704979 | -0.33% | -0.34% | 8/9/0 |
| exist+prod19 | 2691627 | -0.83% | +0.00% | 8/9/0 |
| planes+prod19 | 2683683 | -1.12% | -0.30% | 13/4/0 |
| exist+prod3g | 2714275 | +0.01% | +0.00% | 0/17/0 |
| planes+prod3g | 2704979 | -0.33% | -0.34% | 8/9/0 |

### Regime A per family: change vs production typed payload (fixed configurations)

| family | n | production B | exist+huf | exist+rans | lbp128+rans | gpp+rans | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+rans | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|---|---|
| F1_ghcn_daily | 9 | 219098 | +20.26% | +14.14% | +16.18% | +9.35% | +17.71% | +18.98% | +15.84% | +0.27% | planes+rans |
| F2_mitdb | 8 | 2494958 | +0.00% | -1.41% | +9.17% | -3.94% | +4.60% | +4.14% | +0.79% | -5.01% | gpp+rans |

### Regime B (complete planner, fast), real inputs: complete-input stored bytes (n=17, 9 with INT blocks; input 11852688 B; production frame 2930577 B; zstd -3 4206612 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 3407840 | +16.29% | +0.00% | 0/9/8 |
| exist+huf | 2974958 | +1.51% | +0.00% | 0/9/8 |
| exist+rans | 2961555 | +1.06% | +0.00% | 4/5/8 |
| exist+zstd3 | 2931674 | +0.04% | +0.00% | 0/9/8 |
| gbp+store | 3154276 | +7.63% | -7.44% | 0/9/8 |
| gbp+huf | 2997290 | +2.28% | +0.75% | 0/9/8 |
| gbp+rans | 2993251 | +2.14% | +1.07% | 2/7/8 |
| gbp+zstd3 | 3000521 | +2.39% | +2.35% | 0/9/8 |
| lbp128+store | 3097301 | +5.69% | -9.11% | 0/9/8 |
| lbp128+huf | 2967342 | +1.25% | -0.26% | 2/7/8 |
| lbp128+rans | 2966024 | +1.21% | +0.15% | 3/6/8 |
| lbp128+zstd3 | 2972458 | +1.43% | +1.39% | 0/9/8 |
| gpp+store | 3024745 | +3.21% | -11.24% | 0/9/8 |
| gpp+huf | 2948486 | +0.61% | -0.89% | 5/4/8 |
| gpp+rans | 2951063 | +0.70% | -0.35% | 5/4/8 |
| gpp+zstd3 | 2950910 | +0.69% | +0.66% | 5/4/8 |
| lpp128+store | 3025923 | +3.25% | -11.21% | 0/9/8 |
| lpp128+huf | 2969381 | +1.32% | -0.19% | 2/7/8 |
| lpp128+rans | 2972167 | +1.42% | +0.36% | 1/8/8 |
| lpp128+zstd3 | 2965285 | +1.18% | +1.15% | 2/7/8 |
| sel+store | 3022828 | +3.15% | -11.30% | 0/9/8 |
| sel+huf | 2929690 | -0.03% | -1.52% | 5/4/8 |
| sel+rans | 2931175 | +0.02% | -1.03% | 6/3/8 |
| sel+zstd3 | 2920932 | -0.33% | -0.37% | 5/4/8 |
| planes+store | 3407896 | +16.29% | +0.00% | 0/9/8 |
| planes+huf | 2956232 | +0.88% | -0.63% | 2/7/8 |
| planes+rans | 2915171 | -0.53% | -1.57% | 9/0/8 |
| planes+zstd3 | 2928063 | -0.09% | -0.12% | 8/1/8 |
| exist+ent | 2961555 | +1.06% | +0.00% | 4/5/8 |
| planes+ent | 2915166 | -0.53% | -1.57% | 9/0/8 |
| lpp128+ent | 2968892 | +1.31% | +0.25% | 2/7/8 |
| exist+zstd19 | 2924423 | -0.21% | +0.00% | 3/6/8 |
| planes+zstd19 | 2919493 | -0.38% | -0.17% | 7/2/8 |
| exist+prod3 | 2930692 | +0.00% | +0.00% | 0/9/8 |
| planes+prod3 | 2921396 | -0.31% | -0.32% | 8/1/8 |
| exist+prod19 | 2924385 | -0.21% | +0.00% | 3/6/8 |
| planes+prod19 | 2916441 | -0.48% | -0.27% | 8/1/8 |
| exist+prod3g | 2930692 | +0.00% | +0.00% | 0/9/8 |
| planes+prod3g | 2921396 | -0.31% | -0.32% | 8/1/8 |

### Regime B (complete planner, hc), real inputs: complete-input stored bytes (n=17, 17 with INT blocks; input 11852688 B; production frame 2692268 B; zstd -3 4206612 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 5898261 | +119.08% | +0.00% | 0/17/0 |
| exist+huf | 2759360 | +2.49% | +0.00% | 0/17/0 |
| exist+rans | 2710599 | +0.68% | +0.00% | 8/9/0 |
| exist+zstd3 | 2898418 | +7.66% | +0.00% | 4/13/0 |
| gbp+store | 5482207 | +103.63% | -7.05% | 0/17/0 |
| gbp+huf | 2945867 | +9.42% | +6.76% | 0/17/0 |
| gbp+rans | 2906361 | +7.95% | +7.22% | 7/10/0 |
| gbp+zstd3 | 3066964 | +13.92% | +5.82% | 0/17/0 |
| lbp128+store | 3895160 | +44.68% | -33.96% | 0/17/0 |
| lbp128+huf | 2999101 | +11.40% | +8.69% | 0/17/0 |
| lbp128+rans | 2979245 | +10.66% | +9.91% | 0/17/0 |
| lbp128+zstd3 | 2997007 | +11.32% | +3.40% | 0/17/0 |
| gpp+store | 3485215 | +29.45% | -40.91% | 0/17/0 |
| gpp+huf | 2659326 | -1.22% | -3.63% | 12/5/0 |
| gpp+rans | 2637054 | -2.05% | -2.71% | 12/5/0 |
| gpp+zstd3 | 2652322 | -1.48% | -8.49% | 11/6/0 |
| lpp128+store | 3445608 | +27.98% | -41.58% | 0/17/0 |
| lpp128+huf | 2868578 | +6.55% | +3.96% | 1/16/0 |
| lpp128+rans | 2859713 | +6.22% | +5.50% | 1/16/0 |
| lpp128+zstd3 | 2769411 | +2.87% | -4.45% | 3/14/0 |
| sel+store | 3431754 | +27.47% | -41.82% | 0/17/0 |
| sel+huf | 2615846 | -2.84% | -5.20% | 12/5/0 |
| sel+rans | 2590406 | -3.78% | -4.43% | 14/3/0 |
| sel+zstd3 | 2606243 | -3.20% | -10.08% | 13/4/0 |
| planes+store | 5898317 | +119.08% | +0.00% | 0/17/0 |
| planes+huf | 2740634 | +1.80% | -0.68% | 0/17/0 |
| planes+rans | 2664215 | -1.04% | -1.71% | 16/1/0 |
| planes+zstd3 | 2894807 | +7.52% | -0.12% | 5/12/0 |
| exist+ent | 2710599 | +0.68% | +0.00% | 8/9/0 |
| planes+ent | 2664210 | -1.04% | -1.71% | 16/1/0 |
| lpp128+ent | 2854705 | +6.03% | +5.32% | 1/16/0 |
| exist+zstd19 | 2704637 | +0.46% | +0.00% | 0/17/0 |
| planes+zstd19 | 2699707 | +0.28% | -0.18% | 8/9/0 |
| exist+prod3 | 2715134 | +0.85% | +0.00% | 4/13/0 |
| planes+prod3 | 2705838 | +0.50% | -0.34% | 7/10/0 |
| exist+prod19 | 2692486 | +0.01% | +0.00% | 0/17/0 |
| planes+prod19 | 2684542 | -0.29% | -0.30% | 8/9/0 |
| exist+prod3g | 2715134 | +0.85% | +0.00% | 4/13/0 |
| planes+prod3g | 2705838 | +0.50% | -0.34% | 7/10/0 |

### Regime B per input: production planner selection

| input | mode | origin | bytes | frame | zstd -3 | INT blocks | prod residual codec | INT shape | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|
| ghcn_USW00014739_PRCP.npy | fast | real | 132864 | 18128 | 23669 | 1/1 | huf | w4o0ow2 | planes+rans |
| ghcn_USW00014739_PRCP.npy | hc | real | 132864 | 15464 | 23669 | 1/1 | zstd | w4o0ow2 | planes+rans |
| ghcn_USW00014739_TMAX.npy | fast | real | 132872 | 25712 | 36397 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00014739_TMAX.npy | hc | real | 132872 | 25939 | 36397 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00014739_TMIN.npy | fast | real | 132872 | 22492 | 34331 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00014739_TMIN.npy | hc | real | 132872 | 22929 | 34331 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00023174_PRCP.npy | fast | real | 120332 | 5685 | 7038 | 1/1 | zstd | w4o0ow2 | gpp+huf |
| ghcn_USW00023174_PRCP.npy | hc | real | 120332 | 4883 | 7038 | 1/1 | zstd | w4o0ow2 | gpp+huf |
| ghcn_USW00023174_TMAX.npy | fast | real | 120216 | 18708 | 27831 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00023174_TMAX.npy | hc | real | 120216 | 18708 | 27831 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00023174_TMIN.npy | fast | real | 120308 | 16022 | 25483 | 1/1 | huf | w4o1ow1 | exist+rans |
| ghcn_USW00023174_TMIN.npy | hc | real | 120308 | 16021 | 25483 | 1/1 | huf | w4o1ow1 | exist+rans |
| ghcn_USW00094728_PRCP.npy | fast | real | 230752 | 31194 | 39925 | 1/1 | huf | w4o0ow2 | planes+rans |
| ghcn_USW00094728_PRCP.npy | hc | real | 230752 | 26428 | 39925 | 1/1 | zstd | w4o0ow2 | planes+rans |
| ghcn_USW00094728_TMAX.npy | fast | real | 230724 | 42718 | 61016 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00094728_TMAX.npy | hc | real | 230724 | 43375 | 61016 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00094728_TMIN.npy | fast | real | 230724 | 38907 | 58741 | 1/1 | zstd | w4o1ow2 | planes+rans |
| ghcn_USW00094728_TMIN.npy | hc | real | 230724 | 39504 | 58741 | 1/1 | zstd | w4o1ow2 | planes+rans |
| mitdb_100_sig0.npy | fast | real | 1300128 | 339974 | 473118 | 0/2 | - | - | - |
| mitdb_100_sig0.npy | hc | real | 1300128 | 316668 | 473118 | 1/1 | huf | w2o1ow1 | gpp+zstd3 |
| mitdb_100_sig1.npy | fast | real | 1300128 | 333319 | 470536 | 0/2 | - | - | - |
| mitdb_100_sig1.npy | hc | real | 1300128 | 306747 | 470536 | 1/1 | zstd | w2o1ow1 | gpp+rans |
| mitdb_101_sig0.npy | fast | real | 1300128 | 350161 | 508152 | 0/2 | - | - | - |
| mitdb_101_sig0.npy | hc | real | 1300128 | 325329 | 508152 | 1/1 | zstd | w2o1ow1 | gpp+rans |
| mitdb_101_sig1.npy | fast | real | 1300128 | 304621 | 398577 | 0/2 | - | - | - |
| mitdb_101_sig1.npy | hc | real | 1300128 | 269051 | 398577 | 1/1 | huf | w2o1ow1 | gpp+rans |
| mitdb_200_sig0.npy | fast | real | 1300128 | 392843 | 604058 | 0/2 | - | - | - |
| mitdb_200_sig0.npy | hc | real | 1300128 | 351627 | 604058 | 1/1 | huf | w2o2ow1 | exist+rans |
| mitdb_200_sig1.npy | fast | real | 1300128 | 339129 | 520642 | 0/2 | - | - | - |
| mitdb_200_sig1.npy | hc | real | 1300128 | 312481 | 520642 | 1/1 | zstd | w2o1ow1 | gpp+rans |
| mitdb_201_sig0.npy | fast | real | 1300128 | 321559 | 459372 | 0/2 | - | - | - |
| mitdb_201_sig0.npy | hc | real | 1300128 | 293430 | 459372 | 1/1 | zstd | w2o1ow1 | gpp+rans |
| mitdb_201_sig1.npy | fast | real | 1300128 | 329405 | 457726 | 0/2 | - | - | - |
| mitdb_201_sig1.npy | hc | real | 1300128 | 303684 | 457726 | 1/1 | zstd | w2o1ow1 | gpp+rans |

### Regime B per input with INT blocks: complete-input change vs production frame

| input | mode | origin | production frame | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ghcn_USW00014739_PRCP.npy | fast | real | 18128 | +0.86% | -19.39% | -13.45% | +17.08% | +20.24% | +6.51% | -19.39% | -18.05% | -13.45% |
| ghcn_USW00014739_PRCP.npy | hc | real | 15464 | +18.23% | -5.51% | +1.45% | +37.24% | +40.95% | +24.85% | -5.51% | -3.94% | +1.45% |
| ghcn_USW00014739_TMAX.npy | fast | real | 25712 | +0.03% | -2.62% | -2.49% | -1.45% | -1.47% | -1.57% | -2.62% | -1.47% | -2.49% |
| ghcn_USW00014739_TMAX.npy | hc | real | 25939 | -0.85% | -3.48% | -3.35% | -2.32% | -2.33% | -2.44% | -3.48% | -2.33% | -3.35% |
| ghcn_USW00014739_TMIN.npy | fast | real | 22492 | +0.04% | +30.55% | +30.55% | +26.96% | +28.04% | +27.13% | +15.64% | +15.91% | +0.04% |
| ghcn_USW00014739_TMIN.npy | hc | real | 22929 | -1.88% | +28.06% | +28.06% | +24.54% | +25.59% | +24.70% | +13.43% | +13.70% | -1.88% |
| ghcn_USW00023174_PRCP.npy | fast | real | 5685 | +0.12% | -23.85% | -20.12% | -2.69% | +12.05% | -4.40% | -23.85% | -15.67% | -20.12% |
| ghcn_USW00023174_PRCP.npy | hc | real | 4883 | +16.55% | -11.37% | -7.02% | +13.27% | +30.43% | +11.28% | -11.37% | -1.84% | -7.02% |
| ghcn_USW00023174_TMAX.npy | fast | real | 18708 | +0.04% | +34.25% | +34.13% | +34.23% | +35.83% | +34.70% | +11.96% | +12.30% | +0.04% |
| ghcn_USW00023174_TMAX.npy | hc | real | 18708 | +0.04% | +34.24% | +34.12% | +34.22% | +35.83% | +34.69% | +11.96% | +12.30% | +0.04% |
| ghcn_USW00023174_TMIN.npy | fast | real | 16022 | +2.10% | +33.49% | +32.80% | +38.02% | +39.51% | +38.19% | +0.04% | -0.69% | +2.10% |
| ghcn_USW00023174_TMIN.npy | hc | real | 16021 | +2.10% | +33.49% | +32.80% | +38.02% | +39.52% | +38.19% | +0.04% | -0.69% | +2.10% |
| ghcn_USW00094728_PRCP.npy | fast | real | 31194 | +1.79% | -20.06% | -15.85% | +14.13% | +15.60% | +6.77% | -20.06% | -19.49% | -15.85% |
| ghcn_USW00094728_PRCP.npy | hc | real | 26428 | +20.14% | -5.65% | -0.67% | +34.71% | +36.44% | +26.02% | -5.65% | -4.98% | -0.67% |
| ghcn_USW00094728_TMAX.npy | fast | real | 42718 | +0.02% | -2.04% | -1.96% | +6.45% | +6.33% | +6.56% | -2.04% | -1.23% | -1.96% |
| ghcn_USW00094728_TMAX.npy | hc | real | 43375 | -1.50% | -3.53% | -3.45% | +4.83% | +4.72% | +4.95% | -3.53% | -2.73% | -3.45% |
| ghcn_USW00094728_TMIN.npy | fast | real | 38907 | +0.02% | +30.69% | +30.40% | +27.03% | +27.50% | +27.13% | +15.49% | +15.36% | +0.02% |
| ghcn_USW00094728_TMIN.npy | hc | real | 39504 | -1.49% | +28.71% | +28.42% | +25.10% | +25.57% | +25.20% | +13.74% | +13.61% | -1.49% |
| mitdb_100_sig0.npy | hc | real | 316668 | +6.97% | -5.29% | -6.56% | +6.04% | +5.54% | -1.91% | -5.29% | -6.36% | -6.56% |
| mitdb_100_sig1.npy | hc | real | 306747 | +8.25% | -4.12% | -4.64% | +6.35% | +5.84% | +0.13% | -4.12% | -5.30% | -4.64% |
| mitdb_101_sig0.npy | hc | real | 325329 | +7.33% | -5.34% | -6.14% | +4.20% | +3.77% | -2.50% | -5.34% | -6.37% | -6.14% |
| mitdb_101_sig1.npy | hc | real | 269051 | +12.62% | -0.68% | -0.72% | +3.71% | +2.78% | +3.44% | -0.68% | -1.65% | -0.72% |
| mitdb_200_sig0.npy | hc | real | 351627 | +4.72% | +7.02% | +6.97% | +3.00% | +2.51% | +2.39% | +0.00% | -1.25% | +2.39% |
| mitdb_200_sig1.npy | hc | real | 312481 | +8.13% | -2.83% | -2.54% | +2.23% | +1.87% | +1.48% | -2.83% | -3.32% | -2.54% |
| mitdb_201_sig0.npy | hc | real | 293430 | +9.24% | -3.98% | -4.31% | +8.52% | +8.29% | +3.40% | -3.98% | -5.37% | -4.31% |
| mitdb_201_sig1.npy | hc | real | 303684 | +8.05% | -4.18% | -4.47% | +8.70% | +8.35% | +5.81% | -4.18% | -5.51% | -4.47% |

### Regime A per input: typed payload change vs production typed payload

| input | origin | production payload | INT shape | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| ghcn_USW00014739_PRCP.npy | real | 18076 | o0ow2 | +0.86% | -19.45% | -13.49% | +17.13% | +20.30% | +6.53% | -19.45% | -18.10% | -13.49% |
| ghcn_USW00014739_TMAX.npy | real | 25660 | o1ow2 | +0.03% | -2.63% | -2.50% | -1.45% | -1.47% | -1.57% | -2.63% | -1.47% | -2.50% |
| ghcn_USW00014739_TMIN.npy | real | 22440 | o1ow2 | +0.04% | +30.62% | +30.62% | +27.02% | +28.10% | +27.19% | +15.67% | +15.95% | +0.04% |
| ghcn_USW00023174_PRCP.npy | real | 5633 | o0ow2 | +0.12% | -24.07% | -20.31% | -2.72% | +12.16% | -4.44% | -24.07% | -15.82% | -20.31% |
| ghcn_USW00023174_TMAX.npy | real | 18656 | o1ow2 | +0.04% | +34.34% | +34.22% | +34.32% | +35.93% | +34.79% | +12.00% | +12.34% | +0.04% |
| ghcn_USW00023174_TMIN.npy | real | 15970 | o1ow1 | +2.11% | +33.60% | +32.91% | +38.14% | +39.64% | +38.32% | +0.04% | -0.69% | +2.11% |
| ghcn_USW00094728_PRCP.npy | real | 31142 | o0ow2 | +1.79% | -20.10% | -15.87% | +14.15% | +15.62% | +6.78% | -20.10% | -19.53% | -15.87% |
| ghcn_USW00094728_TMAX.npy | real | 42666 | o1ow2 | +0.02% | -2.04% | -1.97% | +6.45% | +6.34% | +6.57% | -2.04% | -1.24% | -1.97% |
| ghcn_USW00094728_TMIN.npy | real | 38855 | o1ow2 | +0.02% | +30.73% | +30.44% | +27.06% | +27.54% | +27.16% | +15.51% | +15.38% | +0.02% |
| mitdb_100_sig0.npy | real | 316618 | o1ow1 | +6.97% | -5.29% | -6.56% | +6.05% | +5.55% | -1.91% | -5.29% | -6.36% | -6.56% |
| mitdb_100_sig1.npy | real | 308034 | o1ow1 | +7.78% | -4.54% | -5.05% | +5.89% | +5.38% | -0.31% | -4.54% | -5.71% | -5.05% |
| mitdb_101_sig0.npy | real | 326077 | o1ow1 | +7.07% | -5.58% | -6.37% | +3.94% | +3.52% | -2.73% | -5.58% | -6.60% | -6.37% |
| mitdb_101_sig1.npy | real | 269001 | o1ow1 | +12.62% | -0.68% | -0.72% | +3.71% | +2.78% | +3.44% | -0.68% | -1.65% | -0.72% |
| mitdb_200_sig0.npy | real | 351577 | o2ow1 | +4.72% | +7.02% | +6.97% | +3.00% | +2.51% | +2.39% | +0.00% | -1.25% | +2.39% |
| mitdb_200_sig1.npy | real | 317685 | o1ow1 | +6.34% | -4.44% | -4.15% | +0.54% | +0.18% | -0.20% | -4.44% | -4.92% | -4.15% |
| mitdb_201_sig0.npy | real | 298171 | o1ow1 | +7.48% | -5.53% | -5.85% | +6.78% | +6.55% | +1.74% | -5.53% | -6.89% | -5.85% |
| mitdb_201_sig1.npy | real | 307795 | o1ow1 | +6.59% | -5.48% | -5.76% | +7.23% | +6.89% | +4.38% | -5.48% | -6.79% | -5.76% |

### Native kernel timing, regime A typed, real inputs (n=17, 5895191 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.75 | 0.73–0.76 | 7884 | 0.75 | 0.60 | 0.60–0.62 | 9828 | 0.60 |
| exist+huf | 5.58 | 5.59–5.65 | 1057 | 5.58 | 3.33 | 3.31–3.38 | 1772 | 3.33 |
| exist+rans | 19.41 | 19.46–19.63 | 304 | 19.42 | 14.96 | 14.97–15.04 | 394 | 14.96 |
| exist+zstd3 | 46.88 | 46.72–47.41 | 126 | 46.90 | 8.91 | 8.99–9.18 | 662 | 8.90 |
| gbp+store | 23.88 | 23.97–25.19 | 247 | 23.89 | 14.28 | 14.56–15.35 | 413 | 14.28 |
| gbp+huf | 28.02 | 28.38–30.35 | 210 | 28.00 | 17.31 | 17.49–18.55 | 341 | 17.31 |
| gbp+rans | 42.09 | 41.96–42.46 | 140 | 42.10 | 29.89 | 29.67–30.44 | 197 | 29.89 |
| gbp+zstd3 | 65.91 | 65.80–67.38 | 89 | 65.93 | 24.22 | 23.86–25.48 | 243 | 24.21 |
| lbp128+store | 23.27 | 23.19–25.54 | 253 | 23.28 | 14.77 | 14.71–16.57 | 399 | 14.77 |
| lbp128+huf | 27.11 | 26.83–27.82 | 217 | 27.13 | 18.08 | 18.04–18.46 | 326 | 18.07 |
| lbp128+rans | 38.16 | 38.16–42.39 | 155 | 38.17 | 27.27 | 27.15–27.68 | 216 | 27.27 |
| lbp128+zstd3 | 41.29 | 41.32–42.94 | 143 | 41.30 | 19.72 | 19.78–20.39 | 299 | 19.69 |
| gpp+store | 50.08 | 49.85–56.24 | 118 | 50.10 | 14.78 | 14.81–15.75 | 399 | 14.77 |
| gpp+huf | 53.13 | 53.20–53.73 | 111 | 53.11 | 17.71 | 17.86–18.44 | 333 | 17.70 |
| gpp+rans | 66.19 | 65.76–70.38 | 89 | 66.21 | 27.48 | 27.52–28.90 | 215 | 27.47 |
| gpp+zstd3 | 57.02 | 56.80–62.55 | 103 | 57.04 | 18.97 | 19.29–21.50 | 311 | 18.97 |
| lpp128+store | 49.53 | 49.25–50.75 | 119 | 49.54 | 16.59 | 16.44–17.01 | 355 | 16.58 |
| lpp128+huf | 52.25 | 52.15–54.31 | 113 | 52.27 | 19.57 | 19.61–20.56 | 301 | 19.57 |
| lpp128+rans | 62.71 | 62.58–63.76 | 94 | 62.71 | 27.18 | 26.86–28.22 | 217 | 27.17 |
| lpp128+zstd3 | 58.52 | 58.29–59.70 | 101 | 58.54 | 20.44 | 20.04–21.07 | 288 | 20.43 |
| sel+store | 144.36 | 143.66–147.11 | 41 | 144.34 | 16.09 | 16.16–16.69 | 366 | 16.07 |
| sel+huf | 160.93 | 160.60–165.84 | 37 | 160.93 | 17.12 | 17.42–17.90 | 344 | 17.09 |
| sel+rans | 227.77 | 228.08–230.69 | 26 | 227.77 | 26.20 | 25.99–27.84 | 225 | 26.17 |
| sel+zstd3 | 268.72 | 268.63–278.67 | 22 | 268.69 | 20.39 | 20.35–21.33 | 289 | 20.37 |
| planes+store | 0.75 | 0.72–0.75 | 7887 | 0.75 | 0.60 | 0.60–0.61 | 9892 | 0.60 |
| planes+huf | 5.59 | 5.58–5.61 | 1055 | 5.59 | 3.18 | 3.18–3.23 | 1852 | 3.18 |
| planes+rans | 19.37 | 19.30–19.34 | 304 | 19.38 | 15.02 | 15.02–15.10 | 393 | 15.02 |
| planes+zstd3 | 46.86 | 46.51–46.92 | 126 | 46.87 | 9.00 | 8.90–9.06 | 655 | 8.99 |
| exist+ent | 24.68 | 24.60–24.78 | 239 | 24.67 | 15.00 | 15.00–15.08 | 393 | 14.99 |
| planes+ent | 24.70 | 24.61–24.88 | 239 | 24.71 | 14.97 | 14.95–15.03 | 394 | 14.96 |
| lpp128+ent | 66.04 | 66.60–67.06 | 89 | 66.06 | 26.19 | 25.95–26.94 | 225 | 26.18 |
| exist+zstd19 | 2562.14 | 2557.20–2683.43 | 2 | 2561.99 | 6.94 | 6.97–7.00 | 849 | 6.91 |
| planes+zstd19 | 2603.80 | 2618.28–2723.62 | 2 | 2603.57 | 6.75 | 6.83–7.07 | 874 | 6.71 |
| exist+prod3 | 58.56 | 58.10–59.21 | 101 | 58.57 | 3.02 | 2.99–3.12 | 1954 | 3.00 |
| planes+prod3 | 59.42 | 59.37–60.12 | 99 | 59.43 | 3.04 | 2.98–3.18 | 1938 | 3.02 |
| exist+prod19 | 2594.88 | 2563.83–2775.14 | 2 | 2594.57 | 5.39 | 5.34–5.54 | 1093 | 5.35 |
| planes+prod19 | 2644.78 | 2624.02–2776.48 | 2 | 2644.56 | 5.57 | 5.56–5.77 | 1058 | 5.52 |
| exist+prod3g | 100.82 | 100.25–100.45 | 58 | 100.84 | 3.08 | 3.08–3.16 | 1916 | 3.06 |
| planes+prod3g | 101.07 | 100.74–102.36 | 58 | 101.09 | 3.09 | 3.11–3.26 | 1905 | 3.07 |

### Native kernel timing, regime B planner fast, real inputs (n=9, 695200 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.04 | 0.04–0.04 | 15829 | 0.04 | 0.04 | 0.04–0.04 | 19427 | 0.04 |
| exist+huf | 0.69 | 0.69–0.70 | 1001 | 0.70 | 0.54 | 0.53–0.54 | 1299 | 0.53 |
| exist+rans | 2.85 | 2.84–2.86 | 244 | 2.85 | 1.41 | 1.41–1.43 | 492 | 1.41 |
| exist+zstd3 | 1.38 | 1.38–1.41 | 503 | 1.38 | 0.42 | 0.42–0.44 | 1640 | 0.42 |
| gbp+store | 1.10 | 1.10–1.11 | 632 | 1.10 | 0.91 | 0.90–0.91 | 767 | 0.91 |
| gbp+huf | 1.50 | 1.51–1.53 | 463 | 1.50 | 1.19 | 1.19–1.21 | 582 | 1.19 |
| gbp+rans | 3.65 | 3.65–3.66 | 191 | 3.65 | 2.02 | 2.02–2.04 | 344 | 2.02 |
| gbp+zstd3 | 2.53 | 2.54–2.57 | 275 | 2.53 | 1.35 | 1.36–1.37 | 515 | 1.35 |
| lbp128+store | 1.14 | 1.15–1.16 | 608 | 1.14 | 0.89 | 0.89–0.89 | 782 | 0.89 |
| lbp128+huf | 1.51 | 1.52–1.56 | 459 | 1.52 | 1.15 | 1.15–1.15 | 606 | 1.15 |
| lbp128+rans | 3.94 | 3.94–3.97 | 176 | 3.94 | 1.95 | 1.95–1.96 | 356 | 1.95 |
| lbp128+zstd3 | 2.31 | 2.28–2.34 | 300 | 2.32 | 1.29 | 1.28–1.29 | 541 | 1.28 |
| gpp+store | 2.62 | 2.63–2.64 | 266 | 2.62 | 0.90 | 0.91–0.91 | 772 | 0.90 |
| gpp+huf | 2.96 | 2.96–2.97 | 235 | 2.96 | 1.14 | 1.14–1.15 | 612 | 1.13 |
| gpp+rans | 5.16 | 5.17–5.18 | 135 | 5.16 | 1.79 | 1.79–1.81 | 388 | 1.79 |
| gpp+zstd3 | 3.50 | 3.48–3.66 | 199 | 3.50 | 1.22 | 1.22–1.24 | 571 | 1.22 |
| lpp128+store | 2.72 | 2.72–2.74 | 255 | 2.72 | 0.89 | 0.89–0.89 | 781 | 0.89 |
| lpp128+huf | 3.11 | 3.11–3.12 | 224 | 3.11 | 1.16 | 1.16–1.18 | 599 | 1.16 |
| lpp128+rans | 5.44 | 5.45–5.47 | 128 | 5.44 | 1.81 | 1.81–1.82 | 383 | 1.81 |
| lpp128+zstd3 | 3.59 | 3.58–3.65 | 194 | 3.59 | 1.23 | 1.23–1.25 | 563 | 1.23 |
| sel+store | 7.82 | 7.83–7.86 | 89 | 7.82 | 0.90 | 0.90–0.91 | 770 | 0.90 |
| sel+huf | 10.00 | 9.98–10.06 | 69 | 10.01 | 1.07 | 1.07–1.07 | 652 | 1.06 |
| sel+rans | 21.41 | 21.42–21.47 | 32 | 21.41 | 1.85 | 1.86–1.88 | 375 | 1.85 |
| sel+zstd3 | 13.65 | 13.74–14.03 | 51 | 13.66 | 0.97 | 0.98–1.00 | 716 | 0.97 |
| planes+store | 0.05 | 0.05–0.05 | 14712 | 0.05 | 0.04 | 0.04–0.04 | 19426 | 0.04 |
| planes+huf | 0.68 | 0.68–0.71 | 1023 | 0.68 | 0.36 | 0.36–0.37 | 1908 | 0.36 |
| planes+rans | 2.78 | 2.78–2.79 | 250 | 2.78 | 1.41 | 1.40–1.42 | 493 | 1.41 |
| planes+zstd3 | 1.41 | 1.39–1.44 | 494 | 1.41 | 0.44 | 0.44–0.44 | 1576 | 0.44 |
| exist+ent | 3.55 | 3.53–3.58 | 196 | 3.55 | 1.42 | 1.42–1.43 | 491 | 1.42 |
| planes+ent | 3.45 | 3.45–3.47 | 201 | 3.46 | 1.35 | 1.35–1.36 | 514 | 1.35 |
| lpp128+ent | 5.81 | 5.80–5.84 | 120 | 5.82 | 1.40 | 1.40–1.44 | 495 | 1.40 |
| exist+zstd19 | 164.18 | 163.76–165.46 | 4 | 164.19 | 0.76 | 0.78–0.79 | 918 | 0.74 |
| planes+zstd19 | 159.74 | 159.52–161.68 | 4 | 159.72 | 0.66 | 0.66–0.71 | 1057 | 0.64 |
| exist+prod3 | 2.72 | 2.70–2.81 | 255 | 2.72 | 0.33 | 0.33–0.33 | 2102 | 0.33 |
| planes+prod3 | 3.09 | 3.09–3.13 | 225 | 3.09 | 0.34 | 0.35–0.35 | 2016 | 0.34 |
| exist+prod19 | 166.52 | 164.43–168.54 | 4 | 166.51 | 0.69 | 0.69–0.71 | 1005 | 0.67 |
| planes+prod19 | 166.89 | 165.50–170.49 | 4 | 166.90 | 0.93 | 0.92–0.95 | 748 | 0.91 |
| exist+prod3g | 2.68 | 2.69–2.76 | 260 | 2.68 | 0.33 | 0.33–0.33 | 2125 | 0.33 |
| planes+prod3g | 3.10 | 3.06–3.15 | 224 | 3.10 | 0.34 | 0.34–0.35 | 2034 | 0.34 |

### Native kernel timing, regime B planner hc, real inputs (n=17, 5895191 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.68 | 0.67–0.71 | 8715 | 0.68 | 0.57 | 0.58–0.58 | 10332 | 0.57 |
| exist+huf | 5.50 | 5.47–5.62 | 1071 | 5.50 | 3.22 | 3.24–3.29 | 1829 | 3.22 |
| exist+rans | 19.28 | 19.27–19.42 | 306 | 19.28 | 14.89 | 14.88–15.01 | 396 | 14.88 |
| exist+zstd3 | 45.67 | 45.23–46.34 | 129 | 45.68 | 8.42 | 8.38–8.68 | 701 | 8.41 |
| gbp+store | 22.54 | 22.64–23.67 | 262 | 22.55 | 13.19 | 13.57–13.91 | 447 | 13.19 |
| gbp+huf | 26.37 | 26.69–27.43 | 224 | 26.38 | 15.99 | 16.04–16.64 | 369 | 15.99 |
| gbp+rans | 40.24 | 40.19–40.88 | 146 | 40.24 | 28.36 | 28.47–29.09 | 208 | 28.33 |
| gbp+zstd3 | 64.33 | 64.47–68.87 | 92 | 64.33 | 23.20 | 23.11–27.87 | 254 | 23.20 |
| lbp128+store | 21.97 | 22.04–22.84 | 268 | 21.98 | 14.02 | 14.19–14.76 | 421 | 14.02 |
| lbp128+huf | 25.52 | 25.33–26.11 | 231 | 25.53 | 16.68 | 16.60–17.55 | 353 | 16.68 |
| lbp128+rans | 36.89 | 37.26–37.63 | 160 | 36.90 | 26.00 | 26.04–26.63 | 227 | 26.00 |
| lbp128+zstd3 | 39.47 | 38.97–41.01 | 149 | 39.48 | 18.52 | 18.72–19.12 | 318 | 18.52 |
| gpp+store | 47.46 | 47.71–49.27 | 124 | 47.45 | 14.12 | 14.30–15.22 | 418 | 14.12 |
| gpp+huf | 50.01 | 50.69–51.58 | 118 | 50.03 | 16.64 | 16.97–17.51 | 354 | 16.64 |
| gpp+rans | 62.48 | 62.14–70.01 | 94 | 62.48 | 25.28 | 25.51–26.04 | 233 | 25.28 |
| gpp+zstd3 | 56.28 | 56.38–57.22 | 105 | 56.29 | 18.69 | 18.05–20.03 | 315 | 18.68 |
| lpp128+store | 48.15 | 48.45–49.22 | 122 | 48.16 | 15.23 | 15.42–16.69 | 387 | 15.23 |
| lpp128+huf | 51.63 | 51.28–52.24 | 114 | 51.64 | 18.39 | 18.25–19.05 | 321 | 18.39 |
| lpp128+rans | 61.17 | 61.36–61.98 | 96 | 61.18 | 25.11 | 25.02–26.02 | 235 | 25.11 |
| lpp128+zstd3 | 56.92 | 57.10–57.98 | 104 | 56.93 | 18.94 | 18.95–19.57 | 311 | 18.93 |
| sel+store | 139.11 | 139.39–142.06 | 42 | 139.12 | 14.99 | 14.96–15.34 | 393 | 14.98 |
| sel+huf | 156.22 | 156.77–160.32 | 38 | 156.24 | 16.74 | 17.00–17.79 | 352 | 16.73 |
| sel+rans | 217.91 | 220.10–223.52 | 27 | 217.91 | 25.21 | 25.57–26.15 | 234 | 25.20 |
| sel+zstd3 | 257.43 | 257.40–264.51 | 23 | 257.44 | 19.28 | 19.22–20.49 | 306 | 19.27 |
| planes+store | 0.68 | 0.66–0.73 | 8660 | 0.68 | 0.57 | 0.57–0.58 | 10356 | 0.57 |
| planes+huf | 5.52 | 5.54–5.59 | 1068 | 5.52 | 3.05 | 3.06–3.11 | 1932 | 3.05 |
| planes+rans | 19.29 | 19.28–19.85 | 306 | 19.29 | 14.91 | 14.85–15.12 | 396 | 14.90 |
| planes+zstd3 | 44.53 | 44.55–45.29 | 132 | 44.54 | 8.55 | 8.47–8.69 | 689 | 8.54 |
| exist+ent | 24.52 | 24.52–24.86 | 240 | 24.53 | 14.88 | 14.86–15.09 | 396 | 14.87 |
| planes+ent | 24.54 | 24.54–24.75 | 240 | 24.54 | 14.83 | 14.84–14.87 | 397 | 14.83 |
| lpp128+ent | 64.89 | 65.22–65.50 | 91 | 64.86 | 24.50 | 24.56–25.76 | 241 | 24.47 |
| exist+zstd19 | 2299.12 | 2298.44–2430.84 | 3 | 2298.90 | 6.80 | 6.82–6.91 | 867 | 6.76 |
| planes+zstd19 | 2321.72 | 2262.85–2505.08 | 3 | 2321.60 | 6.66 | 6.67–6.73 | 885 | 6.63 |
| exist+prod3 | 57.27 | 56.78–57.42 | 103 | 57.29 | 2.86 | 2.86–2.95 | 2062 | 2.85 |
| planes+prod3 | 57.11 | 56.71–57.84 | 103 | 57.12 | 2.88 | 2.89–2.93 | 2049 | 2.86 |
| exist+prod19 | 2265.69 | 2301.76–2492.60 | 3 | 2265.53 | 4.94 | 4.98–5.01 | 1194 | 4.89 |
| planes+prod19 | 2379.06 | 2345.97–2533.12 | 2 | 2378.83 | 5.20 | 5.23–5.32 | 1133 | 5.16 |
| exist+prod3g | 97.92 | 97.76–98.78 | 60 | 97.91 | 3.07 | 3.10–3.17 | 1918 | 3.06 |
| planes+prod3g | 98.12 | 98.09–99.30 | 60 | 98.12 | 3.08 | 3.12–3.18 | 1911 | 3.07 |

### End-to-end, complete planner fast, real inputs with INT blocks (n=9, 1451664 B). Encode for experimental rows = production compress - production residual-leaf re-encode (1.0 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 219566 | 46.5 | 2.01 | 1.85 |
| zstd -3 whole input (direct control) | 314431 | 5.5 | 1.68 | - |
| exist+store | 696829 | 45.6 | - | 1.49 |
| exist+huf | 263947 | 46.3 | - | 2.02 |
| exist+rans | 250544 | 48.5 | - | 2.99 |
| exist+zstd3 | 220663 | 47.6 | - | 1.94 |
| gbp+store | 443265 | 46.6 | - | 2.40 |
| gbp+huf | 286279 | 47.1 | - | 2.70 |
| gbp+rans | 282240 | 49.3 | - | 3.65 |
| gbp+zstd3 | 289510 | 48.7 | - | 2.90 |
| lbp128+store | 386290 | 46.7 | - | 2.38 |
| lbp128+huf | 256331 | 47.1 | - | 2.65 |
| lbp128+rans | 255013 | 49.6 | - | 3.54 |
| lbp128+zstd3 | 261447 | 48.4 | - | 2.77 |
| gpp+store | 313734 | 48.3 | - | 2.39 |
| gpp+huf | 237475 | 48.7 | - | 2.71 |
| gpp+rans | 240052 | 50.9 | - | 3.36 |
| gpp+zstd3 | 239899 | 49.5 | - | 2.72 |
| lpp128+store | 314912 | 48.4 | - | 2.41 |
| lpp128+huf | 258370 | 48.8 | - | 2.68 |
| lpp128+rans | 261156 | 51.2 | - | 3.50 |
| lpp128+zstd3 | 254274 | 49.6 | - | 2.78 |
| sel+store | 311817 | 53.6 | - | 2.44 |
| sel+huf | 218679 | 55.7 | - | 2.62 |
| sel+rans | 220164 | 67.1 | - | 3.42 |
| sel+zstd3 | 209921 | 59.9 | - | 2.62 |
| planes+store | 696885 | 45.6 | - | 1.56 |
| planes+huf | 245221 | 46.2 | - | 1.86 |
| planes+rans | 204160 | 48.4 | - | 2.98 |
| planes+zstd3 | 217052 | 47.8 | - | 1.99 |
| exist+ent | 250544 | 49.2 | - | 2.96 |
| planes+ent | 204155 | 49.2 | - | 2.86 |
| lpp128+ent | 257881 | 51.5 | - | 2.95 |
| exist+zstd19 | 213412 | 213.0 | - | 2.56 |
| planes+zstd19 | 208482 | 207.4 | - | 2.52 |
| exist+prod3 | 219681 | 48.9 | - | 1.86 |
| planes+prod3 | 210385 | 49.4 | - | 1.89 |
| exist+prod19 | 213374 | 214.8 | - | 2.63 |
| planes+prod19 | 205430 | 212.5 | - | 2.82 |
| exist+prod3g | 219681 | 49.0 | - | 1.87 |
| planes+prod3g | 210385 | 49.5 | - | 1.88 |

### End-to-end, complete planner hc, real inputs with INT blocks (n=17, 11852688 B). Encode for experimental rows = production compress - production residual-leaf re-encode (1480.6 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 2692268 | 3243.0 | 24.49 | 22.99 |
| zstd -3 whole input (direct control) | 4206612 | 68.1 | 15.20 | - |
| exist+store | 5898261 | 1764.0 | - | 19.19 |
| exist+huf | 2759360 | 1768.6 | - | 21.91 |
| exist+rans | 2710599 | 1782.1 | - | 33.80 |
| exist+zstd3 | 2898418 | 1809.2 | - | 27.82 |
| gbp+store | 5482207 | 1785.2 | - | 32.80 |
| gbp+huf | 2945867 | 1789.2 | - | 35.12 |
| gbp+rans | 2906361 | 1803.1 | - | 47.97 |
| gbp+zstd3 | 3066964 | 1828.1 | - | 42.99 |
| lbp128+store | 3895160 | 1784.2 | - | 32.67 |
| lbp128+huf | 2999101 | 1787.5 | - | 35.82 |
| lbp128+rans | 2979245 | 1799.6 | - | 45.32 |
| lbp128+zstd3 | 2997007 | 1802.6 | - | 38.23 |
| gpp+store | 3485215 | 1810.4 | - | 34.03 |
| gpp+huf | 2659326 | 1813.3 | - | 36.46 |
| gpp+rans | 2637054 | 1825.9 | - | 45.27 |
| gpp+zstd3 | 2652322 | 1820.6 | - | 38.13 |
| lpp128+store | 3445608 | 1811.0 | - | 34.95 |
| lpp128+huf | 2868578 | 1814.2 | - | 37.89 |
| lpp128+rans | 2859713 | 1824.5 | - | 45.68 |
| lpp128+zstd3 | 2769411 | 1820.2 | - | 38.86 |
| sel+store | 3431754 | 1902.0 | - | 34.69 |
| sel+huf | 2615846 | 1918.7 | - | 35.84 |
| sel+rans | 2590406 | 1981.9 | - | 44.20 |
| sel+zstd3 | 2606243 | 2023.4 | - | 39.02 |
| planes+store | 5898317 | 1764.1 | - | 19.23 |
| planes+huf | 2740634 | 1768.6 | - | 21.69 |
| planes+rans | 2664215 | 1782.0 | - | 33.72 |
| planes+zstd3 | 2894807 | 1809.6 | - | 27.97 |
| exist+ent | 2710599 | 1787.6 | - | 34.10 |
| planes+ent | 2664210 | 1787.5 | - | 33.85 |
| lpp128+ent | 2854705 | 1827.9 | - | 45.05 |
| exist+zstd19 | 2704637 | 4000.7 | - | 27.16 |
| planes+zstd19 | 2699707 | 3992.5 | - | 27.07 |
| exist+prod3 | 2715134 | 1821.9 | - | 22.72 |
| planes+prod3 | 2705838 | 1821.8 | - | 22.73 |
| exist+prod19 | 2692486 | 4132.5 | - | 26.24 |
| planes+prod19 | 2684542 | 4094.2 | - | 26.47 |
| exist+prod3g | 2715134 | 1862.3 | - | 23.05 |
| planes+prod3g | 2705838 | 1863.4 | - | 23.15 |

### Peak memory, fast mode, real inputs with INT blocks: process RSS high-water above the RSS just before the operation (KiB; /proc clear_refs reset after malloc_trim; input, frame and output buffers pre-touched)

| operation | config | max over inputs | median over inputs |
|---|---|---|---|
| prod_compress | - | 1716 | 1420 |
| prod_decompress_api | - | 428 | 268 |
| prod_decode_seq | - | 440 | 272 |
| exp_residual_encode | exist+store | 228 | 132 |
| exp_residual_decode | exist+store | 228 | 132 |
| exp_decode_seq | exist+store | 480 | 296 |
| exp_residual_encode | exist+huf | 168 | 96 |
| exp_residual_decode | exist+huf | 228 | 132 |
| exp_decode_seq | exist+huf | 480 | 296 |
| exp_residual_encode | exist+rans | 224 | 128 |
| exp_residual_decode | exist+rans | 288 | 192 |
| exp_decode_seq | exist+rans | 540 | 356 |
| exp_residual_encode | exist+zstd3 | 692 | 564 |
| exp_residual_decode | exist+zstd3 | 316 | 196 |
| exp_decode_seq | exist+zstd3 | 520 | 348 |
| exp_residual_encode | gbp+store | 624 | 332 |
| exp_residual_decode | gbp+store | 652 | 364 |
| exp_decode_seq | gbp+store | 712 | 416 |
| exp_residual_encode | gbp+huf | 568 | 328 |
| exp_residual_decode | gbp+huf | 652 | 364 |
| exp_decode_seq | gbp+huf | 672 | 432 |
| exp_residual_encode | gbp+rans | 628 | 364 |
| exp_residual_decode | gbp+rans | 712 | 428 |
| exp_decode_seq | gbp+rans | 732 | 444 |
| exp_residual_encode | gbp+zstd3 | 1112 | 788 |
| exp_residual_decode | gbp+zstd3 | 708 | 424 |
| exp_decode_seq | gbp+zstd3 | 708 | 472 |
| exp_residual_encode | lbp128+store | 688 | 364 |
| exp_residual_decode | lbp128+store | 632 | 364 |
| exp_decode_seq | lbp128+store | 652 | 384 |
| exp_residual_encode | lbp128+huf | 648 | 352 |
| exp_residual_decode | lbp128+huf | 632 | 364 |
| exp_decode_seq | lbp128+huf | 652 | 384 |
| exp_residual_encode | lbp128+rans | 688 | 384 |
| exp_residual_decode | lbp128+rans | 692 | 452 |
| exp_decode_seq | lbp128+rans | 712 | 468 |
| exp_residual_encode | lbp128+zstd3 | 1144 | 816 |
| exp_residual_decode | lbp128+zstd3 | 716 | 424 |
| exp_decode_seq | lbp128+zstd3 | 724 | 432 |
| exp_residual_encode | gpp+store | 572 | 328 |
| exp_residual_decode | gpp+store | 624 | 360 |
| exp_decode_seq | gpp+store | 640 | 380 |
| exp_residual_encode | gpp+huf | 564 | 324 |
| exp_residual_decode | gpp+huf | 624 | 360 |
| exp_decode_seq | gpp+huf | 640 | 380 |
| exp_residual_encode | gpp+rans | 608 | 344 |
| exp_residual_decode | gpp+rans | 712 | 424 |
| exp_decode_seq | gpp+rans | 732 | 440 |
| exp_residual_encode | gpp+zstd3 | 1032 | 768 |
| exp_residual_decode | gpp+zstd3 | 704 | 412 |
| exp_decode_seq | gpp+zstd3 | 704 | 420 |
| exp_residual_encode | lpp128+store | 572 | 340 |
| exp_residual_decode | lpp128+store | 624 | 356 |
| exp_decode_seq | lpp128+store | 640 | 376 |
| exp_residual_encode | lpp128+huf | 564 | 336 |
| exp_residual_decode | lpp128+huf | 624 | 356 |
| exp_decode_seq | lpp128+huf | 700 | 376 |
| exp_residual_encode | lpp128+rans | 608 | 352 |
| exp_residual_decode | lpp128+rans | 708 | 420 |
| exp_decode_seq | lpp128+rans | 728 | 468 |
| exp_residual_encode | lpp128+zstd3 | 1060 | 804 |
| exp_residual_decode | lpp128+zstd3 | 708 | 412 |
| exp_decode_seq | lpp128+zstd3 | 896 | 416 |
| exp_residual_encode | sel+store | 860 | 460 |
| exp_residual_decode | sel+store | 620 | 360 |
| exp_decode_seq | sel+store | 644 | 380 |
| exp_residual_encode | sel+huf | 736 | 424 |
| exp_residual_decode | sel+huf | 628 | 360 |
| exp_decode_seq | sel+huf | 900 | 384 |
| exp_residual_encode | sel+rans | 812 | 460 |
| exp_residual_decode | sel+rans | 692 | 452 |
| exp_decode_seq | sel+rans | 856 | 472 |
| exp_residual_encode | sel+zstd3 | 1464 | 932 |
| exp_residual_decode | sel+zstd3 | 708 | 336 |
| exp_decode_seq | sel+zstd3 | 896 | 404 |
| exp_residual_encode | planes+store | 228 | 132 |
| exp_residual_decode | planes+store | 172 | 100 |
| exp_decode_seq | planes+store | 424 | 252 |
| exp_residual_encode | planes+huf | 160 | 92 |
| exp_residual_decode | planes+huf | 172 | 100 |
| exp_decode_seq | planes+huf | 424 | 252 |
| exp_residual_encode | planes+rans | 200 | 116 |
| exp_residual_decode | planes+rans | 232 | 160 |
| exp_decode_seq | planes+rans | 548 | 312 |
| exp_residual_encode | planes+zstd3 | 652 | 552 |
| exp_residual_decode | planes+zstd3 | 264 | 164 |
| exp_decode_seq | planes+zstd3 | 492 | 336 |
| exp_residual_encode | exist+ent | 280 | 160 |
| exp_residual_decode | exist+ent | 288 | 192 |
| exp_decode_seq | exist+ent | 540 | 356 |
| exp_residual_encode | planes+ent | 248 | 144 |
| exp_residual_decode | planes+ent | 232 | 160 |
| exp_decode_seq | planes+ent | 484 | 312 |
| exp_residual_encode | lpp128+ent | 664 | 384 |
| exp_residual_decode | lpp128+ent | 692 | 420 |
| exp_decode_seq | lpp128+ent | 708 | 452 |
| exp_residual_encode | exist+zstd19 | 2436 | 2324 |
| exp_residual_decode | exist+zstd19 | 296 | 180 |
| exp_decode_seq | exist+zstd19 | 532 | 328 |
| exp_residual_encode | planes+zstd19 | 1648 | 1532 |
| exp_residual_decode | planes+zstd19 | 256 | 160 |
| exp_decode_seq | planes+zstd19 | 552 | 300 |
| exp_residual_encode | exist+prod3 | 912 | 692 |
| exp_residual_decode | exist+prod3 | 200 | 120 |
| exp_decode_seq | exist+prod3 | 440 | 268 |
| exp_residual_encode | planes+prod3 | 856 | 680 |
| exp_residual_decode | planes+prod3 | 148 | 92 |
| exp_decode_seq | planes+prod3 | 388 | 232 |
| exp_residual_encode | exist+prod19 | 2672 | 2456 |
| exp_residual_decode | exist+prod19 | 184 | 116 |
| exp_decode_seq | exist+prod19 | 416 | 256 |
| exp_residual_encode | planes+prod19 | 1844 | 1688 |
| exp_residual_decode | planes+prod19 | 176 | 128 |
| exp_decode_seq | planes+prod19 | 492 | 284 |
| exp_residual_encode | exist+prod3g | 912 | 692 |
| exp_residual_decode | exist+prod3g | 200 | 116 |
| exp_decode_seq | exist+prod3g | 440 | 268 |
| exp_residual_encode | planes+prod3g | 856 | 680 |
| exp_residual_decode | planes+prod3g | 148 | 92 |
| exp_decode_seq | planes+prod3g | 388 | 236 |
