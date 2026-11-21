
#### compress: MB/s (fastest of 5–5), and time relative to `generic`

| input | mode | T | generic | avx2 | native | dispatch | avx2/generic | native/generic | dispatch/generic |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| silesia.tar | speed | 1 | 416.3 | 419.4 | 409.5 | 401.7 | 0.993 | 1.017 | 1.036 |
| silesia.tar | speed | 4 | 1407.4 | 1445.7 | 1413.9 | 1373.9 | 0.974 | 0.995 | 1.024 |
| silesia.tar | balanced | 1 | 175.8 | 183.8 | 190.8 | 192.0 | 0.957 | 0.921 | 0.916 |
| silesia.tar | balanced | 4 | 629.6 | 641.6 | 638.0 | 652.7 | 0.981 | 0.987 | 0.965 |
| silesia.tar | hc | 1 | 2.6 | 2.6 | 2.5 | 2.5 | 1.015 | 1.039 | 1.057 |
| silesia.tar | hc | 4 | 8.2 | 8.4 | 8.5 | 8.7 | 0.984 | 0.972 | 0.947 |
| silesia.tar | ratio | 1 | 2.8 | 2.7 | 2.8 | 2.6 | 1.015 | 0.990 | 1.050 |
| silesia.tar | ratio | 4 | 5.6 | 5.6 | 5.9 | 6.0 | 1.008 | 0.950 | 0.934 |
| trades.csv | speed | 1 | 325.2 | 352.4 | 357.2 | 340.2 | 0.923 | 0.910 | 0.956 |
| trades.csv | speed | 4 | 1017.2 | 1048.1 | 1047.9 | 1008.5 | 0.971 | 0.971 | 1.009 |
| trades.csv | balanced | 1 | 180.4 | 199.8 | 197.5 | 178.0 | 0.903 | 0.913 | 1.014 |
| trades.csv | balanced | 4 | 454.9 | 521.8 | 555.5 | 462.1 | 0.872 | 0.819 | 0.984 |
| trades.csv | hc | 1 | 9.0 | 9.0 | 9.0 | 8.7 | 1.003 | 1.003 | 1.043 |
| trades.csv | hc | 4 | 26.9 | 27.1 | 24.8 | 24.9 | 0.992 | 1.085 | 1.077 |
| trades.csv | ratio | 1 | 4.6 | 4.6 | 4.7 | 4.5 | 0.989 | 0.968 | 1.017 |
| trades.csv | ratio | 4 | 4.6 | 4.5 | 4.4 | 4.6 | 1.011 | 1.025 | 0.983 |
| events.ndjson | speed | 1 | 478.5 | 495.7 | 507.3 | 499.7 | 0.965 | 0.943 | 0.958 |
| events.ndjson | speed | 4 | 1536.2 | 1565.7 | 1631.2 | 1605.7 | 0.981 | 0.942 | 0.957 |
| events.ndjson | balanced | 1 | 180.3 | 179.7 | 188.9 | 177.5 | 1.003 | 0.954 | 1.015 |
| events.ndjson | balanced | 4 | 574.9 | 532.4 | 591.8 | 547.5 | 1.080 | 0.971 | 1.050 |
| events.ndjson | hc | 1 | 5.1 | 5.1 | 5.0 | 4.9 | 1.002 | 1.021 | 1.037 |
| events.ndjson | hc | 4 | 14.5 | 15.0 | 14.9 | 14.6 | 0.972 | 0.975 | 1.000 |
| events.ndjson | ratio | 1 | 39.8 | 39.2 | 42.3 | 40.0 | 1.014 | 0.942 | 0.994 |
| events.ndjson | ratio | 4 | 38.7 | 38.7 | 39.3 | 39.4 | 1.002 | 0.986 | 0.984 |
| walk_i32.npy | speed | 1 | 916.5 | 943.9 | 916.8 | 845.1 | 0.971 | 1.000 | 1.084 |
| walk_i32.npy | speed | 4 | 2025.8 | 2212.2 | 1716.4 | 1797.0 | 0.916 | 1.180 | 1.127 |
| walk_i32.npy | balanced | 1 | 302.5 | 304.1 | 319.8 | 318.6 | 0.995 | 0.946 | 0.950 |
| walk_i32.npy | balanced | 4 | 897.5 | 715.4 | 870.9 | 918.7 | 1.255 | 1.031 | 0.977 |
| walk_i32.npy | hc | 1 | 5.5 | 5.6 | 5.2 | 5.2 | 0.994 | 1.060 | 1.071 |
| walk_i32.npy | hc | 4 | 20.5 | 19.8 | 19.9 | 19.7 | 1.034 | 1.029 | 1.040 |
| walk_i32.npy | ratio | 1 | 2.6 | 2.6 | 2.6 | 2.6 | 1.010 | 1.013 | 1.018 |
| walk_i32.npy | ratio | 4 | 2.7 | 2.6 | 2.6 | 2.6 | 1.041 | 1.017 | 1.005 |

