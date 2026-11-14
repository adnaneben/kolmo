
### Regime A (typed, kg_encode_num at zstd level 3), real inputs: total stored bytes (n=17; input 30812992 B; production typed payload 4134947 B; zstd -3 whole input 5360307 B)

| config | bytes | vs production payload | vs exist, same backend | inputs smaller/larger/equal vs production |
|---|---|---|---|---|
| exist+store | 19338921 | +367.69% | +0.00% | 0/17/0 |
| exist+huf | 5466949 | +32.21% | +0.00% | 0/17/0 |
| exist+rans | 5821617 | +40.79% | +0.00% | 3/14/0 |
| exist+zstd3 | 4145271 | +0.25% | +0.00% | 0/17/0 |
| gbp+store | 12547392 | +203.45% | -35.12% | 0/17/0 |
| gbp+huf | 5719911 | +38.33% | +4.63% | 0/17/0 |
| gbp+rans | 5039270 | +21.87% | -13.44% | 2/15/0 |
| gbp+zstd3 | 4963928 | +20.05% | +19.75% | 1/16/0 |
| lbp128+store | 7259628 | +75.57% | -62.46% | 0/17/0 |
| lbp128+huf | 4719944 | +14.15% | -13.66% | 0/17/0 |
| lbp128+rans | 4527716 | +9.50% | -22.23% | 2/15/0 |
| lbp128+zstd3 | 4580185 | +10.77% | +10.49% | 2/15/0 |
| gpp+store | 4658825 | +12.67% | -75.91% | 2/15/0 |
| gpp+huf | 4212175 | +1.87% | -22.95% | 8/9/0 |
| gpp+rans | 4196152 | +1.48% | -27.92% | 7/10/0 |
| gpp+zstd3 | 4178482 | +1.05% | +0.80% | 9/8/0 |
| lpp128+store | 4578658 | +10.73% | -76.32% | 0/17/0 |
| lpp128+huf | 4307911 | +4.18% | -21.20% | 5/12/0 |
| lpp128+rans | 4304868 | +4.11% | -26.05% | 5/12/0 |
| lpp128+zstd3 | 4185233 | +1.22% | +0.96% | 10/7/0 |
| sel+store | 4538549 | +9.76% | -76.53% | 2/15/0 |
| sel+huf | 4154425 | +0.47% | -24.01% | 10/7/0 |
| sel+rans | 4154201 | +0.47% | -28.64% | 11/6/0 |
| sel+zstd3 | 4085265 | -1.20% | -1.45% | 11/6/0 |
| planes+store | 19339327 | +367.70% | +0.00% | 0/17/0 |
| planes+huf | 5333645 | +28.99% | -2.44% | 1/16/0 |
| planes+rans | 7339389 | +77.50% | +26.07% | 3/14/0 |
| planes+zstd3 | 4135027 | +0.00% | -0.25% | 9/8/0 |
| exist+ent | 4710997 | +13.93% | +0.00% | 3/14/0 |
| planes+ent | 4224525 | +2.17% | -10.33% | 7/10/0 |
| lpp128+ent | 4294088 | +3.85% | -8.85% | 6/11/0 |
| exist+zstd19 | 4104100 | -0.75% | +0.00% | 12/5/0 |
| planes+zstd19 | 4085812 | -1.19% | -0.45% | 12/5/0 |
| exist+prod3 | 4135167 | +0.01% | +0.00% | 0/17/0 |
| planes+prod3 | 4251632 | +2.82% | +2.82% | 8/9/0 |
| exist+prod19 | 4101601 | -0.81% | +0.00% | 12/5/0 |
| planes+prod19 | 4079214 | -1.35% | -0.55% | 12/5/0 |
| exist+prod3g | 4135167 | +0.01% | +0.00% | 0/17/0 |
| planes+prod3g | 4116431 | -0.45% | -0.45% | 10/7/0 |

### Regime A per family: change vs production typed payload (fixed configurations)

| family | n | production B | exist+huf | exist+rans | lbp128+rans | gpp+rans | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+rans | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|---|---|
| C1_dense_control | 4 | 2140271 | +13.46% | +52.43% | +4.63% | +1.14% | +1.32% | +0.98% | +1.90% | +0.59% | exist+zstd19 |
| C2_single_byte_control | 1 | 126395 | +0.01% | -0.20% | +7.39% | +7.25% | +3.26% | +2.96% | +3.20% | -0.20% | exist+rans |
| F1_backblaze | 6 | 1298720 | +25.77% | +26.91% | +12.44% | +2.29% | +3.34% | +3.63% | +0.94% | +1.91% | planes+zstd19 |
| F2_owid_covid | 6 | 569561 | +124.51% | +37.79% | +21.56% | -0.35% | +17.06% | +17.23% | -1.15% | -3.14% | planes+zstd3 |

### Regime B (complete planner, fast), real inputs: complete-input stored bytes (n=17, 11 with INT blocks; input 30812992 B; production frame 4241868 B; zstd -3 5360307 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 11247436 | +165.15% | +0.00% | 0/11/6 |
| exist+huf | 4868589 | +14.77% | +0.00% | 0/11/6 |
| exist+rans | 5239440 | +23.52% | +0.00% | 1/10/6 |
| exist+zstd3 | 4245266 | +0.08% | +0.00% | 0/11/6 |
| gbp+store | 7336691 | +72.96% | -34.77% | 0/11/6 |
| gbp+huf | 5124602 | +20.81% | +5.26% | 0/11/6 |
| gbp+rans | 4971662 | +17.20% | -5.11% | 0/11/6 |
| gbp+zstd3 | 4962013 | +16.98% | +16.88% | 0/11/6 |
| lbp128+store | 5417648 | +27.72% | -51.83% | 0/11/6 |
| lbp128+huf | 4586618 | +8.13% | -5.79% | 0/11/6 |
| lbp128+rans | 4544733 | +7.14% | -13.26% | 0/11/6 |
| lbp128+zstd3 | 4589183 | +8.19% | +8.10% | 0/11/6 |
| gpp+store | 4635070 | +9.27% | -58.79% | 0/11/6 |
| gpp+huf | 4318269 | +1.80% | -11.30% | 4/7/6 |
| gpp+rans | 4311022 | +1.63% | -17.72% | 4/7/6 |
| gpp+zstd3 | 4314956 | +1.72% | +1.64% | 4/7/6 |
| lpp128+store | 4526661 | +6.71% | -59.75% | 0/11/6 |
| lpp128+huf | 4333676 | +2.16% | -10.99% | 4/7/6 |
| lpp128+rans | 4332222 | +2.13% | -17.32% | 4/7/6 |
| lpp128+zstd3 | 4311708 | +1.65% | +1.57% | 6/5/6 |
| sel+store | 4513136 | +6.40% | -59.87% | 0/11/6 |
| sel+huf | 4257340 | +0.36% | -12.55% | 6/5/6 |
| sel+rans | 4263997 | +0.52% | -18.62% | 7/4/6 |
| sel+zstd3 | 4223154 | -0.44% | -0.52% | 6/5/6 |
| planes+store | 11247800 | +165.16% | +0.00% | 0/11/6 |
| planes+huf | 4790068 | +12.92% | -1.61% | 1/10/6 |
| planes+rans | 5613846 | +32.34% | +7.15% | 3/8/6 |
| planes+zstd3 | 4222112 | -0.47% | -0.55% | 6/5/6 |
| exist+ent | 4718796 | +11.24% | +0.00% | 1/10/6 |
| planes+ent | 4255453 | +0.32% | -9.82% | 6/5/6 |
| lpp128+ent | 4322837 | +1.91% | -8.39% | 4/7/6 |
| exist+zstd19 | 4206655 | -0.83% | +0.00% | 9/2/6 |
| planes+zstd19 | 4190086 | -1.22% | -0.39% | 9/2/6 |
| exist+prod3 | 4242127 | +0.01% | +0.00% | 0/11/6 |
| planes+prod3 | 4311650 | +1.65% | +1.64% | 7/4/6 |
| exist+prod19 | 4206740 | -0.83% | +0.00% | 9/2/6 |
| planes+prod19 | 4186139 | -1.31% | -0.49% | 9/2/6 |
| exist+prod3g | 4242127 | +0.01% | +0.00% | 0/11/6 |
| planes+prod3g | 4213446 | -0.67% | -0.68% | 7/4/6 |

