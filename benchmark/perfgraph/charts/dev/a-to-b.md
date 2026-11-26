# Kolmo A (v0.0.2) to B (main a1d39559), all dev groups together, one thread

Ratio is original/compressed; speeds are decimal MB/s (median of 3). "Same bytes: yes" = A and B wrote byte-identical outputs (SHA-256) on all 17 inputs.

| Level | Ratio A → B | Encode MB/s A → B | Decode MB/s A → B | Combined MB/s A → B | Same bytes |
|---|---|---|---|---|---|
| 1 | 2.106 → 2.147 (+2.0%) | 462.9 → 486.2 (+5.0%) | 758.3 → 831.7 (+9.7%) | 290.39 → 306.57 (+5.6%) | no |
| 2 | 2.749 → 2.777 (+1.0%) | 361.3 → 379.0 (+4.9%) | 584.0 → 661.1 (+13.2%) | 225.72 → 240.92 (+6.7%) | no |
| 3 | 3.552 → 3.562 (+0.3%) | 164.8 → 160.4 (-2.7%) | 537.0 → 593.6 (+10.5%) | 125.87 → 126.07 (+0.2%) | no |
| 4 | 3.810 → 3.810 (+0.0%) | 44.1 → 43.7 (-1.0%) | 488.1 → 510.1 (+4.5%) | 40.40 → 40.22 (-0.5%) | yes |
| 5 | 3.906 → 3.906 (+0.0%) | 16.9 → 14.6 (-13.2%) | 464.3 → 487.0 (+4.9%) | 16.28 → 14.21 (-12.7%) | yes |
| 6 | 3.962 → 3.962 (+0.0%) | 10.5 → 9.6 (-8.6%) | 495.4 → 550.8 (+11.2%) | 10.31 → 9.46 (-8.2%) | yes |
| 7 | 3.973 → 3.973 (+0.0%) | 5.4 → 4.6 (-14.5%) | 511.1 → 547.3 (+7.1%) | 5.36 → 4.60 (-14.3%) | yes |
| 8 | 4.227 → 4.237 (+0.2%) | 14.0 → 12.5 (-11.2%) | 43.4 → 51.8 (+19.5%) | 10.63 → 10.06 (-5.4%) | no |
| 9 | 4.295 → 4.295 (+0.0%) | 6.0 → 5.9 (-0.5%) | 53.3 → 53.3 (+0.1%) | 5.36 → 5.34 (-0.4%) | yes |
| U | 4.608 → 4.608 (+0.0%) | 0.9 → 1.0 (+3.7%) | 1.1 → 1.2 (+4.5%) | 0.52 → 0.54 (+4.2%) | yes |

No A/A (same binary twice) arm was run, so changes of a few percent are within the noise; Codex's A/A on this Mac moved by up to about ±5%. Levels 5-7 write identical bytes yet encode 9-15% slower on main. That is larger than the noise and is worth an A/A recheck before it is called a regression.
