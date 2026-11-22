| input | mode | T | generic ms | avx2 ms | native ms | dispatch ms | avx2/generic | native/generic | dispatch/generic |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| trades.csv | balanced | 1 | 153 | 115 | 112 | 118 | 0.748 | 0.729 | 0.766 |
| trades.csv | balanced | 4 | 52 | 41 | 40 | 41 | 0.781 | 0.770 | 0.787 |
| trades.csv | hc | 1 | 173 | 135 | 133 | 139 | 0.780 | 0.769 | 0.806 |
| walk_i32.npy | balanced | 1 | 65 | 62 | 61 | 54 | 0.958 | 0.942 | 0.824 |
| walk_i32.npy | speed | 1 | 36 | 36 | 35 | 36 | 1.007 | 0.996 | 1.013 |
| silesia.tar | speed | 1 | 144 | 144 | 140 | 142 | 0.996 | 0.969 | 0.986 |
| silesia.tar | hc | 4 | 150 | 138 | 159 | 147 | 0.922 | 1.062 | 0.982 |
| events.ndjson | balanced | 4 | 39 | 40 | 40 | 41 | 1.029 | 1.023 | 1.052 |