### Regime B (complete planner, hc), real inputs: complete-input stored bytes (n=17, 10 with INT blocks; input 30812992 B; production frame 4076351 B; zstd -3 5360307 B)

| config | bytes | vs production frame | vs exist, same backend | inputs smaller/larger/equal |
|---|---|---|---|---|
| exist+store | 12835119 | +214.87% | +0.00% | 0/10/7 |
| exist+huf | 4861554 | +19.26% | +0.00% | 0/10/7 |
| exist+rans | 5504298 | +35.03% | +0.00% | 1/9/7 |
| exist+zstd3 | 4097508 | +0.52% | +0.00% | 3/7/7 |
| gbp+store | 9284677 | +127.77% | -27.66% | 0/10/7 |
| gbp+huf | 5155877 | +26.48% | +6.05% | 0/10/7 |
| gbp+rans | 4759517 | +16.76% | -13.53% | 0/10/7 |
| gbp+zstd3 | 4715240 | +15.67% | +15.08% | 0/10/7 |
| lbp128+store | 5826821 | +42.94% | -54.60% | 0/10/7 |
| lbp128+huf | 4485493 | +10.04% | -7.74% | 0/10/7 |
| lbp128+rans | 4407706 | +8.13% | -19.92% | 0/10/7 |
| lbp128+zstd3 | 4472470 | +9.72% | +9.15% | 0/10/7 |
| gpp+store | 4458776 | +9.38% | -65.26% | 0/10/7 |
| gpp+huf | 4149825 | +1.80% | -14.64% | 4/6/7 |
| gpp+rans | 4132978 | +1.39% | -24.91% | 4/6/7 |
| gpp+zstd3 | 4144589 | +1.67% | +1.15% | 4/6/7 |
| lpp128+store | 4432600 | +8.74% | -65.47% | 0/10/7 |
| lpp128+huf | 4240914 | +4.04% | -12.77% | 0/10/7 |
| lpp128+rans | 4233536 | +3.86% | -23.09% | 0/10/7 |
| lpp128+zstd3 | 4158957 | +2.03% | +1.50% | 3/7/7 |
| sel+store | 4408515 | +8.15% | -65.65% | 0/10/7 |
| sel+huf | 4115544 | +0.96% | -15.35% | 4/6/7 |
| sel+rans | 4116334 | +0.98% | -25.22% | 5/5/7 |
| sel+zstd3 | 4068511 | -0.19% | -0.71% | 4/6/7 |
| planes+store | 12835322 | +214.87% | +0.00% | 0/10/7 |
| planes+huf | 4828588 | +18.45% | -0.68% | 0/10/7 |
| planes+rans | 5411459 | +32.75% | -1.69% | 1/9/7 |
| planes+zstd3 | 4087180 | +0.27% | -0.25% | 4/6/7 |
| exist+ent | 4449644 | +9.16% | +0.00% | 1/9/7 |
| planes+ent | 4141248 | +1.59% | -6.93% | 2/8/7 |
| lpp128+ent | 4229766 | +3.76% | -4.94% | 0/10/7 |
| exist+zstd19 | 4076445 | +0.00% | +0.00% | 0/10/7 |
| planes+zstd19 | 4059400 | -0.42% | -0.42% | 6/4/7 |
| exist+prod3 | 4087369 | +0.27% | +0.00% | 3/7/7 |
| planes+prod3 | 4202750 | +3.10% | +2.82% | 3/7/7 |
| exist+prod19 | 4076481 | +0.00% | +0.00% | 0/10/7 |
| planes+prod19 | 4057006 | -0.47% | -0.48% | 8/2/7 |
| exist+prod3g | 4087369 | +0.27% | +0.00% | 3/7/7 |
| planes+prod3g | 4069680 | -0.16% | -0.43% | 4/6/7 |

### Regime B per input: production planner selection

| input | mode | origin | bytes | frame | zstd -3 | INT blocks | prod residual codec | INT shape | smallest fixed config |
|---|---|---|---|---|---|---|---|---|---|
| backblaze_capacity_bytes.npy | fast | real | 1884904 | 6002 | 9099 | 0/2 | - | - | - |
| backblaze_capacity_bytes.npy | hc | real | 1884904 | 3654 | 9099 | 0/1 | - | - | - |
| backblaze_smart_194_raw.npy | fast | real | 1881608 | 126498 | 130060 | 2/2 | huf | w8o1ow1 | exist+rans |
| backblaze_smart_194_raw.npy | hc | real | 1881608 | 126446 | 130060 | 1/1 | huf | w8o1ow1 | exist+rans |
| backblaze_smart_197_raw.npy | fast | real | 1818848 | 8585 | 8275 | 0/2 | - | - | - |
| backblaze_smart_197_raw.npy | hc | real | 1818848 | 8905 | 8275 | 0/1 | - | - | - |
| backblaze_smart_241_raw.npy | fast | real | 996616 | 483121 | 571914 | 1/1 | zstd | w8o1ow8 | planes+zstd19 |
| backblaze_smart_241_raw.npy | hc | real | 996616 | 476880 | 571914 | 1/1 | zstd | w8o1ow8 | planes+zstd19 |
| backblaze_smart_242_raw.npy | fast | real | 996616 | 581859 | 702750 | 1/1 | zstd | w8o1ow8 | planes+zstd19 |
| backblaze_smart_242_raw.npy | hc | real | 996616 | 576451 | 702750 | 1/1 | zstd | w8o1ow8 | planes+zstd19 |
| backblaze_smart_5_raw.npy | fast | real | 1867000 | 19508 | 19310 | 0/2 | - | - | - |
| backblaze_smart_5_raw.npy | hc | real | 1867000 | 18961 | 19310 | 0/1 | - | - | - |
| backblaze_smart_9_raw.npy | fast | real | 1881608 | 188199 | 288876 | 2/2 | zstd | w8o1ow4 | gpp+huf |
| backblaze_smart_9_raw.npy | hc | real | 1881608 | 183960 | 288876 | 1/1 | zstd | w8o1ow4 | gpp+rans |
| eeg_S005R01.npy | fast | real | 1249408 | 619467 | 729823 | 2/2 | zstd | w2o1ow2 | lpp128+rans |
| eeg_S005R01.npy | hc | real | 1249408 | 578478 | 729823 | 0/1 | - | - | - |
| eeg_S006R01.npy | fast | real | 1249408 | 473918 | 628738 | 2/2 | zstd | w2o1ow1,w2o1ow2 | exist+zstd19 |
| eeg_S006R01.npy | hc | real | 1249408 | 469645 | 628738 | 1/1 | zstd | w2o1ow2 | planes+zstd19 |
| eeg_S007R01.npy | fast | real | 1249408 | 601819 | 725685 | 2/2 | zstd | w2o1ow2 | planes+zstd19 |
| eeg_S007R01.npy | hc | real | 1249408 | 594183 | 725685 | 1/1 | zstd | w2o1ow2 | planes+zstd19 |
| eeg_S008R01.npy | fast | real | 1249408 | 454126 | 649087 | 2/2 | zstd | w2o1ow1,w2o1ow2 | exist+zstd19 |
| eeg_S008R01.npy | hc | real | 1249408 | 449896 | 649087 | 1/1 | zstd | w2o1ow2 | exist+zstd19 |
| owid_new_cases.npy | fast | real | 3281400 | 109100 | 113373 | 0/4 | - | - | - |
| owid_new_cases.npy | hc | real | 3281400 | 85676 | 113373 | 1/1 | zstd | w8o0ow4 | gpp+rans |
| owid_new_deaths.npy | fast | real | 3284992 | 55236 | 56081 | 0/4 | - | - | - |
| owid_new_deaths.npy | hc | real | 3284992 | 53827 | 56081 | 0/1 | - | - | - |
| owid_people_vaccinated.npy | fast | real | 649184 | 148366 | 223460 | 1/1 | zstd | w8o2ow8 | planes+zstd19 |
| owid_people_vaccinated.npy | hc | real | 649184 | 142958 | 223460 | 0/1 | - | - | - |
| owid_total_cases.npy | fast | real | 3294560 | 82400 | 134270 | 4/4 | zstd | w8o1ow4 | gpp+zstd3 |
| owid_total_cases.npy | hc | real | 3294560 | 89345 | 134270 | 1/1 | zstd | w8o1ow4 | gpp+rans |
| owid_total_deaths.npy | fast | real | 3294560 | 101606 | 101814 | 0/4 | - | - | - |
| owid_total_deaths.npy | hc | real | 3294560 | 42346 | 101814 | 1/1 | zstd | w8o1ow4 | gpp+zstd3 |
| owid_total_vaccinations.npy | fast | real | 683464 | 182058 | 267692 | 1/1 | zstd | w8o2ow8 | planes+zstd19 |
| owid_total_vaccinations.npy | hc | real | 683464 | 174740 | 267692 | 0/1 | - | - | - |

