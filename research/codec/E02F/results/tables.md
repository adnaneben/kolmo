
### Regime A (typed, kg_encode_num at zstd level 3), real inputs: total stored bytes (n=16; input 17985484 B; production typed payload 8832439 B; zstd -3 whole input 11323279 B)

| config | bytes | vs production payload | vs exist, same backend | inputs smaller/larger/equal vs production |
|---|---|---|---|---|
| exist+store | 20705759 | +134.43% | +0.00% | 0/16/0 |
| exist+huf | 9297114 | +5.26% | +0.00% | 0/16/0 |
| exist+rans | 16103698 | +82.32% | +0.00% | 1/15/0 |
| exist+zstd3 | 8837397 | +0.06% | +0.00% | 0/16/0 |
| gbp+store | 14747421 | +66.97% | -28.78% | 0/16/0 |
| gbp+huf | 10789142 | +22.15% | +16.05% | 0/16/0 |
| gbp+rans | 10723598 | +21.41% | -33.41% | 1/15/0 |
| gbp+zstd3 | 10697544 | +21.12% | +21.05% | 0/16/0 |
| lbp128+store | 9576994 | +8.43% | -53.75% | 2/14/0 |
| lbp128+huf | 9075573 | +2.75% | -2.38% | 6/10/0 |
| lbp128+rans | 9048540 | +2.45% | -43.81% | 3/13/0 |
| lbp128+zstd3 | 9074495 | +2.74% | +2.68% | 6/10/0 |
| gpp+store | 10319971 | +16.84% | -50.16% | 7/9/0 |
| gpp+huf | 9144058 | +3.53% | -1.65% | 9/7/0 |
| gpp+rans | 9102918 | +3.06% | -43.47% | 8/8/0 |
| gpp+zstd3 | 9155305 | +3.66% | +3.60% | 8/8/0 |
| lpp128+store | 9166348 | +3.78% | -55.73% | 8/8/0 |
| lpp128+huf | 8705149 | -1.44% | -6.37% | 12/4/0 |
| lpp128+rans | 8663545 | -1.91% | -46.20% | 12/4/0 |
| lpp128+zstd3 | 8734778 | -1.11% | -1.16% | 12/4/0 |
| sel+store | 9166304 | +3.78% | -55.73% | 8/8/0 |
| sel+huf | 8695691 | -1.55% | -6.47% | 12/4/0 |
| sel+rans | 8653534 | -2.03% | -46.26% | 12/4/0 |
| sel+zstd3 | 8646771 | -2.10% | -2.16% | 12/4/0 |
| planes+store | 20705916 | +134.43% | +0.00% | 0/16/0 |
| planes+huf | 9184312 | +3.98% | -1.21% | 8/8/0 |
| planes+rans | 12969581 | +46.84% | -19.46% | 6/10/0 |
| planes+zstd3 | 8826811 | -0.06% | -0.12% | 14/2/0 |
| exist+ent | 9288014 | +5.16% | +0.00% | 1/15/0 |
| planes+ent | 8848186 | +0.18% | -4.74% | 12/4/0 |
| lpp128+ent | 8661573 | -1.93% | -6.74% | 12/4/0 |
| exist+zstd19 | 8644929 | -2.12% | +0.00% | 16/0/0 |
| planes+zstd19 | 8613380 | -2.48% | -0.36% | 16/0/0 |
| exist+prod3 | 8832635 | +0.00% | +0.00% | 0/16/0 |
| planes+prod3 | 8867896 | +0.40% | +0.40% | 12/4/0 |
| exist+prod19 | 8645009 | -2.12% | +0.00% | 16/0/0 |
| planes+prod19 | 8612149 | -2.49% | -0.38% | 16/0/0 |
| exist+prod3g | 8832635 | +0.00% | +0.00% | 0/16/0 |
| planes+prod3g | 8819676 | -0.14% | -0.15% | 14/2/0 |

### Regime A per family: change vs production typed payload (fixed configurations)

| family | n | production B | exist+huf | exist+rans | lbp128+rans | gpp+rans | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+rans | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|---|---|
| F1_eeg | 4 | 1904076 | +13.44% | +94.72% | +8.35% | +4.84% | +4.12% | +3.73% | +3.72% | +3.47% | exist+zstd19 |
| F2_pageviews | 8 | 50988 | +16.91% | +20.44% | +2.70% | -4.69% | -6.62% | -5.92% | -6.02% | -6.04% | planes+zstd19 |
| F3_speech | 4 | 6877375 | +2.91% | +79.35% | +0.81% | +2.63% | -2.94% | -3.44% | -2.41% | -3.52% | lpp128+rans |

### Regime B (complete planner, fast), real inputs: complete-input stored bytes (n=16, 16 with INT blocks; input 17985484 B; production frame 8810204 B; zstd -3 11323279 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 17360900 | +97.05% | +0.00% | 0/16/0 |
| exist+huf | 9222158 | +4.68% | +0.00% | 0/16/0 |
| exist+rans | 10303403 | +16.95% | +0.00% | 1/15/0 |
| exist+zstd3 | 8816055 | +0.07% | +0.00% | 0/16/0 |
| gbp+store | 14020568 | +59.14% | -19.24% | 0/16/0 |
| gbp+huf | 10688342 | +21.32% | +15.90% | 0/16/0 |
| gbp+rans | 10629074 | +20.65% | +3.16% | 0/16/0 |
| gbp+zstd3 | 10560526 | +19.87% | +19.79% | 0/16/0 |
| lbp128+store | 9563933 | +8.56% | -44.91% | 2/14/0 |
| lbp128+huf | 9064036 | +2.88% | -1.71% | 6/10/0 |
| lbp128+rans | 9036952 | +2.57% | -12.29% | 3/13/0 |
| lbp128+zstd3 | 9055205 | +2.78% | +2.71% | 6/10/0 |
| gpp+store | 10333253 | +17.29% | -40.48% | 7/9/0 |
| gpp+huf | 9175814 | +4.15% | -0.50% | 9/7/0 |
| gpp+rans | 9139972 | +3.74% | -11.29% | 8/8/0 |
| gpp+zstd3 | 9172934 | +4.12% | +4.05% | 8/8/0 |
| lpp128+store | 9154928 | +3.91% | -47.27% | 8/8/0 |
| lpp128+huf | 8694476 | -1.31% | -5.72% | 12/4/0 |
| lpp128+rans | 8660834 | -1.70% | -15.94% | 12/4/0 |
| lpp128+zstd3 | 8719883 | -1.03% | -1.09% | 12/4/0 |
| sel+store | 9154403 | +3.91% | -47.27% | 8/8/0 |
| sel+huf | 8679649 | -1.48% | -5.88% | 12/4/0 |
| sel+rans | 8642070 | -1.91% | -16.12% | 12/4/0 |
| sel+zstd3 | 8639236 | -1.94% | -2.01% | 12/4/0 |
| planes+store | 17361126 | +97.06% | +0.00% | 0/16/0 |
| planes+huf | 9163423 | +4.01% | -0.64% | 8/8/0 |
| planes+rans | 9328907 | +5.89% | -9.46% | 5/11/0 |
| planes+zstd3 | 8805079 | -0.06% | -0.12% | 12/4/0 |
| exist+ent | 9210447 | +4.54% | +0.00% | 1/15/0 |
| planes+ent | 8812857 | +0.03% | -4.32% | 11/5/0 |
| lpp128+ent | 8655006 | -1.76% | -6.03% | 12/4/0 |
| exist+zstd19 | 8641618 | -1.91% | +0.00% | 16/0/0 |
| planes+zstd19 | 8610866 | -2.26% | -0.36% | 16/0/0 |
| exist+prod3 | 8810565 | +0.00% | +0.00% | 0/16/0 |
| planes+prod3 | 8797102 | -0.15% | -0.15% | 11/5/0 |
| exist+prod19 | 8641763 | -1.91% | +0.00% | 16/0/0 |
| planes+prod19 | 8609473 | -2.28% | -0.37% | 16/0/0 |
| exist+prod3g | 8810565 | +0.00% | +0.00% | 0/16/0 |
| planes+prod3g | 8797102 | -0.15% | -0.15% | 11/5/0 |