#### decompress: MB/s (fastest of 5–5), and time relative to `generic`

| input | mode | T | generic | avx2 | native | dispatch | avx2/generic | native/generic | dispatch/generic |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| silesia.tar | speed | 1 | 1252.5 | 1384.3 | 1354.2 | 1221.0 | 0.905 | 0.925 | 1.026 |
| silesia.tar | speed | 4 | 3644.6 | 3809.7 | 3837.9 | 3671.8 | 0.957 | 0.950 | 0.993 |
| silesia.tar | balanced | 1 | 657.3 | 647.8 | 682.6 | 675.6 | 1.015 | 0.963 | 0.973 |
| silesia.tar | balanced | 4 | 2147.9 | 2094.2 | 2145.9 | 2147.7 | 1.026 | 1.001 | 1.000 |
| silesia.tar | hc | 1 | 427.2 | 413.7 | 418.7 | 411.1 | 1.033 | 1.020 | 1.039 |
| silesia.tar | hc | 4 | 1011.6 | 1162.1 | 1189.7 | 1144.6 | 0.870 | 0.850 | 0.884 |
| silesia.tar | ratio | 1 | 41.1 | 40.2 | 40.2 | 38.1 | 1.023 | 1.021 | 1.078 |
| silesia.tar | ratio | 4 | 83.1 | 90.3 | 92.6 | 95.5 | 0.920 | 0.898 | 0.870 |
| trades.csv | speed | 1 | 1377.9 | 1518.5 | 1413.4 | 1511.5 | 0.907 | 0.975 | 0.912 |
| trades.csv | speed | 4 | 2655.9 | 2611.0 | 2758.6 | 2731.6 | 1.017 | 0.963 | 0.972 |
| trades.csv | balanced | 1 | 313.6 | 443.2 | 461.1 | 426.0 | 0.708 | 0.680 | 0.736 |
| trades.csv | balanced | 4 | 831.9 | 1126.2 | 1080.6 | 1103.1 | 0.739 | 0.770 | 0.754 |
| trades.csv | hc | 1 | 171.1 | 201.0 | 216.1 | 201.4 | 0.851 | 0.792 | 0.850 |
| trades.csv | hc | 4 | 282.4 | 305.9 | 268.8 | 299.8 | 0.923 | 1.051 | 0.942 |
| trades.csv | ratio | 1 | 76.0 | 82.8 | 81.2 | 77.6 | 0.917 | 0.935 | 0.979 |
| trades.csv | ratio | 4 | 80.0 | 87.4 | 84.0 | 79.6 | 0.915 | 0.952 | 1.005 |
| events.ndjson | speed | 1 | 1459.4 | 1567.5 | 1510.3 | 1606.6 | 0.931 | 0.966 | 0.908 |
| events.ndjson | speed | 4 | 2868.4 | 3266.2 | 3511.3 | 3452.4 | 0.878 | 0.817 | 0.831 |
| events.ndjson | balanced | 1 | 616.3 | 671.2 | 665.3 | 644.8 | 0.918 | 0.926 | 0.956 |
| events.ndjson | balanced | 4 | 1371.5 | 1591.1 | 2053.9 | 1890.8 | 0.862 | 0.668 | 0.725 |
| events.ndjson | hc | 1 | 247.5 | 218.8 | 242.2 | 237.3 | 1.131 | 1.022 | 1.043 |
| events.ndjson | hc | 4 | 280.6 | 321.7 | 360.3 | 286.4 | 0.872 | 0.779 | 0.980 |
| events.ndjson | ratio | 1 | 62.5 | 64.1 | 62.5 | 62.3 | 0.976 | 1.000 | 1.003 |
| events.ndjson | ratio | 4 | 64.0 | 61.1 | 63.7 | 61.6 | 1.047 | 1.005 | 1.040 |
| walk_i32.npy | speed | 1 | 1619.2 | 1532.4 | 1562.1 | 1759.1 | 1.057 | 1.037 | 0.920 |
| walk_i32.npy | speed | 4 | 2632.7 | 2969.0 | 2522.6 | 2872.1 | 0.887 | 1.044 | 0.917 |
| walk_i32.npy | balanced | 1 | 989.6 | 1044.3 | 1028.7 | 1127.9 | 0.948 | 0.962 | 0.877 |
| walk_i32.npy | balanced | 4 | 1993.1 | 1940.6 | 2200.8 | 2043.1 | 1.027 | 0.906 | 0.976 |
| walk_i32.npy | hc | 1 | 202.3 | 223.6 | 196.3 | 226.2 | 0.905 | 1.031 | 0.894 |
| walk_i32.npy | hc | 4 | 291.2 | 339.1 | 314.0 | 326.7 | 0.859 | 0.927 | 0.891 |
| walk_i32.npy | ratio | 1 | 165.1 | 151.1 | 164.9 | 161.3 | 1.093 | 1.002 | 1.024 |
| walk_i32.npy | ratio | 4 | 168.6 | 158.6 | 160.8 | 164.7 | 1.063 | 1.048 | 1.024 |