### Regime B per input with INT blocks: complete-input change vs production frame

| input | mode | origin | production frame | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| backblaze_smart_194_raw.npy | fast | real | 126498 | +2.57% | +5.89% | +5.95% | +3.12% | +3.31% | +3.10% | +0.01% | -0.75% | +1.91% |
| backblaze_smart_194_raw.npy | hc | real | 126446 | +8.06% | +6.00% | +6.30% | +3.25% | +2.96% | +3.20% | +0.01% | -0.20% | +3.20% |
| backblaze_smart_241_raw.npy | fast | real | 483121 | +0.00% | +7.73% | +8.33% | +6.95% | +6.79% | +7.61% | +2.75% | +6.79% | +0.00% |
| backblaze_smart_241_raw.npy | hc | real | 476880 | +1.31% | +9.14% | +9.74% | +8.35% | +8.18% | +9.02% | +4.09% | +8.18% | +1.31% |
| backblaze_smart_242_raw.npy | fast | real | 581859 | +0.00% | +0.05% | +0.58% | -0.14% | -0.20% | -0.03% | -0.14% | -0.20% | -0.03% |
| backblaze_smart_242_raw.npy | hc | real | 576451 | +0.94% | +0.99% | +1.52% | +0.80% | +0.74% | +0.91% | +0.80% | +0.74% | +0.91% |
| backblaze_smart_9_raw.npy | fast | real | 188199 | +0.01% | -4.53% | -4.34% | +1.39% | +2.08% | -1.68% | -4.53% | -4.11% | -4.34% |
| backblaze_smart_9_raw.npy | hc | real | 183960 | +2.07% | -2.28% | -2.25% | +3.67% | +3.86% | +0.70% | -2.28% | -2.31% | -2.25% |
| eeg_S005R01.npy | fast | real | 619467 | +0.00% | -0.75% | -0.14% | -1.74% | -1.92% | -0.41% | -1.74% | -1.92% | -0.56% |
| eeg_S006R01.npy | fast | real | 473918 | +0.00% | +2.34% | +2.43% | +3.19% | +2.94% | +4.01% | +2.08% | +1.17% | +0.00% |
| eeg_S006R01.npy | hc | real | 469645 | +0.90% | +3.26% | +3.33% | +4.08% | +3.69% | +4.85% | +3.26% | +2.41% | +0.90% |
| eeg_S007R01.npy | fast | real | 601819 | +0.00% | -0.12% | -0.87% | -0.36% | -0.67% | -0.16% | -0.36% | -0.72% | -0.87% |
| eeg_S007R01.npy | hc | real | 594183 | +0.45% | +1.15% | +1.35% | +0.89% | +0.47% | +1.21% | +0.89% | +0.47% | +0.45% |
| eeg_S008R01.npy | fast | real | 454126 | +0.00% | +3.74% | +2.57% | +3.99% | +3.74% | +3.68% | +3.23% | +2.52% | +0.00% |
| eeg_S008R01.npy | hc | real | 449896 | +0.93% | +4.69% | +3.51% | +4.91% | +4.51% | +4.62% | +4.69% | +3.99% | +0.93% |
| owid_new_cases.npy | hc | real | 85676 | -7.33% | -10.71% | -14.25% | +26.67% | +26.68% | -11.04% | -10.71% | -14.74% | -14.25% |
| owid_people_vaccinated.npy | fast | real | 148366 | +0.01% | +6.84% | +6.37% | +0.87% | +1.23% | -0.18% | +0.87% | +1.23% | -0.18% |
| owid_total_cases.npy | fast | real | 82400 | +0.04% | -1.17% | -4.68% | +37.98% | +41.20% | -0.56% | -1.17% | -3.98% | -4.68% |
| owid_total_cases.npy | hc | real | 89345 | -7.71% | -9.48% | -12.95% | +26.44% | +26.30% | -9.78% | -9.48% | -13.45% | -12.95% |
| owid_total_deaths.npy | hc | real | 42346 | -5.66% | -11.36% | -15.35% | +38.27% | +38.37% | -8.26% | -11.36% | -14.73% | -15.35% |
| owid_total_vaccinations.npy | fast | real | 182058 | +0.00% | +4.34% | +4.08% | -0.25% | -0.04% | +0.57% | -0.25% | -0.04% | +0.00% |

### Regime A per input: typed payload change vs production typed payload