### Regime B (complete planner, hc), real inputs: complete-input stored bytes (n=16, 13 with INT blocks; input 17985484 B; production frame 8645263 B; zstd -3 11323279 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 20687204 | +139.29% | +0.00% | 0/13/3 |
| exist+huf | 9294042 | +7.50% | +0.00% | 0/13/3 |
| exist+rans | 16099943 | +86.23% | +0.00% | 0/13/3 |
| exist+zstd3 | 8837090 | +2.22% | +0.00% | 0/13/3 |
| gbp+store | 14741644 | +70.52% | -28.74% | 0/13/3 |
| gbp+huf | 10786725 | +24.77% | +16.06% | 0/13/3 |
| gbp+rans | 10720359 | +24.00% | -33.41% | 0/13/3 |
| gbp+zstd3 | 10695097 | +23.71% | +21.03% | 0/13/3 |
| lbp128+store | 9576341 | +10.77% | -53.71% | 0/13/3 |
| lbp128+huf | 9075486 | +4.98% | -2.35% | 1/12/3 |
| lbp128+rans | 9047887 | +4.66% | -43.80% | 0/13/3 |
| lbp128+zstd3 | 9074375 | +4.96% | +2.69% | 0/13/3 |
| gpp+store | 10320184 | +19.37% | -50.11% | 3/10/3 |
| gpp+huf | 9145025 | +5.78% | -1.60% | 5/8/3 |
| gpp+rans | 9103520 | +5.30% | -43.46% | 5/8/3 |
| gpp+zstd3 | 9156022 | +5.91% | +3.61% | 6/7/3 |
| lpp128+store | 9167157 | +6.04% | -55.69% | 4/9/3 |
| lpp128+huf | 8706250 | +0.71% | -6.32% | 8/5/3 |
| lpp128+rans | 8664515 | +0.22% | -46.18% | 8/5/3 |
| lpp128+zstd3 | 8735767 | +1.05% | -1.15% | 6/7/3 |
| sel+store | 9167125 | +6.04% | -55.69% | 4/9/3 |
| sel+huf | 8696896 | +0.60% | -6.43% | 8/5/3 |
| sel+rans | 8654552 | +0.11% | -46.24% | 8/5/3 |
| sel+zstd3 | 8647850 | +0.03% | -2.14% | 7/6/3 |
| planes+store | 20687326 | +139.29% | +0.00% | 0/13/3 |
| planes+huf | 9185013 | +6.24% | -1.17% | 4/9/3 |
| planes+rans | 12963579 | +49.95% | -19.48% | 2/11/3 |
| planes+zstd3 | 8827888 | +2.11% | -0.10% | 5/8/3 |
| exist+ent | 9284942 | +7.40% | +0.00% | 0/13/3 |
| planes+ent | 8849746 | +2.37% | -4.69% | 6/7/3 |
| lpp128+ent | 8662674 | +0.20% | -6.70% | 8/5/3 |
| exist+zstd19 | 8645361 | +0.00% | +0.00% | 0/13/3 |
| planes+zstd19 | 8614860 | -0.35% | -0.35% | 9/4/3 |
| exist+prod3 | 8832318 | +2.16% | +0.00% | 0/13/3 |
| planes+prod3 | 8869454 | +2.59% | +0.42% | 5/8/3 |
| exist+prod19 | 8645426 | +0.00% | +0.00% | 0/13/3 |
| planes+prod19 | 8613704 | -0.37% | -0.37% | 10/3/3 |
| exist+prod3g | 8832318 | +2.16% | +0.00% | 0/13/3 |
| planes+prod3g | 8821234 | +2.04% | -0.13% | 5/8/3 |

### Regime B per input: production planner selection

| input | mode | origin | bytes | frame | zstd -3 | INT blocks | prod residual codec | INT shape | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|
| eeg_S001R01.npy | fast | real | 1249408 | 486201 | 681791 | 2/2 | zstd | w2o1ow1,w2o1ow2 | planes+rans |
| eeg_S001R01.npy | hc | real | 1249408 | 485128 | 681791 | 1/1 | zstd | w2o1ow2 | planes+rans |
| eeg_S002R01.npy | fast | real | 1249408 | 478269 | 646322 | 2/2 | zstd | w2o1ow2 | exist+zstd19 |
| eeg_S002R01.npy | hc | real | 1249408 | 477815 | 646322 | 1/1 | zstd | w2o1ow2 | exist+zstd19 |
| eeg_S003R01.npy | fast | real | 1249408 | 509174 | 703991 | 2/2 | zstd | w2o1ow2 | exist+zstd19 |
| eeg_S003R01.npy | hc | real | 1249408 | 508097 | 703991 | 1/1 | zstd | w2o1ow2 | exist+zstd19 |
| eeg_S004R01.npy | fast | real | 1249408 | 430855 | 629877 | 2/2 | zstd | w2o1ow1,w2o1ow2 | exist+zstd19 |
| eeg_S004R01.npy | hc | real | 1249408 | 428133 | 629877 | 1/1 | zstd | w2o1ow2 | exist+zstd19 |
| fsdd_jackson.npy | fast | real | 4131808 | 2979875 | 3653994 | 4/4 | zstd | w2o1ow2,w2o2ow2,w2o2ow4 | lpp128+rans |
| fsdd_jackson.npy | hc | real | 4131808 | 2953648 | 3653994 | 1/1 | zstd | w2o2ow4 | lpp128+rans |
| fsdd_nicolas.npy | fast | real | 2793630 | 575927 | 778002 | 3/3 | huf,zstd | w2o1ow1 | exist+zstd19 |
| fsdd_nicolas.npy | hc | real | 2793630 | 528936 | 778002 | 1/1 | zstd | w2o1ow1 | gpp+zstd3 |
| fsdd_theo.npy | fast | real | 3111026 | 1734900 | 2189831 | 3/3 | zstd | w2o1ow2 | lpp128+rans |
| fsdd_theo.npy | hc | real | 3111026 | 1689229 | 2189831 | 1/1 | zstd | w2o1ow2 | lpp128+rans |
| fsdd_yweweler.npy | fast | real | 2833468 | 1563599 | 1971770 | 3/3 | zstd | w2o1ow2 | lpp128+rans |
| fsdd_yweweler.npy | hc | real | 2833468 | 1524667 | 1971770 | 1/1 | zstd | w2o1ow2 | lpp128+rans |
| pageviews_Albert_Einstein.npy | fast | real | 14740 | 6843 | 8819 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Albert_Einstein.npy | hc | real | 14740 | 6792 | 8819 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Earth.npy | fast | real | 14740 | 6659 | 8835 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Earth.npy | hc | real | 14740 | 6516 | 8835 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_London.npy | fast | real | 14740 | 6408 | 8568 | 1/1 | huf | w4o1ow2 | planes+huf |
| pageviews_London.npy | hc | real | 14740 | 6278 | 8568 | 1/1 | zstd | w4o1ow2 | planes+huf |
| pageviews_Mathematics.npy | fast | real | 14740 | 6085 | 7992 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Mathematics.npy | hc | real | 14740 | 5668 | 7992 | 0/1 | - | - | - |
| pageviews_Moon.npy | fast | real | 14740 | 6259 | 8328 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Moon.npy | hc | real | 14740 | 6075 | 8328 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Music.npy | fast | real | 14740 | 6485 | 8534 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Music.npy | hc | real | 14740 | 6266 | 8534 | 0/1 | - | - | - |
| pageviews_Python_programming_language.npy | fast | real | 14740 | 6458 | 8477 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Python_programming_language.npy | hc | real | 14740 | 6275 | 8477 | 1/1 | zstd | w4o1ow4 | planes+zstd19 |
| pageviews_Water.npy | fast | real | 14740 | 6207 | 8148 | 1/1 | huf | w4o1ow2 | planes+huf |
| pageviews_Water.npy | hc | real | 14740 | 5740 | 8148 | 0/1 | - | - | - |

