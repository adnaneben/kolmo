fastest vs second fastest of 5: median gap 4.4%, 90th percentile 16.4%
median of 5 vs fastest: median 11.1%

| direction | T | avx2/generic | native/generic | dispatch/generic |
|---|---:|---:|---:|---:|
| compress | 1 | 0.984 | 0.976 | 1.012 |
| compress | 4 | 1.002 | 0.993 | 1.003 |
| decompress | 1 | 0.952 | 0.949 | 0.947 |
| decompress | 4 | 0.925 | 0.907 | 0.920 |