| input | origin | production payload | INT shape | exist+zstd3 | gpp+huf | gpp+zstd3 | lpp128+huf | lpp128+rans | lpp128+zstd3 | sel+huf | sel+rans | sel+zstd3 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| backblaze_capacity_bytes.npy | real | 21935 | o1ow8 | +0.04% | +44.57% | -78.83% | +68.00% | +77.06% | -67.93% | +44.57% | +44.59% | -78.83% |
| backblaze_smart_194_raw.npy | real | 126395 | o1ow1 | +8.07% | +6.00% | +6.31% | +3.26% | +2.96% | +3.20% | +0.01% | -0.20% | +3.20% |
| backblaze_smart_197_raw.npy | real | 6940 | o0ow4 | +0.10% | -41.67% | -40.40% | -37.71% | -6.02% | -34.97% | -41.67% | -35.45% | -40.40% |
| backblaze_smart_241_raw.npy | real | 483069 | o1ow8 | +0.00% | +7.73% | +8.33% | +6.95% | +6.79% | +7.61% | +2.75% | +6.79% | +0.00% |
| backblaze_smart_242_raw.npy | real | 581807 | o1ow8 | +0.00% | +0.05% | +0.58% | -0.14% | -0.20% | -0.03% | -0.14% | -0.20% | -0.03% |
| backblaze_smart_5_raw.npy | real | 17269 | o0ow2 | +0.05% | -37.71% | -35.96% | -27.37% | -24.69% | -26.45% | -37.71% | -35.13% | -35.96% |
| backblaze_smart_9_raw.npy | real | 187700 | o1ow4 | +0.00% | -4.26% | -4.22% | +1.58% | +1.77% | -1.34% | -4.26% | -4.28% | -4.22% |
| eeg_S005R01.npy | real | 615667 | o1ow2 | +0.00% | -0.15% | -0.02% | -1.17% | -1.39% | +0.15% | -1.17% | -1.39% | -0.02% |
| eeg_S006R01.npy | real | 473790 | o1ow2 | +0.00% | +2.34% | +2.42% | +3.16% | +2.78% | +3.92% | +2.34% | +1.50% | +0.00% |
| eeg_S007R01.npy | real | 596804 | o1ow2 | +0.00% | +0.70% | +0.89% | +0.44% | +0.02% | +0.75% | +0.44% | +0.02% | +0.00% |
| eeg_S008R01.npy | real | 454010 | o1ow2 | +0.00% | +3.73% | +2.56% | +3.95% | +3.55% | +3.66% | +3.73% | +3.04% | +0.00% |
| owid_new_cases.npy | real | 79339 | o0ow4 | +0.01% | -3.64% | -7.47% | +36.72% | +36.74% | -4.00% | -3.64% | -7.99% | -7.47% |
| owid_new_deaths.npy | real | 37610 | o0ow4 | +0.02% | -7.54% | -11.28% | +48.08% | +48.37% | -3.21% | -7.54% | -11.45% | -11.28% |
| owid_people_vaccinated.npy | real | 148314 | o2ow8 | +0.01% | +6.84% | +6.37% | +0.87% | +1.23% | -0.18% | +0.87% | +1.23% | -0.18% |
| owid_total_cases.npy | real | 82401 | o1ow4 | +0.01% | -1.91% | -5.67% | +37.03% | +36.88% | -2.24% | -1.91% | -6.22% | -5.67% |
| owid_total_deaths.npy | real | 39891 | o1ow4 | +0.02% | -6.03% | -10.27% | +46.65% | +46.76% | -2.74% | -6.03% | -9.61% | -10.27% |
| owid_total_vaccinations.npy | real | 182006 | o2ow8 | +0.00% | +4.34% | +4.08% | -0.25% | -0.04% | +0.57% | -0.25% | -0.04% | +0.00% |

### Native kernel timing, regime A typed, real inputs (n=17, 19336690 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 1.10 | 1.11–1.14 | 17499 | 1.11 | 0.86 | 0.86–0.89 | 22556 | 0.86 |
| exist+huf | 14.20 | 14.22–14.24 | 1362 | 14.20 | 7.48 | 7.50–7.55 | 2585 | 7.48 |
| exist+rans | 41.90 | 42.05–42.13 | 462 | 41.89 | 29.84 | 30.08–30.33 | 648 | 29.84 |
| exist+zstd3 | 18.83 | 18.98–19.45 | 1027 | 18.83 | 6.16 | 6.19–6.24 | 3141 | 6.15 |
| gbp+store | 18.53 | 18.66–19.03 | 1043 | 18.53 | 13.88 | 13.86–14.67 | 1393 | 13.88 |
| gbp+huf | 29.41 | 29.50–29.82 | 658 | 29.41 | 19.50 | 19.51–19.57 | 991 | 19.50 |
| gbp+rans | 51.09 | 51.31–51.44 | 378 | 51.09 | 37.37 | 37.48–38.48 | 517 | 37.37 |
| gbp+zstd3 | 40.39 | 40.01–41.67 | 479 | 40.40 | 20.20 | 19.77–20.32 | 957 | 20.20 |
| lbp128+store | 17.69 | 17.70–17.80 | 1093 | 17.69 | 12.66 | 12.69–12.71 | 1527 | 12.66 |
| lbp128+huf | 23.86 | 23.89–24.81 | 811 | 23.86 | 16.52 | 16.54–16.56 | 1170 | 16.52 |
| lbp128+rans | 36.84 | 36.83–37.12 | 525 | 36.85 | 27.26 | 27.29–27.54 | 709 | 27.26 |
| lbp128+zstd3 | 34.23 | 34.30–36.06 | 565 | 34.23 | 17.05 | 17.07–17.17 | 1134 | 17.05 |
| gpp+store | 25.81 | 25.78–25.89 | 749 | 25.81 | 10.98 | 11.02–11.22 | 1762 | 10.97 |
| gpp+huf | 29.58 | 29.58–29.70 | 654 | 29.58 | 13.88 | 13.88–13.92 | 1393 | 13.88 |
| gpp+rans | 38.89 | 38.94–39.21 | 497 | 38.89 | 20.50 | 20.49–20.56 | 943 | 20.50 |
| gpp+zstd3 | 34.70 | 34.88–35.45 | 557 | 34.70 | 13.19 | 13.22–13.27 | 1466 | 13.19 |
| lpp128+store | 28.17 | 28.28–28.45 | 686 | 28.17 | 11.00 | 11.02–11.08 | 1758 | 11.00 |
| lpp128+huf | 32.01 | 32.22–32.36 | 604 | 32.00 | 14.03 | 14.05–14.11 | 1378 | 14.03 |
| lpp128+rans | 40.27 | 40.30–40.56 | 480 | 40.27 | 20.25 | 20.27–20.30 | 955 | 20.25 |
| lpp128+zstd3 | 35.77 | 35.57–38.51 | 541 | 35.77 | 12.69 | 12.69–12.73 | 1524 | 12.69 |
| sel+store | 93.05 | 93.79–95.58 | 208 | 93.05 | 11.03 | 11.09–11.20 | 1753 | 11.02 |
| sel+huf | 129.79 | 130.53–131.12 | 149 | 129.80 | 13.18 | 13.18–13.36 | 1468 | 13.16 |
| sel+rans | 210.43 | 210.96–212.89 | 92 | 210.41 | 21.23 | 20.47–24.44 | 911 | 21.22 |
| sel+zstd3 | 161.86 | 163.23–166.27 | 119 | 161.85 | 9.73 | 9.74–9.84 | 1986 | 9.73 |
| planes+store | 1.25 | 1.25–1.26 | 15526 | 1.25 | 0.86 | 0.86–0.87 | 22378 | 0.86 |
| planes+huf | 14.11 | 14.13–14.25 | 1371 | 14.11 | 7.13 | 7.14–7.19 | 2711 | 7.13 |
| planes+rans | 38.65 | 38.75–38.87 | 500 | 38.66 | 27.59 | 27.61–27.70 | 701 | 27.58 |
| planes+zstd3 | 19.11 | 19.56–20.91 | 1012 | 19.11 | 6.52 | 6.53–6.55 | 2966 | 6.52 |
| exist+ent | 55.76 | 55.81–56.19 | 347 | 55.74 | 20.25 | 20.36–20.64 | 955 | 20.25 |
| planes+ent | 52.15 | 52.18–52.40 | 371 | 52.16 | 27.01 | 27.12–27.24 | 716 | 27.01 |
| lpp128+ent | 44.26 | 44.39–44.80 | 437 | 44.26 | 19.47 | 19.46–19.51 | 993 | 19.47 |
| exist+zstd19 | 1426.52 | 1429.69–1448.33 | 14 | 1426.40 | 6.02 | 6.02–6.32 | 3210 | 5.99 |
| planes+zstd19 | 1523.53 | 1523.21–1534.27 | 13 | 1523.41 | 6.36 | 6.42–6.94 | 3039 | 6.33 |
| exist+prod3 | 34.30 | 34.67–35.78 | 564 | 34.30 | 5.55 | 5.56–5.59 | 3482 | 5.55 |
| planes+prod3 | 40.67 | 41.42–43.66 | 475 | 40.67 | 6.56 | 6.55–6.58 | 2948 | 6.56 |
| exist+prod19 | 1425.26 | 1451.51–1469.51 | 14 | 1425.15 | 7.32 | 7.32–7.37 | 2643 | 7.29 |
| planes+prod19 | 1555.84 | 1563.73–1568.08 | 12 | 1555.76 | 11.28 | 11.31–11.39 | 1714 | 11.25 |
| exist+prod3g | 34.63 | 35.26–35.75 | 558 | 34.62 | 5.56 | 5.56–5.58 | 3476 | 5.56 |
| planes+prod3g | 43.14 | 43.13–45.73 | 448 | 43.14 | 6.22 | 6.22–6.24 | 3108 | 6.22 |