### Regime B per input with INT blocks: complete-input change vs production frame

| input | mode | origin | production frame | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| eeg_S001R01.npy | fast | real | 486201 | +0.00% | +7.84% | +7.07% | +4.36% | +3.97% | +3.85% | +3.73% | +3.13% | +0.00% |
| eeg_S001R01.npy | hc | real | 485128 | +0.22% | +8.06% | +7.30% | +4.54% | +3.99% | +4.07% | +4.54% | +3.99% | +0.22% |
| eeg_S002R01.npy | fast | real | 478269 | +0.00% | +2.28% | +1.37% | +3.00% | +2.75% | +2.53% | +2.28% | +1.66% | +0.00% |
| eeg_S002R01.npy | hc | real | 477815 | +0.09% | +2.36% | +1.60% | +3.04% | +2.63% | +2.55% | +2.36% | +1.60% | +0.09% |
| eeg_S003R01.npy | fast | real | 509174 | +0.00% | +5.76% | +4.81% | +4.79% | +4.55% | +4.45% | +4.79% | +4.55% | +0.00% |
| eeg_S003R01.npy | hc | real | 508097 | +0.19% | +5.97% | +5.07% | +4.97% | +4.57% | +4.64% | +4.97% | +4.57% | +0.19% |
| eeg_S004R01.npy | fast | real | 430855 | +0.00% | +5.11% | +4.28% | +4.48% | +4.33% | +4.19% | +3.70% | +3.45% | +0.00% |
| eeg_S004R01.npy | hc | real | 428133 | +0.62% | +5.77% | +4.93% | +5.10% | +4.91% | +4.80% | +5.10% | +4.91% | +0.62% |
| fsdd_jackson.npy | fast | real | 2979875 | +0.00% | +4.67% | +5.17% | -1.58% | -2.03% | -1.11% | -1.58% | -2.03% | -1.11% |
| fsdd_jackson.npy | hc | real | 2953648 | +1.41% | +4.69% | +5.58% | -0.26% | -0.80% | +0.19% | -0.26% | -0.80% | +0.19% |
| fsdd_nicolas.npy | fast | real | 575927 | +0.98% | -5.37% | -7.05% | -4.98% | -5.54% | -5.50% | -5.80% | -6.52% | -7.05% |
| fsdd_nicolas.npy | hc | real | 528936 | +10.59% | +2.31% | -0.27% | +3.42% | +2.76% | +2.93% | +2.31% | +1.81% | -0.27% |
| fsdd_theo.npy | fast | real | 1734900 | +0.00% | +2.98% | +3.18% | -3.48% | -3.88% | -2.47% | -3.48% | -3.88% | -2.47% |
| fsdd_theo.npy | hc | real | 1689229 | +2.72% | +5.76% | +6.31% | -0.89% | -1.37% | +0.27% | -0.89% | -1.37% | +0.27% |
| fsdd_yweweler.npy | fast | real | 1563599 | +0.00% | +6.95% | +7.22% | -3.53% | -3.90% | -3.28% | -3.53% | -3.90% | -3.28% |
| fsdd_yweweler.npy | hc | real | 1524667 | +2.77% | +9.69% | +9.95% | -1.10% | -1.54% | -0.65% | -1.10% | -1.54% | -0.65% |
| pageviews_Albert_Einstein.npy | fast | real | 6843 | +0.09% | -6.33% | -5.79% | -5.10% | -4.40% | -4.60% | -6.33% | -4.44% | -5.79% |
| pageviews_Albert_Einstein.npy | hc | real | 6792 | +0.82% | -5.64% | -5.09% | -4.40% | -3.70% | -3.90% | -5.64% | -3.74% | -5.09% |
| pageviews_Earth.npy | fast | real | 6659 | +0.09% | -5.09% | -4.11% | -7.33% | -6.71% | -6.71% | -7.33% | -6.71% | -6.71% |
| pageviews_Earth.npy | hc | real | 6516 | +2.27% | -3.02% | -2.03% | -5.31% | -4.68% | -4.68% | -5.31% | -4.68% | -4.68% |
| pageviews_London.npy | fast | real | 6408 | +0.25% | -7.90% | -7.04% | -7.21% | -6.65% | -6.76% | -7.90% | -6.66% | -7.04% |
| pageviews_London.npy | hc | real | 6278 | +2.31% | -6.01% | -5.13% | -5.30% | -4.73% | -4.84% | -6.01% | -4.75% | -5.13% |
| pageviews_Mathematics.npy | fast | real | 6085 | +0.10% | -9.73% | -9.19% | -8.02% | -7.13% | -7.71% | -9.73% | -7.92% | -9.19% |
| pageviews_Moon.npy | fast | real | 6259 | +0.10% | -6.52% | -4.70% | -4.90% | -4.17% | -4.15% | -6.52% | -4.35% | -4.70% |
| pageviews_Moon.npy | hc | real | 6075 | +3.11% | -3.70% | -1.83% | -2.04% | -1.28% | -1.27% | -3.70% | -1.47% | -1.83% |
| pageviews_Music.npy | fast | real | 6485 | +0.09% | -2.36% | +0.15% | -5.81% | -5.17% | -4.90% | -5.81% | -5.17% | -4.90% |
| pageviews_Python_programming_language.npy | fast | real | 6458 | +0.09% | -5.17% | -4.52% | -5.92% | -5.11% | -5.33% | -5.92% | -5.11% | -5.33% |
| pageviews_Python_programming_language.npy | hc | real | 6275 | +3.00% | -2.42% | -1.75% | -3.19% | -2.36% | -2.58% | -3.19% | -2.36% | -2.58% |
| pageviews_Water.npy | fast | real | 6207 | +0.18% | -8.15% | -7.28% | -8.38% | -7.81% | -7.83% | -8.38% | -7.81% | -7.83% |

### Regime A per input: typed payload change vs production typed payload