steal: mean 0.54% of CPU time, max 12.67%, runs with steal > 1%: 174 of 1280

frames: 32 (input, mode, threads) cases x 4 builds; all builds byte-identical: True; all round trips exact: True

| input | mode | T | size (bytes) | SHA-256 (first 16) | builds agreeing |
|---|---|---:|---:|---|---:|
| silesia.tar | speed | 1 | 99,918,100 | `88007657ae0a774f` | 4/4 |
| silesia.tar | speed | 4 | 99,918,100 | `88007657ae0a774f` | 4/4 |
| silesia.tar | balanced | 1 | 58,245,943 | `43ef81d7fc5da90e` | 4/4 |
| silesia.tar | balanced | 4 | 58,245,943 | `43ef81d7fc5da90e` | 4/4 |
| silesia.tar | hc | 1 | 47,624,828 | `2342befaad9eb08e` | 4/4 |
| silesia.tar | hc | 4 | 47,624,828 | `2342befaad9eb08e` | 4/4 |
| silesia.tar | ratio | 1 | 40,687,857 | `649ccb3c09a84e36` | 4/4 |
| silesia.tar | ratio | 4 | 40,687,857 | `649ccb3c09a84e36` | 4/4 |
| trades.csv | speed | 1 | 36,817,721 | `849c8ec4a1840aa6` | 4/4 |
| trades.csv | speed | 4 | 36,817,721 | `849c8ec4a1840aa6` | 4/4 |
| trades.csv | balanced | 1 | 12,085,484 | `f4d6fc81bbd9fca6` | 4/4 |
| trades.csv | balanced | 4 | 12,085,484 | `f4d6fc81bbd9fca6` | 4/4 |
| trades.csv | hc | 1 | 11,782,261 | `7a690f0d706ac084` | 4/4 |
| trades.csv | hc | 4 | 11,782,261 | `7a690f0d706ac084` | 4/4 |
| trades.csv | ratio | 1 | 11,615,464 | `ec4d6ac51681e842` | 4/4 |
| trades.csv | ratio | 4 | 11,615,464 | `ec4d6ac51681e842` | 4/4 |
| events.ndjson | speed | 1 | 19,727,354 | `59041f724700f1f8` | 4/4 |
| events.ndjson | speed | 4 | 19,727,354 | `59041f724700f1f8` | 4/4 |
| events.ndjson | balanced | 1 | 6,770,194 | `a2eeaf90fe08beb7` | 4/4 |
| events.ndjson | balanced | 4 | 6,770,194 | `a2eeaf90fe08beb7` | 4/4 |
| events.ndjson | hc | 1 | 5,178,170 | `a47ce72a4fb6a5af` | 4/4 |
| events.ndjson | hc | 4 | 5,178,170 | `a47ce72a4fb6a5af` | 4/4 |
| events.ndjson | ratio | 1 | 4,252,578 | `d9a0b7c3b127b8c3` | 4/4 |
| events.ndjson | ratio | 4 | 4,252,578 | `d9a0b7c3b127b8c3` | 4/4 |
| walk_i32.npy | speed | 1 | 67,107,281 | `dc0bd9071ff4bcf2` | 4/4 |
| walk_i32.npy | speed | 4 | 67,107,281 | `dc0bd9071ff4bcf2` | 4/4 |
| walk_i32.npy | balanced | 1 | 26,541,943 | `2bcea0bf21426717` | 4/4 |
| walk_i32.npy | balanced | 4 | 26,541,943 | `2bcea0bf21426717` | 4/4 |
| walk_i32.npy | hc | 1 | 26,541,311 | `8601ab034763a27f` | 4/4 |
| walk_i32.npy | hc | 4 | 26,541,311 | `8601ab034763a27f` | 4/4 |
| walk_i32.npy | ratio | 1 | 26,377,410 | `5645d7d50063f32b` | 4/4 |
| walk_i32.npy | ratio | 4 | 26,377,410 | `5645d7d50063f32b` | 4/4 |