### Native kernel timing, regime B planner fast, real inputs (n=11, 10944837 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.64 | 0.64–0.65 | 17151 | 0.64 | 0.50 | 0.50–0.52 | 22060 | 0.50 |
| exist+huf | 8.13 | 8.14–8.15 | 1347 | 8.13 | 4.97 | 4.96–4.99 | 2204 | 4.96 |
| exist+rans | 25.39 | 25.41–25.47 | 431 | 25.38 | 19.16 | 19.19–19.38 | 571 | 19.16 |
| exist+zstd3 | 12.19 | 12.20–12.43 | 898 | 12.19 | 4.02 | 4.02–4.05 | 2723 | 4.02 |
| gbp+store | 12.00 | 11.99–12.02 | 912 | 12.01 | 8.38 | 8.40–8.42 | 1305 | 8.39 |
| gbp+huf | 17.79 | 17.79–18.04 | 615 | 17.79 | 12.20 | 12.21–12.49 | 897 | 12.20 |
| gbp+rans | 31.34 | 31.37–31.42 | 349 | 31.34 | 23.24 | 23.21–23.34 | 471 | 23.24 |
| gbp+zstd3 | 27.31 | 27.46–27.69 | 401 | 27.31 | 12.85 | 12.85–12.88 | 852 | 12.84 |
| lbp128+store | 12.08 | 12.08–12.18 | 906 | 12.08 | 8.22 | 8.22–8.26 | 1331 | 8.22 |
| lbp128+huf | 16.27 | 16.25–16.35 | 673 | 16.27 | 11.35 | 11.32–11.37 | 964 | 11.35 |
| lbp128+rans | 25.89 | 25.91–25.98 | 423 | 25.89 | 19.06 | 19.09–19.13 | 574 | 19.06 |
| lbp128+zstd3 | 23.03 | 23.14–23.39 | 475 | 23.03 | 11.60 | 11.60–11.64 | 944 | 11.60 |
| gpp+store | 19.52 | 19.52–19.58 | 561 | 19.52 | 8.11 | 8.12–8.13 | 1349 | 8.11 |
| gpp+huf | 23.01 | 23.01–23.06 | 476 | 23.01 | 10.85 | 10.84–10.87 | 1009 | 10.85 |
| gpp+rans | 31.84 | 31.88–31.94 | 344 | 31.84 | 16.93 | 16.96–16.99 | 646 | 16.93 |
| gpp+zstd3 | 27.07 | 27.17–27.26 | 404 | 27.06 | 10.12 | 10.14–10.15 | 1082 | 10.11 |
| lpp128+store | 21.11 | 21.11–21.15 | 518 | 21.11 | 8.15 | 8.16–8.20 | 1343 | 8.15 |
| lpp128+huf | 24.56 | 24.58–24.71 | 446 | 24.56 | 10.97 | 10.97–11.00 | 998 | 10.97 |
| lpp128+rans | 33.03 | 33.01–33.16 | 331 | 33.03 | 16.62 | 16.63–16.73 | 659 | 16.62 |
| lpp128+zstd3 | 27.14 | 27.12–27.81 | 403 | 27.14 | 9.61 | 9.61–9.65 | 1139 | 9.61 |
| sel+store | 65.94 | 65.97–66.27 | 166 | 65.94 | 8.12 | 8.13–8.15 | 1348 | 8.12 |
| sel+huf | 90.17 | 90.21–90.58 | 121 | 90.16 | 9.75 | 9.75–9.83 | 1123 | 9.74 |
| sel+rans | 147.46 | 147.49–147.56 | 74 | 147.45 | 16.35 | 16.35–16.39 | 669 | 16.34 |
| sel+zstd3 | 115.77 | 115.99–116.30 | 95 | 115.77 | 7.26 | 7.27–7.31 | 1508 | 7.25 |
| planes+store | 0.78 | 0.78–0.80 | 14057 | 0.78 | 0.49 | 0.49–0.49 | 22545 | 0.49 |
| planes+huf | 8.16 | 8.15–8.18 | 1342 | 8.16 | 4.64 | 4.63–4.64 | 2360 | 4.64 |
| planes+rans | 23.96 | 23.95–24.03 | 457 | 23.96 | 16.95 | 16.95–17.01 | 646 | 16.94 |
| planes+zstd3 | 11.32 | 11.35–11.47 | 967 | 11.32 | 3.99 | 3.99–4.00 | 2742 | 3.99 |
| exist+ent | 33.27 | 33.28–33.34 | 329 | 33.27 | 7.82 | 7.82–7.95 | 1400 | 7.81 |
| planes+ent | 31.72 | 31.71–31.80 | 345 | 31.72 | 16.58 | 16.58–16.60 | 660 | 16.57 |
| lpp128+ent | 36.75 | 36.76–36.89 | 298 | 36.75 | 15.17 | 15.17–15.19 | 721 | 15.17 |
| exist+zstd19 | 730.40 | 730.09–734.87 | 15 | 730.36 | 4.42 | 4.43–4.44 | 2478 | 4.40 |
| planes+zstd19 | 852.71 | 850.21–853.27 | 13 | 852.66 | 4.65 | 4.66–4.67 | 2353 | 4.63 |
| exist+prod3 | 22.09 | 22.15–22.30 | 496 | 22.09 | 3.69 | 3.68–3.71 | 2969 | 3.68 |
| planes+prod3 | 22.05 | 22.12–22.22 | 496 | 22.05 | 4.36 | 4.36–4.41 | 2512 | 4.36 |
| exist+prod19 | 739.55 | 737.53–748.96 | 15 | 739.54 | 4.01 | 4.01–4.06 | 2730 | 3.98 |
| planes+prod19 | 871.23 | 870.09–871.46 | 13 | 871.14 | 8.08 | 8.09–8.13 | 1354 | 8.06 |
| exist+prod3g | 22.57 | 22.49–23.17 | 485 | 22.57 | 3.69 | 3.69–3.71 | 2967 | 3.69 |
| planes+prod3g | 23.03 | 23.01–23.21 | 475 | 23.03 | 4.12 | 4.10–4.14 | 2658 | 4.11 |