| input | origin | production payload | INT shape | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| eeg_S001R01.npy | real | 486130 | o1ow2 | +0.00% | +7.83% | +7.07% | +4.31% | +3.76% | +3.85% | +4.31% | +3.76% | +0.00% |
| eeg_S002R01.npy | real | 478199 | o1ow2 | +0.00% | +2.27% | +1.51% | +2.95% | +2.53% | +2.45% | +2.27% | +1.51% | +0.00% |
| eeg_S003R01.npy | real | 509002 | o1ow2 | +0.00% | +5.77% | +4.88% | +4.77% | +4.38% | +4.45% | +4.77% | +4.38% | +0.00% |
| eeg_S004R01.npy | real | 430745 | o1ow2 | +0.00% | +5.12% | +4.28% | +4.45% | +4.26% | +4.15% | +4.45% | +4.26% | +0.00% |
| fsdd_jackson.npy | real | 2995352 | o2ow4 | +0.00% | +3.23% | +4.11% | -1.65% | -2.19% | -1.21% | -1.65% | -2.19% | -1.21% |
| fsdd_nicolas.npy | real | 580058 | o1ow1 | +0.83% | -6.72% | -9.06% | -5.70% | -6.30% | -6.15% | -6.72% | -7.18% | -9.06% |
| fsdd_theo.npy | real | 1735157 | o1ow2 | +0.00% | +2.96% | +3.49% | -3.51% | -3.98% | -2.39% | -3.51% | -3.98% | -2.39% |
| fsdd_yweweler.npy | real | 1566808 | o1ow2 | +0.00% | +6.73% | +6.99% | -3.76% | -4.19% | -3.33% | -3.76% | -4.19% | -3.33% |
| pageviews_Albert_Einstein.npy | real | 6791 | o1ow4 | +0.09% | -6.38% | -5.83% | -5.14% | -4.43% | -4.64% | -6.38% | -4.48% | -5.83% |
| pageviews_Earth.npy | real | 6607 | o1ow4 | +0.09% | -5.13% | -4.15% | -7.39% | -6.77% | -6.77% | -7.39% | -6.77% | -6.77% |
| pageviews_London.npy | real | 6356 | o1ow2 | +0.25% | -7.96% | -7.10% | -7.27% | -6.70% | -6.81% | -7.96% | -6.72% | -7.10% |
| pageviews_Mathematics.npy | real | 6033 | o1ow4 | +0.10% | -9.81% | -9.27% | -8.09% | -7.19% | -7.77% | -9.81% | -7.99% | -9.27% |
| pageviews_Moon.npy | real | 6207 | o1ow4 | +0.10% | -6.57% | -4.74% | -4.95% | -4.20% | -4.19% | -6.57% | -4.38% | -4.74% |
| pageviews_Music.npy | real | 6433 | o1ow4 | +0.09% | -2.38% | +0.16% | -5.86% | -5.21% | -4.94% | -5.86% | -5.21% | -4.94% |
| pageviews_Python_programming_language.npy | real | 6406 | o1ow4 | +0.09% | -5.21% | -4.56% | -5.96% | -5.15% | -5.37% | -5.96% | -5.15% | -5.37% |
| pageviews_Water.npy | real | 6155 | o1ow2 | +0.18% | -8.22% | -7.34% | -8.45% | -7.88% | -7.90% | -8.45% | -7.88% | -7.90% |

### Native kernel timing, regime A typed, real inputs (n=16, 20703704 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 1.17 | 1.09–1.60 | 17641 | 1.17 | 0.93 | 0.91–1.81 | 22377 | 0.93 |
| exist+huf | 9.65 | 9.63–10.02 | 2145 | 9.65 | 6.44 | 6.40–6.97 | 3213 | 6.44 |
| exist+rans | 31.02 | 30.82–31.26 | 668 | 31.01 | 16.88 | 16.89–17.27 | 1227 | 16.88 |
| exist+zstd3 | 27.74 | 27.65–28.00 | 746 | 27.74 | 7.48 | 7.49–7.63 | 2766 | 7.48 |
| gbp+store | 19.43 | 19.46–24.82 | 1065 | 19.43 | 11.99 | 12.07–12.56 | 1727 | 11.99 |
| gbp+huf | 27.77 | 27.95–29.31 | 745 | 27.78 | 18.24 | 18.20–19.09 | 1135 | 18.23 |
| gbp+rans | 51.88 | 51.90–52.42 | 399 | 51.89 | 41.85 | 41.63–42.62 | 495 | 41.85 |
| gbp+zstd3 | 59.26 | 60.29–64.27 | 349 | 59.26 | 20.77 | 20.69–21.52 | 997 | 20.77 |
| lbp128+store | 19.20 | 19.15–19.77 | 1078 | 19.21 | 11.34 | 11.33–11.40 | 1826 | 11.33 |
| lbp128+huf | 24.72 | 24.68–25.01 | 838 | 24.72 | 16.23 | 16.20–16.36 | 1276 | 16.23 |
| lbp128+rans | 35.61 | 36.03–36.25 | 581 | 35.61 | 27.64 | 27.66–28.02 | 749 | 27.63 |
| lbp128+zstd3 | 32.81 | 33.25–35.82 | 631 | 32.80 | 16.11 | 16.12–16.97 | 1285 | 16.11 |
| gpp+store | 36.84 | 36.60–38.14 | 562 | 36.84 | 11.56 | 11.53–11.59 | 1791 | 11.56 |
| gpp+huf | 42.79 | 42.51–44.05 | 484 | 42.79 | 16.68 | 16.69–16.86 | 1241 | 16.68 |
| gpp+rans | 56.74 | 56.84–60.73 | 365 | 56.72 | 29.68 | 29.78–30.63 | 697 | 29.67 |
| gpp+zstd3 | 54.89 | 55.31–58.05 | 377 | 54.89 | 17.06 | 17.17–17.95 | 1214 | 17.06 |
| lpp128+store | 39.98 | 39.60–44.62 | 518 | 39.99 | 12.29 | 12.35–12.95 | 1685 | 12.29 |
| lpp128+huf | 44.87 | 44.72–45.08 | 461 | 44.87 | 16.68 | 16.72–16.77 | 1241 | 16.68 |
| lpp128+rans | 54.58 | 54.44–55.85 | 379 | 54.56 | 25.98 | 25.97–26.28 | 797 | 25.98 |
| lpp128+zstd3 | 50.82 | 51.10–55.86 | 407 | 50.83 | 14.62 | 14.75–15.31 | 1416 | 14.62 |
| sel+store | 123.37 | 123.63–125.71 | 168 | 123.37 | 19.71 | 19.64–19.86 | 1050 | 19.71 |
| sel+huf | 157.66 | 156.61–163.79 | 131 | 157.67 | 24.19 | 24.14–24.38 | 856 | 24.19 |
| sel+rans | 240.30 | 239.62–256.10 | 86 | 240.28 | 34.25 | 34.07–36.98 | 605 | 34.25 |
| sel+zstd3 | 229.84 | 229.94–239.39 | 90 | 229.85 | 19.75 | 19.69–20.51 | 1048 | 19.74 |
| planes+store | 1.37 | 1.36–1.39 | 15135 | 1.37 | 0.79 | 0.79–0.80 | 26343 | 0.79 |
| planes+huf | 9.62 | 9.59–9.69 | 2152 | 9.62 | 5.83 | 5.82–5.90 | 3550 | 5.83 |
| planes+rans | 30.19 | 30.11–30.55 | 686 | 30.19 | 26.71 | 26.59–26.85 | 775 | 26.71 |
| planes+zstd3 | 27.34 | 26.87–31.69 | 757 | 27.34 | 7.27 | 7.22–7.36 | 2848 | 7.27 |
| exist+ent | 39.89 | 39.58–41.22 | 519 | 39.89 | 8.66 | 8.62–9.44 | 2391 | 8.66 |
| planes+ent | 39.17 | 39.18–39.44 | 529 | 39.17 | 26.74 | 26.57–28.99 | 774 | 26.74 |
| lpp128+ent | 59.77 | 60.24–65.43 | 346 | 59.77 | 25.67 | 25.99–26.32 | 807 | 25.67 |
| exist+zstd19 | 1780.32 | 1790.99–1832.31 | 12 | 1780.10 | 8.28 | 8.26–8.72 | 2501 | 8.27 |
| planes+zstd19 | 1724.41 | 1719.72–1768.23 | 12 | 1724.21 | 7.94 | 7.90–8.49 | 2608 | 7.93 |
| exist+prod3 | 38.50 | 38.33–40.02 | 538 | 38.50 | 6.40 | 6.40–6.60 | 3233 | 6.40 |
| planes+prod3 | 43.55 | 43.34–46.28 | 475 | 43.55 | 6.42 | 6.40–6.45 | 3226 | 6.42 |
| exist+prod19 | 1814.06 | 1811.09–1849.65 | 11 | 1813.96 | 7.66 | 7.64–7.70 | 2704 | 7.65 |
| planes+prod19 | 1790.39 | 1771.85–1830.94 | 12 | 1790.23 | 8.18 | 8.21–8.24 | 2531 | 8.17 |
| exist+prod3g | 44.75 | 44.65–47.76 | 463 | 44.75 | 6.48 | 6.43–6.51 | 3196 | 6.48 |
| planes+prod3g | 52.22 | 51.70–55.09 | 396 | 52.21 | 6.36 | 6.37–6.43 | 3258 | 6.35 |

### Native kernel timing, regime B planner fast, real inputs (n=16, 17357246 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.73 | 0.73–0.75 | 23706 | 0.73 | 0.58 | 0.56–0.59 | 30141 | 0.58 |
| exist+huf | 8.97 | 8.89–9.04 | 1936 | 8.97 | 5.95 | 5.94–6.01 | 2919 | 5.95 |
| exist+rans | 34.53 | 34.43–34.73 | 503 | 34.53 | 30.43 | 30.33–30.42 | 570 | 30.42 |
| exist+zstd3 | 24.62 | 24.49–24.96 | 705 | 24.62 | 7.12 | 7.08–7.15 | 2437 | 7.12 |
| gbp+store | 17.46 | 17.40–17.51 | 994 | 17.46 | 10.79 | 10.66–10.92 | 1608 | 10.79 |
| gbp+huf | 25.53 | 25.54–25.66 | 680 | 25.52 | 17.03 | 16.92–17.21 | 1019 | 17.03 |
| gbp+rans | 50.06 | 49.62–50.09 | 347 | 50.06 | 41.11 | 40.79–41.11 | 422 | 41.10 |
| gbp+zstd3 | 50.08 | 49.93–50.43 | 347 | 50.08 | 18.92 | 18.81–18.99 | 917 | 18.92 |
| lbp128+store | 17.97 | 17.91–17.95 | 966 | 17.97 | 10.91 | 10.78–10.98 | 1591 | 10.91 |
| lbp128+huf | 23.45 | 23.30–23.60 | 740 | 23.44 | 15.68 | 15.56–15.77 | 1107 | 15.68 |
| lbp128+rans | 34.98 | 34.66–35.06 | 496 | 34.96 | 27.32 | 27.18–27.47 | 635 | 27.31 |
| lbp128+zstd3 | 29.33 | 29.02–29.78 | 592 | 29.33 | 15.87 | 15.80–16.07 | 1094 | 15.87 |
| gpp+store | 35.37 | 35.25–35.45 | 491 | 35.37 | 10.87 | 10.80–11.03 | 1597 | 10.87 |
| gpp+huf | 41.33 | 41.19–41.94 | 420 | 41.33 | 16.11 | 16.03–16.13 | 1077 | 16.10 |
| gpp+rans | 56.93 | 56.60–57.06 | 305 | 56.93 | 29.64 | 29.52–29.81 | 586 | 29.64 |
| gpp+zstd3 | 50.25 | 50.21–50.58 | 345 | 50.25 | 16.64 | 16.66–16.71 | 1043 | 16.63 |
| lpp128+store | 38.20 | 37.99–38.27 | 454 | 38.20 | 11.44 | 11.40–11.52 | 1517 | 11.44 |
| lpp128+huf | 43.65 | 43.63–43.96 | 398 | 43.67 | 16.18 | 16.09–16.51 | 1073 | 16.18 |
| lpp128+rans | 53.96 | 53.31–54.69 | 322 | 53.96 | 25.70 | 25.46–25.80 | 675 | 25.70 |
| lpp128+zstd3 | 47.85 | 47.77–48.59 | 363 | 47.85 | 14.29 | 14.36–14.53 | 1215 | 14.29 |
| sel+store | 110.53 | 110.21–110.95 | 157 | 110.53 | 11.52 | 11.49–11.57 | 1506 | 11.52 |
| sel+huf | 143.48 | 142.53–143.58 | 121 | 143.47 | 16.04 | 16.00–16.13 | 1082 | 16.05 |
| sel+rans | 230.32 | 229.59–230.57 | 75 | 230.31 | 25.85 | 25.78–25.87 | 672 | 25.85 |
| sel+zstd3 | 199.84 | 199.09–200.53 | 87 | 199.83 | 11.50 | 11.42–11.72 | 1510 | 11.49 |
| planes+store | 0.82 | 0.81–0.84 | 21208 | 0.82 | 0.55 | 0.56–0.56 | 31276 | 0.55 |
| planes+huf | 8.95 | 8.94–9.12 | 1939 | 8.95 | 5.59 | 5.56–5.63 | 3108 | 5.58 |
| planes+rans | 30.35 | 30.23–30.48 | 572 | 30.35 | 26.97 | 26.84–27.38 | 644 | 26.96 |
| planes+zstd3 | 24.65 | 24.34–25.23 | 704 | 24.65 | 7.09 | 7.08–7.15 | 2448 | 7.09 |
| exist+ent | 43.05 | 43.00–43.38 | 403 | 43.04 | 8.58 | 8.57–8.61 | 2023 | 8.58 |
| planes+ent | 39.11 | 38.89–39.20 | 444 | 39.11 | 26.30 | 26.16–26.33 | 660 | 26.30 |
| lpp128+ent | 59.60 | 59.55–60.04 | 291 | 59.60 | 25.02 | 24.90–25.11 | 694 | 25.02 |
| exist+zstd19 | 1347.32 | 1353.62–1415.99 | 13 | 1347.12 | 8.12 | 8.15–8.32 | 2139 | 8.10 |
| planes+zstd19 | 1406.34 | 1407.29–1441.58 | 12 | 1406.23 | 7.84 | 7.84–7.93 | 2215 | 7.82 |
| exist+prod3 | 40.16 | 40.14–40.85 | 432 | 40.16 | 6.62 | 6.61–6.71 | 2622 | 6.62 |
| planes+prod3 | 41.91 | 41.47–43.75 | 414 | 41.92 | 6.64 | 6.61–6.69 | 2614 | 6.63 |
| exist+prod19 | 1350.78 | 1351.58–1402.57 | 13 | 1350.63 | 7.63 | 7.62–7.80 | 2275 | 7.60 |
| planes+prod19 | 1425.83 | 1422.54–1452.36 | 12 | 1425.75 | 8.70 | 8.69–8.78 | 1995 | 8.68 |
| exist+prod3g | 43.02 | 42.81–43.50 | 403 | 43.02 | 6.66 | 6.65–6.74 | 2607 | 6.66 |
| planes+prod3g | 46.20 | 46.03–47.84 | 376 | 46.18 | 6.64 | 6.62–6.69 | 2616 | 6.63 |