### Native kernel timing, regime B planner hc, real inputs (n=10, 11851774 residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)

| config | enc ms (wall) | enc IQR of per-rep sums | enc MB/s | enc ms (cpu) | dec ms (wall) | dec IQR | dec MB/s | dec ms (cpu) |
|---|---|---|---|---|---|---|---|---|
| exist+store | 0.67 | 0.67–0.68 | 17767 | 0.67 | 0.52 | 0.52–0.53 | 22696 | 0.52 |
| exist+huf | 8.75 | 8.74–8.77 | 1355 | 8.75 | 4.83 | 4.83–4.85 | 2455 | 4.83 |
| exist+rans | 26.01 | 26.06–26.26 | 456 | 26.01 | 17.89 | 17.98–18.23 | 662 | 17.89 |
| exist+zstd3 | 13.88 | 13.81–14.69 | 854 | 13.88 | 4.50 | 4.49–4.52 | 2636 | 4.49 |
| gbp+store | 12.19 | 12.19–12.53 | 972 | 12.19 | 8.94 | 8.95–8.97 | 1326 | 8.94 |
| gbp+huf | 19.32 | 19.33–19.39 | 614 | 19.32 | 12.87 | 12.87–13.29 | 921 | 12.87 |
| gbp+rans | 34.30 | 34.34–35.01 | 345 | 34.30 | 24.86 | 24.93–25.36 | 477 | 24.86 |
| gbp+zstd3 | 26.96 | 26.89–27.16 | 440 | 26.95 | 13.34 | 13.36–13.41 | 888 | 13.34 |
| lbp128+store | 11.78 | 11.79–11.80 | 1006 | 11.78 | 8.29 | 8.29–8.30 | 1429 | 8.29 |
| lbp128+huf | 15.81 | 15.85–15.87 | 750 | 15.81 | 11.00 | 10.99–11.03 | 1077 | 11.00 |
| lbp128+rans | 24.62 | 24.64–24.74 | 481 | 24.62 | 18.29 | 18.31–18.35 | 648 | 18.29 |
| lbp128+zstd3 | 24.76 | 24.51–26.80 | 479 | 24.76 | 11.87 | 11.87–12.48 | 998 | 11.87 |
| gpp+store | 17.94 | 17.96–18.03 | 661 | 17.94 | 7.52 | 7.51–7.55 | 1576 | 7.52 |
| gpp+huf | 20.71 | 20.74–20.78 | 572 | 20.71 | 9.64 | 9.64–9.67 | 1229 | 9.64 |
| gpp+rans | 27.03 | 27.03–27.13 | 438 | 27.02 | 14.53 | 14.54–14.60 | 816 | 14.53 |
| gpp+zstd3 | 24.30 | 24.32–25.43 | 488 | 24.29 | 9.01 | 9.03–9.05 | 1315 | 9.01 |
| lpp128+store | 19.67 | 19.70–20.01 | 603 | 19.67 | 7.56 | 7.56–7.61 | 1568 | 7.56 |
| lpp128+huf | 22.38 | 22.40–22.54 | 529 | 22.39 | 9.82 | 9.82–9.85 | 1206 | 9.82 |
| lpp128+rans | 28.25 | 28.30–28.47 | 420 | 28.25 | 14.52 | 14.53–14.60 | 816 | 14.51 |
| lpp128+zstd3 | 24.98 | 25.02–25.25 | 475 | 24.98 | 8.93 | 8.91–8.94 | 1327 | 8.93 |
| sel+store | 62.78 | 62.84–63.23 | 189 | 62.78 | 7.54 | 7.54–7.55 | 1571 | 7.54 |
| sel+huf | 87.29 | 87.34–87.47 | 136 | 87.29 | 8.93 | 8.91–8.98 | 1328 | 8.92 |
| sel+rans | 140.22 | 140.44–140.89 | 85 | 140.21 | 14.45 | 14.43–14.53 | 820 | 14.44 |
| sel+zstd3 | 112.04 | 111.96–115.78 | 106 | 112.03 | 5.83 | 5.84–5.90 | 2033 | 5.82 |
| planes+store | 0.77 | 0.77–0.77 | 15393 | 0.77 | 0.53 | 0.53–0.53 | 22523 | 0.53 |
| planes+huf | 8.86 | 8.86–8.87 | 1338 | 8.86 | 4.62 | 4.62–4.64 | 2566 | 4.62 |
| planes+rans | 25.05 | 25.04–25.09 | 473 | 25.05 | 18.28 | 18.21–18.39 | 648 | 18.27 |
| planes+zstd3 | 14.30 | 14.39–15.51 | 829 | 14.31 | 4.81 | 4.81–4.85 | 2463 | 4.81 |
| exist+ent | 34.48 | 34.50–34.66 | 344 | 34.48 | 11.40 | 11.42–11.58 | 1040 | 11.40 |
| planes+ent | 33.54 | 33.53–33.72 | 353 | 33.54 | 17.88 | 17.94–18.04 | 663 | 17.88 |
| lpp128+ent | 31.20 | 31.19–31.54 | 380 | 31.20 | 13.93 | 13.92–13.94 | 851 | 13.92 |
| exist+zstd19 | 904.30 | 919.53–925.61 | 13 | 904.21 | 4.37 | 4.36–4.41 | 2713 | 4.34 |
| planes+zstd19 | 959.42 | 958.55–962.68 | 12 | 959.33 | 4.61 | 4.62–5.11 | 2570 | 4.59 |
| exist+prod3 | 23.06 | 22.87–23.59 | 514 | 23.05 | 4.03 | 4.03–4.07 | 2943 | 4.03 |
| planes+prod3 | 28.46 | 28.54–29.74 | 416 | 28.46 | 4.82 | 4.84–4.85 | 2457 | 4.82 |
| exist+prod19 | 911.35 | 913.11–928.76 | 13 | 911.27 | 3.92 | 3.91–3.94 | 3024 | 3.90 |
| planes+prod19 | 968.88 | 973.46–983.98 | 12 | 968.82 | 7.73 | 7.75–7.83 | 1533 | 7.71 |
| exist+prod3g | 23.02 | 22.85–23.73 | 515 | 23.02 | 4.04 | 4.04–4.05 | 2935 | 4.04 |
| planes+prod3g | 29.50 | 29.50–31.33 | 402 | 29.50 | 4.49 | 4.49–4.50 | 2641 | 4.49 |