### Native kernel timing, regime B planner hc, real inputs (n=13, 20667184 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 1.14 | 1.13–1.17 | 18200 | 1.14 | 0.84 | 0.82–0.85 | 24564 | 0.84 |
| exist+huf | 9.71 | 9.68–9.85 | 2128 | 9.71 | 6.34 | 6.32–6.73 | 3261 | 6.34 |
| exist+rans | 30.46 | 30.34–31.66 | 678 | 30.46 | 16.69 | 16.61–16.73 | 1238 | 16.69 |
| exist+zstd3 | 26.87 | 26.82–28.19 | 769 | 26.87 | 7.28 | 7.22–7.38 | 2838 | 7.28 |
| gbp+store | 18.90 | 18.75–19.31 | 1094 | 18.90 | 11.24 | 11.12–12.19 | 1838 | 11.24 |
| gbp+huf | 27.09 | 27.14–27.63 | 763 | 27.09 | 17.29 | 17.25–17.54 | 1195 | 17.29 |
| gbp+rans | 50.94 | 50.85–51.12 | 406 | 50.94 | 40.98 | 40.90–41.36 | 504 | 40.98 |
| gbp+zstd3 | 57.71 | 57.93–66.22 | 358 | 57.68 | 19.59 | 19.61–22.29 | 1055 | 19.58 |
| lbp128+store | 18.94 | 18.80–19.92 | 1091 | 18.94 | 11.24 | 11.18–11.40 | 1839 | 11.24 |
| lbp128+huf | 24.55 | 24.61–24.97 | 842 | 24.55 | 16.13 | 16.13–16.51 | 1281 | 16.13 |
| lbp128+rans | 35.16 | 35.42–41.45 | 588 | 35.16 | 27.54 | 27.58–28.07 | 751 | 27.53 |
| lbp128+zstd3 | 32.34 | 32.01–33.06 | 639 | 32.35 | 15.90 | 15.89–16.21 | 1300 | 15.90 |
| gpp+store | 35.92 | 35.88–36.31 | 575 | 35.92 | 11.20 | 11.31–11.50 | 1846 | 11.19 |
| gpp+huf | 41.97 | 41.85–44.00 | 492 | 41.97 | 16.34 | 16.33–16.39 | 1265 | 16.34 |
| gpp+rans | 55.42 | 55.26–56.10 | 373 | 55.40 | 29.29 | 29.25–29.45 | 706 | 29.29 |
| gpp+zstd3 | 54.18 | 54.03–56.36 | 381 | 54.18 | 16.94 | 16.90–17.51 | 1220 | 16.94 |
| lpp128+store | 38.74 | 38.79–39.65 | 533 | 38.73 | 12.00 | 12.06–12.23 | 1722 | 12.00 |
| lpp128+huf | 44.03 | 43.85–45.08 | 469 | 44.03 | 16.46 | 16.45–16.92 | 1256 | 16.46 |
| lpp128+rans | 52.72 | 52.73–53.02 | 392 | 52.72 | 25.76 | 25.58–25.92 | 802 | 25.76 |
| lpp128+zstd3 | 49.44 | 49.79–50.63 | 418 | 49.42 | 14.35 | 14.37–14.64 | 1440 | 14.35 |
| sel+store | 117.99 | 117.89–118.31 | 175 | 118.00 | 12.18 | 12.18–12.45 | 1697 | 12.18 |
| sel+huf | 149.01 | 149.28–152.83 | 139 | 148.99 | 16.35 | 16.38–16.67 | 1264 | 16.35 |
| sel+rans | 228.50 | 226.33–230.14 | 90 | 228.51 | 26.28 | 26.03–26.59 | 786 | 26.28 |
| sel+zstd3 | 219.71 | 220.84–227.57 | 94 | 219.64 | 11.80 | 11.78–11.91 | 1751 | 11.80 |
| planes+store | 1.37 | 1.38–1.47 | 15105 | 1.37 | 0.77 | 0.77–0.79 | 26853 | 0.77 |
| planes+huf | 9.57 | 9.51–9.62 | 2159 | 9.57 | 5.80 | 5.82–5.86 | 3564 | 5.80 |
| planes+rans | 29.77 | 29.82–31.47 | 694 | 29.77 | 26.43 | 26.39–28.43 | 782 | 26.43 |
| planes+zstd3 | 26.69 | 26.53–27.61 | 774 | 26.69 | 7.20 | 7.16–7.25 | 2869 | 7.20 |
| exist+ent | 38.86 | 38.89–39.37 | 532 | 38.85 | 8.44 | 8.42–8.56 | 2449 | 8.44 |
| planes+ent | 38.59 | 38.51–41.57 | 536 | 38.59 | 26.39 | 26.49–26.84 | 783 | 26.39 |
| lpp128+ent | 58.32 | 58.37–60.17 | 354 | 58.32 | 25.62 | 25.45–26.23 | 807 | 25.62 |
| exist+zstd19 | 1677.88 | 1684.00–1724.56 | 12 | 1677.68 | 8.07 | 8.06–8.28 | 2562 | 8.06 |
| planes+zstd19 | 1650.34 | 1635.35–1746.03 | 13 | 1650.11 | 7.89 | 7.82–9.53 | 2620 | 7.88 |
| exist+prod3 | 37.64 | 37.59–39.49 | 549 | 37.64 | 6.34 | 6.33–6.36 | 3258 | 6.34 |
| planes+prod3 | 42.68 | 43.70–45.14 | 484 | 42.66 | 6.40 | 6.39–6.43 | 3231 | 6.40 |
| exist+prod19 | 1683.40 | 1700.92–1794.43 | 12 | 1683.29 | 7.50 | 7.46–7.62 | 2756 | 7.49 |
| planes+prod19 | 1715.52 | 1664.20–1741.70 | 12 | 1715.36 | 8.12 | 8.15–8.18 | 2544 | 8.12 |
| exist+prod3g | 43.49 | 45.03–48.21 | 475 | 43.49 | 6.32 | 6.30–6.82 | 3270 | 6.31 |
| planes+prod3g | 49.97 | 50.52–51.34 | 414 | 49.96 | 6.32 | 6.34–6.36 | 3270 | 6.32 |

### End-to-end, complete planner fast, real inputs with INT blocks (n=16, 17985484 B). Encode for experimental rows = production compress - production residual-leaf re-encode (23.4 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 8810204 | 160.4 | 25.93 | 26.06 |
| zstd -3 whole input (direct control) | 11323279 | 59.4 | 12.87 | - |
| exist+store | 17360900 | 137.9 | - | 20.00 |
| exist+huf | 9222158 | 146.1 | - | 25.11 |
| exist+rans | 10303403 | 171.8 | - | 49.31 |
| exist+zstd3 | 8816055 | 162.1 | - | 26.43 |
| gbp+store | 14020568 | 154.7 | - | 30.15 |
| gbp+huf | 10688342 | 162.5 | - | 36.01 |
| gbp+rans | 10629074 | 187.1 | - | 59.80 |
| gbp+zstd3 | 10560526 | 188.4 | - | 38.23 |
| lbp128+store | 9563933 | 155.0 | - | 30.17 |
| lbp128+huf | 9064036 | 160.6 | - | 35.04 |
| lbp128+rans | 9036952 | 172.4 | - | 46.45 |
| lbp128+zstd3 | 9055205 | 167.3 | - | 35.25 |
| gpp+store | 10333253 | 172.5 | - | 30.19 |
| gpp+huf | 9175814 | 178.8 | - | 35.42 |
| gpp+rans | 9139972 | 193.9 | - | 48.03 |
| gpp+zstd3 | 9172934 | 187.7 | - | 36.14 |
| lpp128+store | 9154928 | 175.3 | - | 30.80 |
| lpp128+huf | 8694476 | 180.8 | - | 35.33 |
| lpp128+rans | 8660834 | 191.4 | - | 44.58 |
| lpp128+zstd3 | 8719883 | 186.4 | - | 33.72 |
| sel+store | 9154403 | 247.2 | - | 30.90 |
| sel+huf | 8679649 | 280.8 | - | 35.20 |
| sel+rans | 8642070 | 367.5 | - | 44.97 |
| sel+zstd3 | 8639236 | 338.9 | - | 31.03 |
| planes+store | 17361126 | 138.1 | - | 19.87 |
| planes+huf | 9163423 | 146.3 | - | 24.94 |
| planes+rans | 9328907 | 167.5 | - | 46.37 |
| planes+zstd3 | 8805079 | 162.1 | - | 26.50 |
| exist+ent | 9210447 | 180.1 | - | 27.77 |
| planes+ent | 8812857 | 176.0 | - | 45.23 |
| lpp128+ent | 8655006 | 196.9 | - | 44.16 |
| exist+zstd19 | 8641618 | 1479.8 | - | 27.47 |
| planes+zstd19 | 8610866 | 1563.2 | - | 27.10 |
| exist+prod3 | 8810565 | 178.5 | - | 25.98 |
| planes+prod3 | 8797102 | 179.5 | - | 25.90 |
| exist+prod19 | 8641763 | 1505.0 | - | 27.09 |
| planes+prod19 | 8609473 | 1568.4 | - | 28.19 |
| exist+prod3g | 8810565 | 181.0 | - | 26.19 |
| planes+prod3g | 8797102 | 183.7 | - | 25.92 |

### End-to-end, complete planner hc, real inputs with INT blocks (n=13, 17941264 B). Encode for experimental rows = production compress - production residual-leaf re-encode (1901.8 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 8627589 | 2044.1 | 27.71 | 27.74 |
| zstd -3 whole input (direct control) | 11298605 | 59.3 | 12.91 | - |
| exist+store | 20669530 | 143.8 | - | 21.37 |
| exist+huf | 9276368 | 152.3 | - | 26.86 |
| exist+rans | 16082269 | 173.1 | - | 37.25 |
| exist+zstd3 | 8819416 | 170.8 | - | 28.02 |
| gbp+store | 14723970 | 162.6 | - | 33.37 |
| gbp+huf | 10769051 | 170.9 | - | 39.66 |
| gbp+rans | 10702685 | 195.2 | - | 63.62 |
| gbp+zstd3 | 10677423 | 202.8 | - | 41.77 |
| lbp128+store | 9558667 | 162.5 | - | 32.13 |
| lbp128+huf | 9057812 | 168.1 | - | 36.90 |
| lbp128+rans | 9030213 | 178.9 | - | 48.33 |
| lbp128+zstd3 | 9056701 | 177.0 | - | 37.27 |
| gpp+store | 10302510 | 180.1 | - | 32.35 |
| gpp+huf | 9127351 | 186.2 | - | 37.52 |
| gpp+rans | 9085846 | 199.6 | - | 50.33 |
| gpp+zstd3 | 9138348 | 197.9 | - | 37.85 |
| lpp128+store | 9149483 | 182.2 | - | 33.02 |
| lpp128+huf | 8688576 | 187.7 | - | 37.29 |
| lpp128+rans | 8646841 | 196.9 | - | 46.50 |
| lpp128+zstd3 | 8718093 | 194.1 | - | 35.54 |
| sel+store | 9149451 | 268.8 | - | 39.97 |
| sel+huf | 8679222 | 301.9 | - | 44.47 |
| sel+rans | 8636878 | 381.5 | - | 57.08 |
| sel+zstd3 | 8630176 | 375.9 | - | 39.72 |
| planes+store | 20669652 | 144.2 | - | 21.40 |
| planes+huf | 9167339 | 152.2 | - | 26.24 |
| planes+rans | 12945905 | 172.4 | - | 47.16 |
| planes+zstd3 | 8810214 | 170.6 | - | 27.86 |
| exist+ent | 9267268 | 182.1 | - | 29.16 |
| planes+ent | 8832072 | 181.5 | - | 47.17 |
| lpp128+ent | 8645000 | 202.6 | - | 46.33 |
| exist+zstd19 | 8627687 | 1973.5 | - | 29.28 |
| planes+zstd19 | 8597186 | 1987.4 | - | 29.08 |
| exist+prod3 | 8814644 | 183.6 | - | 29.65 |
| planes+prod3 | 8851780 | 186.8 | - | 27.09 |
| exist+prod19 | 8627752 | 2053.8 | - | 31.08 |
| planes+prod19 | 8596030 | 1973.6 | - | 29.10 |
| exist+prod3g | 8814644 | 189.5 | - | 29.57 |
| planes+prod3g | 8803560 | 194.2 | - | 26.89 |

### Peak memory, fast mode, real inputs with INT blocks: process RSS high-water above the RSS just before the operation (KiB; /proc clear_refs reset after malloc_trim; input, frame and output buffers pre-touched)