### End-to-end, complete planner fast, real inputs with INT blocks (n=11, 15381288 B). Encode for experimental rows = production compress - production residual-leaf re-encode (12.1 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 3941831 | 118.8 | 18.33 | 18.25 |
| zstd -3 whole input (direct control) | 5052355 | 57.1 | 14.27 | - |
| exist+store | 10947399 | 107.4 | - | 15.18 |
| exist+huf | 4568552 | 114.9 | - | 19.80 |
| exist+rans | 4939403 | 132.2 | - | 33.79 |
| exist+zstd3 | 3945229 | 120.0 | - | 18.69 |
| gbp+store | 7036654 | 118.7 | - | 23.03 |
| gbp+huf | 4824565 | 124.5 | - | 26.82 |
| gbp+rans | 4671625 | 138.1 | - | 37.97 |
| gbp+zstd3 | 4661976 | 135.0 | - | 27.62 |
| lbp128+store | 5117611 | 118.8 | - | 22.87 |
| lbp128+huf | 4286581 | 123.0 | - | 25.92 |
| lbp128+rans | 4244696 | 132.5 | - | 33.71 |
| lbp128+zstd3 | 4289146 | 130.8 | - | 26.37 |
| gpp+store | 4335033 | 126.2 | - | 22.71 |
| gpp+huf | 4018232 | 129.7 | - | 25.46 |
| gpp+rans | 4010985 | 138.5 | - | 31.56 |
| gpp+zstd3 | 4014919 | 134.5 | - | 24.78 |
| lpp128+store | 4226624 | 127.8 | - | 22.77 |
| lpp128+huf | 4033639 | 131.4 | - | 25.57 |
| lpp128+rans | 4032185 | 139.7 | - | 31.26 |
| lpp128+zstd3 | 4011671 | 134.7 | - | 24.32 |
| sel+store | 4213099 | 173.0 | - | 22.87 |
| sel+huf | 3957303 | 197.0 | - | 24.45 |
| sel+rans | 3963960 | 254.4 | - | 31.07 |
| sel+zstd3 | 3923117 | 223.1 | - | 22.00 |
| planes+store | 10947763 | 107.7 | - | 15.29 |
| planes+huf | 4490031 | 115.0 | - | 19.20 |
| planes+rans | 5313809 | 130.7 | - | 31.62 |
| planes+zstd3 | 3922075 | 119.0 | - | 18.68 |
| exist+ent | 4418759 | 140.1 | - | 22.48 |
| planes+ent | 3955416 | 138.4 | - | 31.12 |
| lpp128+ent | 4022800 | 143.6 | - | 29.74 |
| exist+zstd19 | 3906618 | 867.7 | - | 19.29 |
| planes+zstd19 | 3890049 | 969.0 | - | 19.47 |
| exist+prod3 | 3942090 | 130.1 | - | 18.31 |
| planes+prod3 | 4011613 | 129.7 | - | 19.07 |
| exist+prod19 | 3906703 | 871.6 | - | 18.81 |
| planes+prod19 | 3886102 | 987.4 | - | 23.03 |
| exist+prod3g | 3942090 | 130.5 | - | 18.33 |
| planes+prod3g | 3913409 | 130.8 | - | 18.67 |

### End-to-end, complete planner hc, real inputs with INT blocks (n=10, 19375192 B). Encode for experimental rows = production compress - production residual-leaf re-encode (858.3 ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.

| pipeline | stored bytes | encode ms (wall) | decode API ms (1 thread) | decode sequential ms |
|---|---|---|---|---|
| production kolmo (unmodified) | 3094828 | 999.7 | 19.04 | 18.99 |
| zstd -3 whole input (direct control) | 4046567 | 48.3 | 13.06 | - |
| exist+store | 11853596 | 142.3 | - | 15.99 |
| exist+huf | 3880031 | 150.3 | - | 20.29 |
| exist+rans | 4522775 | 167.5 | - | 33.28 |
| exist+zstd3 | 3115985 | 156.8 | - | 19.89 |
| gbp+store | 8303154 | 153.5 | - | 25.36 |
| gbp+huf | 4174354 | 160.8 | - | 29.27 |
| gbp+rans | 3777994 | 176.3 | - | 41.95 |
| gbp+zstd3 | 3733717 | 169.1 | - | 29.72 |
| lbp128+store | 4845298 | 153.2 | - | 23.71 |
| lbp128+huf | 3503970 | 157.3 | - | 26.62 |
| lbp128+rans | 3426183 | 166.2 | - | 33.71 |
| lbp128+zstd3 | 3490947 | 167.6 | - | 27.38 |
| gpp+store | 3477253 | 159.4 | - | 22.89 |
| gpp+huf | 3168302 | 162.1 | - | 25.35 |
| gpp+rans | 3151455 | 168.5 | - | 29.95 |
| gpp+zstd3 | 3163066 | 166.3 | - | 24.43 |
| lpp128+store | 3451077 | 161.0 | - | 22.92 |
| lpp128+huf | 3259391 | 164.0 | - | 25.36 |
| lpp128+rans | 3252013 | 169.6 | - | 29.83 |
| lpp128+zstd3 | 3177434 | 167.2 | - | 24.33 |
| sel+store | 3426992 | 204.6 | - | 23.02 |
| sel+huf | 3134021 | 228.9 | - | 24.53 |
| sel+rans | 3134811 | 286.8 | - | 30.96 |
| sel+zstd3 | 3086988 | 254.5 | - | 21.27 |
| planes+store | 11853799 | 142.4 | - | 16.08 |
| planes+huf | 3847065 | 150.4 | - | 20.39 |
| planes+rans | 4429936 | 166.5 | - | 33.74 |
| planes+zstd3 | 3105657 | 157.0 | - | 20.24 |
| exist+ent | 3468121 | 176.1 | - | 26.92 |
| planes+ent | 3159725 | 175.0 | - | 33.29 |
| lpp128+ent | 3248243 | 172.6 | - | 29.23 |
| exist+zstd19 | 3094922 | 1040.2 | - | 20.07 |
| planes+zstd19 | 3077877 | 1097.6 | - | 20.31 |
| exist+prod3 | 3105846 | 165.7 | - | 19.43 |
| planes+prod3 | 3221227 | 170.7 | - | 20.21 |
| exist+prod19 | 3094958 | 1037.1 | - | 19.62 |
| planes+prod19 | 3075483 | 1114.2 | - | 23.55 |
| exist+prod3g | 3105846 | 166.0 | - | 19.46 |
| planes+prod3g | 3088157 | 171.9 | - | 19.88 |

### Peak memory, fast mode, real inputs with INT blocks: process RSS high-water above the RSS just before the operation (KiB; /proc clear_refs reset after malloc_trim; input, frame and output buffers pre-touched)

| operation | config | max over inputs | median over inputs |
|---|---|---|---|
| prod_compress | - | 3632 | 2620 |
| prod_decompress_api | - | 2140 | 1988 |
| prod_decode_seq | - | 2136 | 1984 |
| exp_residual_encode | exist+store | 2448 | 1956 |
| exp_residual_decode | exist+store | 2056 | 1888 |
| exp_decode_seq | exist+store | 2056 | 1952 |
| exp_residual_encode | exist+huf | 1904 | 1468 |
| exp_residual_decode | exist+huf | 2056 | 1888 |
| exp_decode_seq | exist+huf | 2056 | 1980 |
| exp_residual_encode | exist+rans | 2720 | 2196 |
| exp_residual_decode | exist+rans | 2512 | 2016 |
| exp_decode_seq | exist+rans | 2436 | 2032 |
| exp_residual_encode | exist+zstd3 | 2784 | 2500 |
| exp_residual_decode | exist+zstd3 | 2076 | 1988 |
| exp_decode_seq | exist+zstd3 | 2076 | 2048 |
| exp_residual_encode | gbp+store | 5572 | 2212 |
| exp_residual_decode | gbp+store | 6036 | 2548 |
| exp_decode_seq | gbp+store | 6604 | 2824 |
| exp_residual_encode | gbp+huf | 5560 | 2124 |
| exp_residual_decode | gbp+huf | 6036 | 2548 |
| exp_decode_seq | gbp+huf | 6560 | 2824 |
| exp_residual_encode | gbp+rans | 5760 | 2676 |
| exp_residual_decode | gbp+rans | 6096 | 2608 |
| exp_decode_seq | gbp+rans | 6636 | 2624 |
| exp_residual_encode | gbp+zstd3 | 6540 | 3060 |
| exp_residual_decode | gbp+zstd3 | 6124 | 2628 |
| exp_decode_seq | gbp+zstd3 | 6684 | 2756 |
| exp_residual_encode | lbp128+store | 5584 | 2184 |
| exp_residual_decode | lbp128+store | 5760 | 2520 |
| exp_decode_seq | lbp128+store | 6036 | 2600 |
| exp_residual_encode | lbp128+huf | 5572 | 2156 |
| exp_residual_decode | lbp128+huf | 5760 | 2520 |
| exp_decode_seq | lbp128+huf | 5972 | 2732 |
| exp_residual_encode | lbp128+rans | 5632 | 2696 |
| exp_residual_decode | lbp128+rans | 5820 | 2580 |
| exp_decode_seq | lbp128+rans | 6016 | 2628 |
| exp_residual_encode | lbp128+zstd3 | 7100 | 3080 |
| exp_residual_decode | lbp128+zstd3 | 6036 | 2604 |
| exp_decode_seq | lbp128+zstd3 | 6092 | 2688 |
| exp_residual_encode | gpp+store | 5508 | 2044 |
| exp_residual_decode | gpp+store | 5736 | 2488 |
| exp_decode_seq | gpp+store | 6048 | 2568 |
| exp_residual_encode | gpp+huf | 5496 | 2036 |
| exp_residual_decode | gpp+huf | 5736 | 2488 |
| exp_decode_seq | gpp+huf | 6048 | 2568 |
| exp_residual_encode | gpp+rans | 5632 | 2524 |
| exp_residual_decode | gpp+rans | 5748 | 2596 |
| exp_decode_seq | gpp+rans | 6016 | 2612 |
| exp_residual_encode | gpp+zstd3 | 6504 | 2992 |
| exp_residual_decode | gpp+zstd3 | 5828 | 2580 |
| exp_decode_seq | gpp+zstd3 | 6044 | 2664 |
| exp_residual_encode | lpp128+store | 5432 | 2068 |
| exp_residual_decode | lpp128+store | 5764 | 2476 |
| exp_decode_seq | lpp128+store | 5792 | 2560 |
| exp_residual_encode | lpp128+huf | 5408 | 2064 |
| exp_residual_decode | lpp128+huf | 5764 | 2476 |
| exp_decode_seq | lpp128+huf | 5792 | 2560 |
| exp_residual_encode | lpp128+rans | 5760 | 2556 |
| exp_residual_decode | lpp128+rans | 5824 | 2588 |
| exp_decode_seq | lpp128+rans | 5908 | 2604 |
| exp_residual_encode | lpp128+zstd3 | 6564 | 3028 |
| exp_residual_decode | lpp128+zstd3 | 5936 | 2512 |
| exp_decode_seq | lpp128+zstd3 | 5940 | 2572 |
| exp_residual_encode | sel+store | 7708 | 3584 |
| exp_residual_decode | sel+store | 5764 | 2480 |
| exp_decode_seq | sel+store | 6308 | 2560 |
| exp_residual_encode | sel+huf | 6272 | 3096 |
| exp_residual_decode | sel+huf | 5768 | 1960 |
| exp_decode_seq | sel+huf | 5856 | 2560 |
| exp_residual_encode | sel+rans | 6848 | 4736 |
| exp_residual_decode | sel+rans | 5828 | 2592 |
| exp_decode_seq | sel+rans | 5856 | 2608 |
| exp_residual_encode | sel+zstd3 | 7720 | 4428 |
| exp_residual_decode | sel+zstd3 | 5936 | 2284 |
| exp_decode_seq | sel+zstd3 | 6380 | 2508 |
| exp_residual_encode | planes+store | 2152 | 1888 |
| exp_residual_decode | planes+store | 1840 | 1104 |
| exp_decode_seq | planes+store | 2068 | 1784 |
| exp_residual_encode | planes+huf | 1604 | 1508 |
| exp_residual_decode | planes+huf | 1840 | 1104 |
| exp_decode_seq | planes+huf | 2056 | 1956 |
| exp_residual_encode | planes+rans | 2444 | 1956 |
| exp_residual_decode | planes+rans | 1900 | 1164 |
| exp_decode_seq | planes+rans | 2440 | 2032 |
| exp_residual_encode | planes+zstd3 | 3756 | 2364 |
| exp_residual_decode | planes+zstd3 | 1832 | 1200 |
| exp_decode_seq | planes+zstd3 | 2180 | 1964 |
| exp_residual_encode | exist+ent | 2944 | 2316 |
| exp_residual_decode | exist+ent | 2452 | 1956 |
| exp_decode_seq | exist+ent | 2436 | 2036 |
| exp_residual_encode | planes+ent | 3124 | 1928 |
| exp_residual_decode | planes+ent | 1900 | 1164 |
| exp_decode_seq | planes+ent | 2440 | 2032 |
| exp_residual_encode | lpp128+ent | 6272 | 3048 |
| exp_residual_decode | lpp128+ent | 5824 | 2524 |
| exp_decode_seq | lpp128+ent | 5852 | 2540 |
| exp_residual_encode | exist+zstd19 | 19044 | 18664 |
| exp_residual_decode | exist+zstd19 | 2076 | 1984 |
| exp_decode_seq | exist+zstd19 | 2088 | 2048 |
| exp_residual_encode | planes+zstd19 | 10528 | 3840 |
| exp_residual_decode | planes+zstd19 | 1932 | 1196 |
| exp_decode_seq | planes+zstd19 | 2080 | 2040 |
| exp_residual_encode | exist+prod3 | 5392 | 4456 |
| exp_residual_decode | exist+prod3 | 1312 | 1008 |
| exp_decode_seq | exist+prod3 | 2544 | 1988 |
| exp_residual_encode | planes+prod3 | 6144 | 3860 |
| exp_residual_decode | planes+prod3 | 1312 | 1072 |
| exp_decode_seq | planes+prod3 | 2252 | 2044 |
| exp_residual_encode | exist+prod19 | 20876 | 20120 |
| exp_residual_decode | exist+prod19 | 1280 | 1008 |
| exp_decode_seq | exist+prod19 | 2452 | 1980 |
| exp_residual_encode | planes+prod19 | 12868 | 5656 |
| exp_residual_decode | planes+prod19 | 1372 | 1052 |
| exp_decode_seq | planes+prod19 | 2604 | 2044 |
| exp_residual_encode | exist+prod3g | 5392 | 4456 |
| exp_residual_decode | exist+prod3g | 1312 | 1008 |
| exp_decode_seq | exist+prod3g | 2544 | 1988 |
| exp_residual_encode | planes+prod3g | 6152 | 3860 |
| exp_residual_decode | planes+prod3g | 1312 | 1072 |
| exp_decode_seq | planes+prod3g | 2252 | 2044 |