| operation | config | max over inputs | median over inputs |
|---|---|---|---|
| prod_compress | - | 6024 | 1602.0 |
| prod_decompress_api | - | 3092 | 862.0 |
| prod_decode_seq | - | 3096 | 830.0 |
| exp_residual_encode | exist+store | 4880 | 528.0 |
| exp_residual_decode | exist+store | 3860 | 700.0 |
| exp_decode_seq | exist+store | 4848 | 1044.0 |
| exp_residual_encode | exist+huf | 3704 | 384.0 |
| exp_residual_decode | exist+huf | 3860 | 700.0 |
| exp_decode_seq | exist+huf | 4848 | 1030.0 |
| exp_residual_encode | exist+rans | 5376 | 570.0 |
| exp_residual_decode | exist+rans | 3920 | 762.0 |
| exp_decode_seq | exist+rans | 4908 | 1186.0 |
| exp_residual_encode | exist+zstd3 | 4744 | 1018.0 |
| exp_residual_decode | exist+zstd3 | 3936 | 742.0 |
| exp_decode_seq | exist+zstd3 | 4908 | 1042.0 |
| exp_residual_encode | gbp+store | 10040 | 2806.0 |
| exp_residual_decode | gbp+store | 10072 | 2910.0 |
| exp_decode_seq | gbp+store | 7784 | 2858.0 |
| exp_residual_encode | gbp+huf | 9704 | 2744.0 |
| exp_residual_decode | gbp+huf | 10072 | 2910.0 |
| exp_decode_seq | gbp+huf | 7848 | 2866.0 |
| exp_residual_encode | gbp+rans | 7332 | 2532.0 |
| exp_residual_decode | gbp+rans | 10132 | 2972.0 |
| exp_decode_seq | gbp+rans | 7844 | 2928.0 |
| exp_residual_encode | gbp+zstd3 | 7856 | 3076.0 |
| exp_residual_decode | gbp+zstd3 | 10164 | 2970.0 |
| exp_decode_seq | gbp+zstd3 | 7860 | 2878.0 |
| exp_residual_encode | lbp128+store | 7684 | 2626.0 |
| exp_residual_decode | lbp128+store | 9056 | 2868.0 |
| exp_decode_seq | lbp128+store | 7428 | 2930.0 |
| exp_residual_encode | lbp128+huf | 7640 | 2608.0 |
| exp_residual_decode | lbp128+huf | 9056 | 2868.0 |
| exp_decode_seq | lbp128+huf | 7428 | 2900.0 |
| exp_residual_encode | lbp128+rans | 7104 | 2554.0 |
| exp_residual_decode | lbp128+rans | 9116 | 2930.0 |
| exp_decode_seq | lbp128+rans | 7544 | 2930.0 |
| exp_residual_encode | lbp128+zstd3 | 7176 | 3272.0 |
| exp_residual_decode | lbp128+zstd3 | 9136 | 2948.0 |
| exp_decode_seq | lbp128+zstd3 | 8452 | 3010.0 |
| exp_residual_encode | gpp+store | 7732 | 2564.0 |
| exp_residual_decode | gpp+store | 9196 | 2872.0 |
| exp_decode_seq | gpp+store | 7468 | 2894.0 |
| exp_residual_encode | gpp+huf | 7628 | 2548.0 |
| exp_residual_decode | gpp+huf | 9196 | 2872.0 |
| exp_decode_seq | gpp+huf | 7468 | 2894.0 |
| exp_residual_encode | gpp+rans | 6884 | 2508.0 |
| exp_residual_decode | gpp+rans | 9364 | 2928.0 |
| exp_decode_seq | gpp+rans | 7572 | 2924.0 |
| exp_residual_encode | gpp+zstd3 | 8356 | 3134.0 |
| exp_residual_decode | gpp+zstd3 | 9284 | 2898.0 |
| exp_decode_seq | gpp+zstd3 | 7540 | 2902.0 |
| exp_residual_encode | lpp128+store | 7372 | 2590.0 |
| exp_residual_decode | lpp128+store | 8428 | 2856.0 |
| exp_decode_seq | lpp128+store | 7252 | 2824.0 |
| exp_residual_encode | lpp128+huf | 7332 | 2574.0 |
| exp_residual_decode | lpp128+huf | 8428 | 2856.0 |
| exp_decode_seq | lpp128+huf | 7252 | 2854.0 |
| exp_residual_encode | lpp128+rans | 6948 | 2362.0 |
| exp_residual_decode | lpp128+rans | 8584 | 2902.0 |
| exp_decode_seq | lpp128+rans | 7352 | 2948.0 |
| exp_residual_encode | lpp128+zstd3 | 7136 | 2818.0 |
| exp_residual_decode | lpp128+zstd3 | 8516 | 2934.0 |
| exp_decode_seq | lpp128+zstd3 | 7316 | 2872.0 |
| exp_residual_encode | sel+store | 10320 | 3262.0 |
| exp_residual_decode | sel+store | 6812 | 2464.0 |
| exp_decode_seq | sel+store | 7340 | 2514.0 |
| exp_residual_encode | sel+huf | 8280 | 3078.0 |
| exp_residual_decode | sel+huf | 7260 | 2748.0 |
| exp_decode_seq | sel+huf | 7104 | 2798.0 |
| exp_residual_encode | sel+rans | 9248 | 2876.0 |
| exp_residual_decode | sel+rans | 7360 | 2844.0 |
| exp_decode_seq | sel+rans | 7296 | 2862.0 |
| exp_residual_encode | sel+zstd3 | 11856 | 3492.0 |
| exp_residual_decode | sel+zstd3 | 7344 | 1210.0 |
| exp_decode_seq | sel+zstd3 | 7296 | 1264.0 |
| exp_residual_encode | planes+store | 3948 | 528.0 |
| exp_residual_decode | planes+store | 2928 | 696.0 |
| exp_decode_seq | planes+store | 4428 | 970.0 |
| exp_residual_encode | planes+huf | 3044 | 386.0 |
| exp_residual_decode | planes+huf | 2928 | 696.0 |
| exp_decode_seq | planes+huf | 4428 | 1064.0 |
| exp_residual_encode | planes+rans | 4448 | 570.0 |
| exp_residual_decode | planes+rans | 2988 | 758.0 |
| exp_decode_seq | planes+rans | 4488 | 1148.0 |
| exp_residual_encode | planes+zstd3 | 4124 | 948.0 |
| exp_residual_decode | planes+zstd3 | 3008 | 734.0 |
| exp_decode_seq | planes+zstd3 | 3524 | 1036.0 |
| exp_residual_encode | exist+ent | 5696 | 764.0 |
| exp_residual_decode | exist+ent | 3860 | 730.0 |
| exp_decode_seq | exist+ent | 4848 | 1122.0 |
| exp_residual_encode | planes+ent | 4776 | 762.0 |
| exp_residual_decode | planes+ent | 2988 | 758.0 |
| exp_decode_seq | planes+ent | 4488 | 1148.0 |
| exp_residual_encode | lpp128+ent | 7124 | 2472.0 |
| exp_residual_decode | lpp128+ent | 8584 | 2870.0 |
| exp_decode_seq | lpp128+ent | 7352 | 2888.0 |
| exp_residual_encode | exist+zstd19 | 54872 | 5126.0 |
| exp_residual_decode | exist+zstd19 | 3928 | 738.0 |
| exp_decode_seq | exist+zstd19 | 4900 | 1080.0 |
| exp_residual_encode | planes+zstd19 | 12228 | 4998.0 |
| exp_residual_decode | planes+zstd19 | 2992 | 732.0 |
| exp_decode_seq | planes+zstd19 | 4476 | 1118.0 |
| exp_residual_encode | exist+prod3 | 6784 | 1816.0 |
| exp_residual_decode | exist+prod3 | 2004 | 480.0 |
| exp_decode_seq | exist+prod3 | 3092 | 1100.0 |
| exp_residual_encode | planes+prod3 | 6940 | 1738.0 |
| exp_residual_decode | planes+prod3 | 2008 | 472.0 |
| exp_decode_seq | planes+prod3 | 4008 | 992.0 |
| exp_residual_encode | exist+prod19 | 56668 | 5574.0 |
| exp_residual_decode | exist+prod19 | 2000 | 306.0 |
| exp_decode_seq | exist+prod19 | 3088 | 1066.0 |
| exp_residual_encode | planes+prod19 | 16144 | 5448.0 |
| exp_residual_decode | planes+prod19 | 2056 | 298.0 |
| exp_decode_seq | planes+prod19 | 4056 | 1088.0 |
| exp_residual_encode | exist+prod3g | 6784 | 1818.0 |
| exp_residual_decode | exist+prod3g | 2004 | 480.0 |
| exp_decode_seq | exist+prod3g | 3092 | 1044.0 |
| exp_residual_encode | planes+prod3g | 6944 | 1740.0 |
| exp_residual_decode | planes+prod3g | 2008 | 472.0 |
| exp_decode_seq | planes+prod3g | 4008 | 1050.0 |
