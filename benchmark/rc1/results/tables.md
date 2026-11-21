# Kolmo 1.2.0-rc1: tables made by benchmark/rc1/report.py from the result files

## A1. `silesia.tar` (211,948,032 bytes), every setting, in memory, one thread

| Setting | Compressed bytes | Ratio | Compress MB/s | Decompress MB/s | Peak memory above the memcpy row, MB | Mark |
|---|---|---|---|---|---|---|
| memcpy | 211,948,032 | 1.000 | 58184 | 58172 |  |  |
| lz4 1.11.0 | 100,298,039 | 2.113 | 1067 | 6945 | 0 |  |
| kolmo -m speed | 99,918,526 | 2.121 | 1055 | 6066 | 9 |  |
| kolmo -m rt | 98,721,692 | 2.147 | 961 | 4896 | 9 |  |
| lzav 7.1 -1 | 83,154,607 | 2.549 | 935 | 5054 | 1 |  |
| zxc 1.0.0 -1 | 130,717,936 | 1.621 | 1179 | 18477 | 1 |  |
| zxc 1.0.0 -3 | 99,289,514 | 2.135 | 361 | 13760 | 2 |  |
| snappy 1.3.1 | 101,435,630 | 2.089 | 857 | 4097 | 0 |  |
| lzfse 2017-05-22 | 67,628,076 | 3.134 | 204 | 1631 | 1 |  |
| zstd 1.5.7 -1 | 73,191,683 | 2.896 | 838 | 2297 | 1 |  |
| kolmo -m fast -l 1 -Q | 70,908,908 | 2.989 | 845 | 2226 | 9 |  |
| kolmo -m fast -l 1 -q | 64,282,916 | 3.297 | 700 | 2059 | 21 |  |
| kolmo -m fast -q | 59,066,220 | 3.588 | 512 | 1975 | 21 |  |
| zstd 1.5.7 -3 | 66,133,404 | 3.205 | 542 | 2187 | 2 |  |
| kolmo -m balanced | 58,246,155 | 3.639 | 481 | 1857 | 25 |  |
| kolmo control build -m balanced | 58,246,155 | 3.639 | 474 | 1817 | 25 |  |
| libdeflate 1.26 -6 | 67,511,173 | 3.139 | 177 | 1458 | 1 |  |
| zlib-ng 2.3.3 -6 | 68,915,203 | 3.075 | 146 | 759 | 0 |  |
| brotli 1.2.0 -5 | 59,565,677 | 3.558 | 116 | 721 | 355 |  |
| kolmo -m eco | 50,606,942 | 4.188 | 38.3 | 743 | 807 |  |
| zstd 1.5.7 -9 | 59,074,311 | 3.588 | 128 | 2412 | 11 |  |
| kolmo -m hc -l 9 | 52,355,017 | 4.048 | 120 | 1717 | 228 |  |
| zstd 1.5.7 -19 | 52,842,305 | 4.011 | 6.25 | 2255 | 85 |  |
| zstd 1.5.7 -22 | 52,283,426 | 4.054 | 4.12 | 2094 | 740 |  |
| kolmo -m hc | 47,625,054 | 4.450 | 8.67 | 1634 | 348 |  |
| brotli 1.2.0 -11 | 50,498,416 | 4.197 | 1.11 | 678 | 164 |  |
| xz 5.8.4 -9 | 48,774,884 | 4.345 | 5.39 | 174 | 953 |  |
| lzma 26.03 -9 | 48,682,396 | 4.354 | 4.87 | 175 | 2178 |  |
| kolmo -m ratio | 40,688,158 | 5.209 | 8.75 | 118 | 1344 |  |
| kolmo -m max -l 1 | 43,967,208 | 4.821 | 54.3 | 73.7 | 914 |  |
| bsc 3.3.12 -m0 -e2 | 48,743,652 | 4.348 | 37.7 | 34.3 | 992 |  |
| bsc 3.3.12 -m0 -e1 | 49,295,078 | 4.300 | 49.0 | 46.7 | 992 |  |
| bzip3 1.5.4 -5 | 47,253,670 | 4.485 | 15.3 | 27.5 | 104 |  |
| bzip3 1.5.4 -10 | 48,754,055 | 4.347 | 15.3 | 22.1 | 1308 |  |
| ppmd8 26.03 -4 | 51,258,616 | 4.135 | 25.5 | 23.5 | 8 |  |
| kanzi 2.6.0 -7 | 47,308,869 | 4.480 | 27.3 | 31.9 | 280 |  |
| kanzi 2.6.0 -9 | 41,856,057 | 5.064 | 6.54 | 5.80 | 1043 |  |
| zpaq 7.15 -5 | 40,485,321 | 5.235 | 0.76 | 0.72 | 363 |  |
| kolmo -x -l 1 | 38,132,579 | 5.558 | 1.42 | 1.37 | 5530 |  |
| kolmo -x | 35,770,985 | 5.925 | 0.78 | 0.76 | 6085 |  |
| kolmo -x -l 2 | 35,770,985 | 5.925 | 0.78 | 0.76 | 6671 |  |

## Areas 1 to 9: Kolmo against the leader of each area

The first line of each area is the pair fixed before measuring; the lines under it are shown for context.

| # | Area | Kolmo setting | Rival | Input | Size | Compress speed | Decompress speed | Standing | Files where Kolmo is smaller / within 1% / larger |
|---|---|---|---|---|---|---|---|---|---|
| 1 | Lowest latency | kolmo -m speed | lz4 | silesia.tar | 0.4% smaller | 0.99× | 0.87× | Behind: same size, slower to decompress |  |
| 1 | Lowest latency | kolmo -m speed | lz4 | 35 files | 0.1% smaller | 0.97× | 0.92× | Level | 2 / 26 / 7 |
|  |  | kolmo -m speed | lzav -1 | silesia.tar | 20.2% larger | 1.13× | 1.20× | Behind: larger, though faster to compress and decompress |  |
|  |  | kolmo -m speed | zxc -1 | silesia.tar | 23.6% smaller | 0.89× | 0.33× | Trade: smaller, slower to compress and decompress |  |
|  |  | kolmo -m speed | zxc -3 | silesia.tar | 0.6% larger | 2.92× | 0.44× | Same size: faster to compress, slower to decompress |  |
|  |  | kolmo -m speed | snappy | silesia.tar | 1.5% smaller | 1.23× | 1.48× | Ahead |  |
|  |  | kolmo -m speed | lzfse | silesia.tar | 47.7% larger | 5.17× | 3.72× | Behind: larger, though faster to compress and decompress |  |
|  |  | kolmo -m rt | lz4 | silesia.tar | 1.6% smaller | 0.90× | 0.70× | Trade: smaller, slower to decompress |  |
| 2 | Fast general purpose | kolmo -m fast -l 1 -Q | zstd -1 | silesia.tar | 3.1% smaller | 1.01× | 0.97× | Ahead |  |
| 2 | Fast general purpose | kolmo -m fast -l 1 -Q | zstd -1 | 35 files | 5.0% smaller | 0.99× | 1.03× | Ahead | 4 / 27 / 4 |
|  |  | kolmo -m fast -l 1 -q | zstd -1 | silesia.tar | 12.2% smaller | 0.84× | 0.90× | Trade: smaller, slower to compress and decompress |  |
| 3 | Balanced default | kolmo -m balanced | zstd -3 | silesia.tar | 11.9% smaller | 0.89× | 0.85× | Trade: smaller, slower to compress and decompress |  |
| 3 | Balanced default | kolmo -m balanced | zstd -3 | 35 files | 15.7% smaller | 0.76× | 0.82× | Trade: smaller, slower to compress and decompress | 24 / 6 / 5 |
|  |  | kolmo -m fast -q | zstd -3 | silesia.tar | 10.7% smaller | 0.94× | 0.90× | Ahead |  |
| 4 | DEFLATE | kolmo -m balanced | libdeflate -6 | silesia.tar | 13.7% smaller | 2.72× | 1.27× | Ahead |  |
| 4 | DEFLATE | kolmo -m balanced | libdeflate -6 | 35 files | 17.4% smaller | 2.94× | 1.55× | Ahead | 31 / 2 / 2 |
|  |  | kolmo -m balanced | zlib-ng -6 | silesia.tar | 15.5% smaller | 3.29× | 2.45× | Ahead |  |
| 5 | Brotli | kolmo -m balanced | brotli -5 | silesia.tar | 2.2% smaller | 4.14× | 2.58× | Ahead |  |
| 5 | Brotli | kolmo -m balanced | brotli -5 | 35 files | 9.2% smaller | 3.82× | 2.75× | Ahead | 23 / 1 / 11 |
|  |  | kolmo -m hc | brotli -11 | silesia.tar | 5.7% smaller | 7.81× | 2.41× | Ahead |  |
|  |  | kolmo -m hc | brotli -11 | 35 files | 8.9% smaller | 8.36× | 3.81× | Ahead | 24 / 3 / 8 |
|  |  | kolmo -m eco | brotli -5 | silesia.tar | 15.0% smaller | 0.33× | 1.03× | Trade: smaller, slower to compress |  |
|  |  | kolmo -m eco | brotli -5 | 35 files | 16.8% smaller | 0.32× | 1.19× | Trade: smaller, slower to compress | 33 / 2 / 0 |
| 6 | High compression, quick decompression | kolmo -m hc | zstd -19 | silesia.tar | 9.9% smaller | 1.39× | 0.72× | Trade: smaller, slower to decompress |  |
| 6 | High compression, quick decompression | kolmo -m hc | zstd -19 | 35 files | 13.6% smaller | 1.47× | 0.83× | Trade: smaller, slower to decompress | 26 / 4 / 5 |
|  |  | kolmo -m hc | zstd -22 | silesia.tar | 8.9% smaller | 2.10× | 0.78× | Trade: smaller, slower to decompress |  |
|  |  | kolmo -m hc -l 9 | zstd -9 | silesia.tar | 11.4% smaller | 0.94× | 0.71× | Trade: smaller, slower to decompress |  |
|  |  | kolmo -m hc -l 9 | zstd -9 | 35 files | 15.0% smaller | 0.95× | 0.78× | Trade: smaller, slower to decompress | 27 / 6 / 2 |
| 7 | LZMA2 | kolmo -m ratio | xz -9 | silesia.tar | 16.6% smaller | 1.62× | 0.67× | Trade: smaller, slower to decompress |  |
| 7 | LZMA2 | kolmo -m ratio | xz -9 | 35 files | 11.6% smaller | 2.29× | 0.78× | Trade: smaller, slower to decompress | 31 / 2 / 2 |
|  |  | kolmo -m ratio | lzma -9 | silesia.tar | 16.4% smaller | 1.80× | 0.67× | Trade: smaller, slower to decompress |  |
|  |  | kolmo -m hc | xz -9 | silesia.tar | 2.4% smaller | 1.61× | 9.37× | Ahead |  |
|  |  | kolmo -m hc | xz -9 | 35 files | 3.9% smaller | 1.92× | 12.0× | Ahead | 24 / 2 / 9 |
| 8 | Strong practical compressors | kolmo -m ratio | bsc -m0 -e2 | silesia.tar | 16.5% smaller | 0.23× | 3.43× | Trade: smaller, slower to compress |  |
| 8 | Strong practical compressors | kolmo -m ratio | bsc -m0 -e2 | 35 files | 11.8% smaller | 0.30× | 3.73× | Trade: smaller, slower to compress | 23 / 5 / 7 |
|  |  | kolmo -m ratio | bsc -m0 -e1 | silesia.tar | 17.5% smaller | 0.18× | 2.52× | Trade: smaller, slower to compress |  |
|  |  | kolmo -m ratio | bzip3 -5 | silesia.tar | 13.9% smaller | 0.57× | 4.28× | Trade: smaller, slower to compress |  |
|  |  | kolmo -m ratio | bzip3 -5 | 35 files | 12.0% smaller | 0.71× | 4.96× | Trade: smaller, slower to compress | 26 / 4 / 5 |
|  |  | kolmo -m ratio | bzip3 -10 | silesia.tar | 16.5% smaller | 0.57× | 5.32× | Trade: smaller, slower to compress |  |
|  |  | kolmo -m ratio | kanzi -9 | silesia.tar | 2.8% smaller | 1.34× | 20.3× | Ahead |  |
|  |  | kolmo -m ratio | kanzi -7 | silesia.tar | 14.0% smaller | 0.32× | 3.69× | Trade: smaller, slower to compress |  |
|  |  | kolmo -m ratio | ppmd8 -4 | silesia.tar | 20.6% smaller | 0.34× | 4.99× | Trade: smaller, slower to compress |  |
|  |  | kolmo -m max -l 1 | bsc -m0 -e2 | silesia.tar | 9.8% smaller | 1.44× | 2.15× | Ahead |  |
| 9 | Extreme ratio | kolmo -x | zpaq -5 | silesia.tar | 11.6% smaller | 1.03× | 1.06× | Ahead |  |
|  |  | kolmo -x -l 2 | zpaq -5 | silesia.tar | 11.6% smaller | 1.03× | 1.06× | Ahead |  |
|  |  | kolmo -x -l 1 | zpaq -5 | silesia.tar | 5.8% smaller | 1.87× | 1.90× | Ahead |  |
|  |  | kolmo -x | kanzi -9 | silesia.tar | 14.5% smaller | 0.12× | 0.13× | Trade: smaller, slower to compress and decompress |  |

## A2 by data type

`-m balanced` is set against zstd -3. `-m ratio` and `-x` are set against the general rival that is smallest on the group (of zstd -19, brotli -11, xz -9, bsc -m0 -e2 and bzip3 with 16 MiB blocks).

| Data type (files, size) | `-m balanced` against zstd -3: size; compress / decompress speed | Smallest general rival | `-m ratio` against it | `-x` against it |
|---|---|---|---|---|
| Plain text (4, 88.3 MB) | 0.6% larger; 1.00× / 0.83× | bsc -m0 -e2 | 0.9% larger; 1.22× / 2.02× | 13.0% smaller; 0.013× / 0.012× |
| Executables and mixed (3, 79.0 MB) | 0.6% smaller; 0.74× / 0.92× | xz -9 | 0.5% larger; 0.95× / 0.97× | 14.6% smaller; 0.14× / 0.0058× |
| Database and markup (3, 49.0 MB) | 10.9% smaller; 0.63× / 0.56× | bsc -m0 -e2 | 1.9% larger; 0.20× / 2.15× | 5.3% smaller; 0.011× / 0.011× |
| JSON (2, 23.7 MB) | 38.5% smaller; 0.39× / 0.63× | bsc -m0 -e2 | 26.1% smaller; 1.52× / 2.73× | 47.4% smaller; 0.018× / 0.013× |
| Log (1, 4.5 MB) | 29.9% smaller; 0.39× / 0.52× | brotli -11 | 19.9% smaller; 18.8× / 0.30× | 39.6% smaller; 0.50× / 0.0004× |
| DNA text (1, 51.8 MB) | same size; 0.73× / 0.93× | bzip3 -5 | 1.5% larger; 3.35× / 3.46× | 10.1% smaller; 0.036× / 0.027× |
| 16-bit medical images (2, 18.4 MB) | 38.0% smaller; 1.11× / 0.69× | bzip3 -5 | 2.5% smaller; 0.66× / 3.61× | 4.8% smaller; 0.091× / 0.046× |
| Star catalogue (1, 7.3 MB) | 58.3% smaller; 0.75× / 1.50× | xz -9 | 50.5% smaller; 2.25× / 2.16× | 52.0% smaller; 0.25× / 0.027× |
| Tables (CSV) (4, 39.9 MB) | 64.0% smaller; 0.71× / 0.45× | bzip3 -5 | 56.2% smaller; 0.56× / 8.45× | 56.7% smaller; 0.080× / 0.062× |
| Raw 8-bit images (2, 1.0 MB) | 30.5% smaller; 0.38× / 0.28× | bzip3 -5 | 9.3% smaller; 0.46× / 1.87× | 22.4% smaller; 0.066× / 0.043× |
| Integer arrays (3, 0.3 MB) | 40.5% smaller; 0.066× / 1.48× | xz -9 | 34.8% smaller; 0.33× / 1.81× | 35.0% smaller; 0.041× / 0.018× |
| NumPy arrays (2, 24.9 MB) | 39.7% smaller; 0.57× / 0.63× | bsc -m0 -e2 | 17.2% smaller; 0.58× / 1.98× | 26.6% smaller; 0.038× / 0.19× |
| Pickle (1, 24.2 MB) | 43.2% smaller; 0.68× / 0.62× | bsc -m0 -e2 | 22.7% smaller; 1.75× / 2.02× | 31.3% smaller; 0.052× / 4.69× |
| Model weights (6, 84.8 MB) | 12.4% smaller; 0.66× / 1.63× | xz -9 | 0.5% smaller; 1.41× / 1.34× | 0.8% smaller; 0.22× / 0.042× |

### A2, every setting by data type

| Data type | Setting | Compressed bytes | Ratio | Compress MB/s | Decompress MB/s | Mark |
|---|---|---|---|---|---|---|
| Plain text | lz4 | 48,281,154 | 1.828 | 814 | 5529 |  |
| Plain text | kolmo -m speed | 48,583,300 | 1.817 | 792 | 5106 |  |
| Plain text | zstd -1 | 28,595,108 | 3.087 | 693 | 2358 |  |
| Plain text | kolmo -m fast -l 1 -Q | 28,653,526 | 3.081 | 687 | 2271 |  |
| Plain text | zstd -3 | 23,478,592 | 3.760 | 446 | 2302 |  |
| Plain text | kolmo -m balanced | 23,609,755 | 3.739 | 447 | 1909 |  |
| Plain text | kolmo -m balanced --native-leaf | 23,609,755 | 3.739 | 446 | 1905 |  |
| Plain text | libdeflate -6 | 26,923,273 | 3.279 | 144 | 1344 |  |
| Plain text | brotli -5 | 21,135,703 | 4.177 | 108 | 922 |  |
| Plain text | kolmo -m eco | 18,730,867 | 4.713 | 35.0 | 970 |  |
| Plain text | zstd -9 | 19,790,195 | 4.461 | 109 | 2780 |  |
| Plain text | kolmo -m hc -l 9 | 19,640,365 | 4.495 | 97.5 | 2149 |  |
| Plain text | zstd -19 | 16,713,830 | 5.282 | 4.79 | 3042 |  |
| Plain text | kolmo -m hc | 16,920,059 | 5.217 | 5.39 | 2256 |  |
| Plain text | brotli -11 | 17,067,061 | 5.172 | 1.15 | 1071 |  |
| Plain text | xz -9 | 16,164,636 | 5.461 | 3.88 | 273 |  |
| Plain text | bsc -m0 -e2 | 12,318,388 | 7.166 | 48.7 | 49.3 |  |
| Plain text | bzip3 -5 | 13,083,943 | 6.747 | 14.2 | 24.4 |  |
| Plain text | kolmo -m ratio | 12,424,374 | 7.105 | 59.2 | 99.7 |  |
| Plain text | kolmo -x | 10,719,991 | 8.235 | 0.62 | 0.60 |  |
| Executables and mixed | lz4 | 38,344,920 | 2.060 | 1084 | 6680 |  |
| Executables and mixed | kolmo -m speed | 38,509,388 | 2.051 | 1048 | 6156 |  |
| Executables and mixed | zstd -1 | 29,049,327 | 2.719 | 862 | 2154 |  |
| Executables and mixed | kolmo -m fast -l 1 -Q | 29,088,824 | 2.715 | 801 | 2078 |  |
| Executables and mixed | zstd -3 | 26,308,559 | 3.002 | 589 | 2092 |  |
| Executables and mixed | kolmo -m balanced | 26,156,836 | 3.019 | 435 | 1917 |  |
| Executables and mixed | kolmo -m balanced --native-leaf | 26,156,836 | 3.019 | 429 | 1917 |  |
| Executables and mixed | libdeflate -6 | 27,214,918 | 2.902 | 173 | 1290 |  |
| Executables and mixed | brotli -5 | 23,785,195 | 3.321 | 115 | 641 |  |
| Executables and mixed | kolmo -m eco | 22,970,845 | 3.438 | 37.8 | 682 |  |
| Executables and mixed | zstd -9 | 23,870,482 | 3.309 | 138 | 2238 |  |
| Executables and mixed | kolmo -m hc -l 9 | 23,747,438 | 3.326 | 126 | 2047 |  |
| Executables and mixed | zstd -19 | 21,558,551 | 3.663 | 7.68 | 1857 |  |
| Executables and mixed | kolmo -m hc | 21,625,883 | 3.652 | 8.96 | 1694 |  |
| Executables and mixed | brotli -11 | 20,381,309 | 3.875 | 0.94 | 555 |  |
| Executables and mixed | xz -9 | 19,564,592 | 4.037 | 6.00 | 148 |  |
| Executables and mixed | bsc -m0 -e2 | 22,376,176 | 3.530 | 36.6 | 31.4 |  |
| Executables and mixed | bzip3 -5 | 22,384,532 | 3.528 | 15.5 | 25.9 |  |
| Executables and mixed | kolmo -m ratio | 19,668,675 | 4.015 | 5.69 | 144 |  |
| Executables and mixed | kolmo -x | 16,717,750 | 4.724 | 0.84 | 0.87 |  |
| Database and markup | lz4 | 11,845,408 | 4.135 | 1542 | 8149 |  |
| Database and markup | kolmo -m speed | 12,045,370 | 4.067 | 1493 | 8025 |  |
| Database and markup | zstd -1 | 7,271,714 | 6.736 | 1315 | 3672 |  |
| Database and markup | kolmo -m fast -l 1 -Q | 7,316,348 | 6.695 | 1287 | 3421 |  |
| Database and markup | zstd -3 | 6,970,599 | 7.027 | 1072 | 3679 |  |
| Database and markup | kolmo -m balanced | 6,214,191 | 7.883 | 673 | 2059 |  |
| Database and markup | kolmo -m balanced --native-leaf | 6,214,191 | 7.883 | 672 | 2061 |  |
| Database and markup | libdeflate -6 | 7,436,412 | 6.587 | 317 | 3273 |  |
| Database and markup | brotli -5 | 6,053,428 | 8.092 | 201 | 1629 |  |
| Database and markup | kolmo -m eco | 5,425,866 | 9.028 | 68.9 | 1029 |  |
| Database and markup | zstd -9 | 6,106,160 | 8.022 | 215 | 4628 |  |
| Database and markup | kolmo -m hc -l 9 | 5,638,828 | 8.687 | 170 | 1710 |  |
| Database and markup | zstd -19 | 5,218,098 | 9.387 | 5.84 | 5078 |  |
| Database and markup | kolmo -m hc | 5,189,672 | 9.439 | 16.1 | 1698 |  |
| Database and markup | brotli -11 | 4,871,535 | 10.055 | 1.27 | 1536 |  |
| Database and markup | xz -9 | 5,042,052 | 9.715 | 8.36 | 367 |  |
| Database and markup | bsc -m0 -e2 | 3,828,242 | 12.796 | 65.4 | 70.9 |  |
| Database and markup | bzip3 -5 | 4,093,699 | 11.966 | 21.9 | 44.5 |  |
| Database and markup | kolmo -m ratio | 3,901,102 | 12.557 | 13.3 | 152 |  |
| Database and markup | kolmo -x | 3,625,590 | 13.511 | 0.73 | 0.75 |  |
| JSON | lz4 | 2,789,806 | 8.508 | 2288 | 11068 |  |
| JSON | kolmo -m speed | 2,969,047 | 7.994 | 2173 | 11423 |  |
| JSON | zstd -1 | 1,536,796 | 15.445 | 2072 | 6278 |  |
| JSON | kolmo -m fast -l 1 -Q | 1,544,523 | 15.367 | 1999 | 5616 |  |
| JSON | zstd -3 | 1,579,443 | 15.028 | 1737 | 6118 |  |
| JSON | kolmo -m balanced | 971,489 | 24.432 | 674 | 3856 |  |
| JSON | kolmo -m balanced --native-leaf | 971,489 | 24.432 | 677 | 3702 |  |
| JSON | libdeflate -6 | 1,433,992 | 16.552 | 511 | 6660 |  |
| JSON | brotli -5 | 1,190,432 | 19.938 | 333 | 3089 |  |
| JSON | kolmo -m eco | 767,364 | 30.931 | 186 | 2193 |  |
| JSON | zstd -9 | 1,104,035 | 21.499 | 366 | 7973 |  |
| JSON | kolmo -m hc -l 9 | 780,311 | 30.418 | 285 | 2996 |  |
| JSON | zstd -19 | 888,890 | 26.702 | 6.92 | 9439 |  |
| JSON | kolmo -m hc | 689,653 | 34.416 | 38.8 | 2838 |  |
| JSON | brotli -11 | 822,355 | 28.863 | 2.05 | 3236 |  |
| JSON | xz -9 | 976,068 | 24.317 | 13.7 | 756 |  |
| JSON | bsc -m0 -e2 | 818,922 | 28.984 | 142 | 195 |  |
| JSON | bzip3 -5 | 820,922 | 28.913 | 35.3 | 87.5 |  |
| JSON | kolmo -m ratio | 605,330 | 39.210 | 217 | 534 |  |
| JSON | kolmo -x | 430,378 | 55.150 | 2.54 | 2.64 |  |
| Log | lz4 | 757,595 | 5.886 | 2159 | 9925 |  |
| Log | kolmo -m speed | 768,456 | 5.802 | 1988 | 11519 |  |
| Log | zstd -1 | 437,640 | 10.188 | 1667 | 4670 |  |
| Log | kolmo -m fast -l 1 -Q | 445,590 | 10.007 | 1576 | 4294 |  |
| Log | zstd -3 | 451,445 | 9.877 | 1293 | 4339 |  |
| Log | kolmo -m balanced | 316,254 | 14.099 | 499 | 2270 |  |
| Log | kolmo -m balanced --native-leaf | 316,254 | 14.099 | 499 | 2264 |  |
| Log | libdeflate -6 | 444,981 | 10.020 | 393 | 4133 |  |
| Log | brotli -5 | 378,678 | 11.775 | 221 | 1640 |  |
| Log | kolmo -m eco | 275,583 | 16.180 | 149 | 1251 |  |
| Log | zstd -9 | 378,746 | 11.773 | 256 | 5293 |  |
| Log | kolmo -m hc -l 9 | 283,917 | 15.705 | 258 | 2297 |  |
| Log | zstd -19 | 330,712 | 13.483 | 6.27 | 5320 |  |
| Log | kolmo -m hc | 266,825 | 16.711 | 34.3 | 2153 |  |
| Log | brotli -11 | 307,364 | 14.507 | 1.14 | 1598 |  |
| Log | xz -9 | 323,564 | 13.781 | 15.0 | 405 |  |
| Log | bsc -m0 -e2 | 355,158 | 12.555 | 86.3 | 109 |  |
| Log | bzip3 -5 | 355,014 | 12.560 | 31.9 | 71.3 |  |
| Log | kolmo -m ratio | 246,107 | 18.118 | 21.4 | 473 |  |
| Log | kolmo -x | 185,557 | 24.030 | 0.57 | 0.67 |  |
| DNA text | lz4 | 21,921,382 | 2.365 | 783 | 4264 |  |
| DNA text | kolmo -m speed | 22,076,570 | 2.348 | 763 | 4039 |  |
| DNA text | zstd -1 | 12,882,833 | 4.024 | 629 | 2239 |  |
| DNA text | kolmo -m fast -l 1 -Q | 12,921,974 | 4.011 | 623 | 2113 |  |
| DNA text | zstd -3 | 12,721,555 | 4.075 | 486 | 2206 |  |
| DNA text | kolmo -m balanced | 12,724,397 | 4.074 | 356 | 2062 |  |
| DNA text | kolmo -m balanced --native-leaf | 12,724,397 | 4.074 | 356 | 2054 |  |
| DNA text | libdeflate -6 | 12,353,056 | 4.196 | 90.6 | 1378 |  |
| DNA text | brotli -5 | 11,636,416 | 4.455 | 83.9 | 806 |  |
| DNA text | kolmo -m eco | 11,131,261 | 4.657 | 17.2 | 935 |  |
| DNA text | zstd -9 | 11,842,311 | 4.377 | 93.6 | 2181 |  |
| DNA text | kolmo -m hc -l 9 | 11,842,085 | 4.377 | 85.4 | 2095 |  |
| DNA text | zstd -19 | 9,886,686 | 5.243 | 3.94 | 2821 |  |
| DNA text | kolmo -m hc | 9,988,697 | 5.189 | 4.48 | 2542 |  |
| DNA text | brotli -11 | 9,833,584 | 5.271 | 1.01 | 529 |  |
| DNA text | xz -9 | 9,505,144 | 5.453 | 2.56 | 237 |  |
| DNA text | bsc -m0 -e2 | 9,136,998 | 5.673 | 48.4 | 44.2 |  |
| DNA text | bzip3 -5 | 9,076,308 | 5.711 | 18.1 | 24.6 |  |
| DNA text | kolmo -m ratio | 9,210,838 | 5.628 | 60.8 | 85.3 |  |
| DNA text | kolmo -x | 8,158,539 | 6.353 | 0.66 | 0.66 |  |
| 16-bit medical images | lz4 | 13,802,669 | 1.336 | 1590 | 10019 |  |
| 16-bit medical images | kolmo -m speed | 12,816,078 | 1.439 | 2120 | 5892 |  |
| 16-bit medical images | zstd -1 | 10,585,527 | 1.742 | 877 | 1843 |  |
| 16-bit medical images | kolmo -m fast -l 1 -Q | 8,205,453 | 2.248 | 1098 | 1915 |  |
| 16-bit medical images | zstd -3 | 9,632,734 | 1.915 | 384 | 1565 |  |
| 16-bit medical images | kolmo -m balanced | 5,973,954 | 3.088 | 428 | 1073 |  |
| 16-bit medical images | kolmo -m balanced --native-leaf | 5,973,954 | 3.088 | 334 | 1075 |  |
| 16-bit medical images | libdeflate -6 | 9,647,082 | 1.912 | 157 | 965 |  |
| 16-bit medical images | brotli -5 | 8,760,048 | 2.106 | 91.3 | 463 |  |
| 16-bit medical images | kolmo -m eco | 5,890,805 | 3.131 | 56.1 | 478 |  |
| 16-bit medical images | zstd -9 | 8,677,794 | 2.126 | 105 | 1616 |  |
| 16-bit medical images | kolmo -m hc -l 9 | 5,926,422 | 3.112 | 178 | 1031 |  |
| 16-bit medical images | zstd -19 | 8,236,967 | 2.239 | 8.14 | 1519 |  |
| 16-bit medical images | kolmo -m hc | 5,907,152 | 3.122 | 21.9 | 1063 |  |
| 16-bit medical images | brotli -11 | 7,512,653 | 2.455 | 0.98 | 379 |  |
| 16-bit medical images | xz -9 | 7,240,140 | 2.548 | 6.55 | 93.5 |  |
| 16-bit medical images | bsc -m0 -e2 | 5,962,560 | 3.093 | 30.4 | 30.8 |  |
| 16-bit medical images | bzip3 -5 | 5,777,081 | 3.193 | 11.7 | 22.3 |  |
| 16-bit medical images | kolmo -m ratio | 5,630,849 | 3.276 | 7.78 | 80.7 |  |
| 16-bit medical images | kolmo -x | 5,501,818 | 3.352 | 1.07 | 1.03 |  |
| Star catalogue | lz4 | 6,790,191 | 1.068 | 1178 | 10104 |  |
| Star catalogue | kolmo -m speed | 6,791,955 | 1.068 | 1190 | 8777 |  |
| Star catalogue | zstd -1 | 6,250,010 | 1.160 | 825 | 1625 |  |
| Star catalogue | kolmo -m fast -l 1 -Q | 6,258,329 | 1.159 | 772 | 1584 |  |
| Star catalogue | zstd -3 | 5,527,504 | 1.312 | 357 | 1329 |  |
| Star catalogue | kolmo -m balanced | 2,302,435 | 3.150 | 268 | 1988 |  |
| Star catalogue | kolmo -m balanced --native-leaf | 2,302,435 | 3.150 | 215 | 1987 |  |
| Star catalogue | libdeflate -6 | 5,335,405 | 1.359 | 110 | 784 |  |
| Star catalogue | brotli -5 | 5,261,213 | 1.378 | 74.1 | 361 |  |
| Star catalogue | kolmo -m eco | 2,213,186 | 3.277 | 43.5 | 805 |  |
| Star catalogue | zstd -9 | 5,215,722 | 1.390 | 108 | 1592 |  |
| Star catalogue | kolmo -m hc -l 9 | 2,236,061 | 3.243 | 83.3 | 2080 |  |
| Star catalogue | zstd -19 | 5,000,572 | 1.450 | 9.94 | 1520 |  |
| Star catalogue | kolmo -m hc | 2,214,262 | 3.275 | 23.2 | 1905 |  |
| Star catalogue | brotli -11 | 4,590,042 | 1.580 | 1.02 | 283 |  |
| Star catalogue | xz -9 | 4,415,072 | 1.643 | 6.67 | 64.4 |  |
| Star catalogue | bsc -m0 -e2 | 4,679,746 | 1.550 | 19.9 | 16.6 |  |
| Star catalogue | bzip3 -5 | 4,673,311 | 1.552 | 11.2 | 17.3 |  |
| Star catalogue | kolmo -m ratio | 2,184,759 | 3.319 | 15.0 | 139 |  |
| Star catalogue | kolmo -x | 2,119,979 | 3.421 | 1.69 | 1.77 |  |
| Tables (CSV) | lz4 | 19,516,419 | 2.045 | 825 | 5133 |  |
| Tables (CSV) | kolmo -m speed | 19,664,548 | 2.029 | 808 | 5058 |  |
| Tables (CSV) | zstd -1 | 10,957,454 | 3.642 | 820 | 2321 |  |
| Tables (CSV) | kolmo -m fast -l 1 -Q | 10,948,930 | 3.645 | 794 | 2231 |  |
| Tables (CSV) | zstd -3 | 11,981,130 | 3.331 | 411 | 1876 |  |
| Tables (CSV) | kolmo -m balanced | 4,318,232 | 9.241 | 293 | 836 |  |
| Tables (CSV) | kolmo -m balanced --native-leaf | 4,318,232 | 9.241 | 293 | 836 |  |
| Tables (CSV) | libdeflate -6 | 10,856,611 | 3.676 | 130 | 1312 |  |
| Tables (CSV) | brotli -5 | 10,791,251 | 3.698 | 84.7 | 715 |  |
| Tables (CSV) | kolmo -m eco | 3,786,964 | 10.538 | 92.4 | 601 |  |
| Tables (CSV) | zstd -9 | 10,606,365 | 3.762 | 93.5 | 2136 |  |
| Tables (CSV) | kolmo -m hc -l 9 | 3,873,914 | 10.301 | 137 | 849 |  |
| Tables (CSV) | zstd -19 | 9,168,576 | 4.353 | 4.86 | 2025 |  |
| Tables (CSV) | kolmo -m hc | 3,736,716 | 10.680 | 36.0 | 870 |  |
| Tables (CSV) | brotli -11 | 8,357,458 | 4.775 | 1.10 | 498 |  |
| Tables (CSV) | xz -9 | 8,419,120 | 4.740 | 4.89 | 151 |  |
| Tables (CSV) | bsc -m0 -e2 | 8,226,668 | 4.851 | 43.2 | 43.1 |  |
| Tables (CSV) | bzip3 -5 | 8,157,061 | 4.892 | 15.4 | 27.1 |  |
| Tables (CSV) | kolmo -m ratio | 3,569,682 | 11.179 | 8.63 | 229 |  |
| Tables (CSV) | kolmo -x | 3,529,106 | 11.308 | 1.23 | 1.70 |  |
| Raw 8-bit images | lz4 | 922,259 | 1.137 | 3373 | 20937 |  |
| Raw 8-bit images | kolmo -m speed | 922,448 | 1.137 | 2547 | 17661 |  |
| Raw 8-bit images | zstd -1 | 856,264 | 1.225 | 1233 | 2273 |  |
| Raw 8-bit images | kolmo -m fast -l 1 -Q | 856,368 | 1.224 | 1097 | 2189 |  |
| Raw 8-bit images | zstd -3 | 828,979 | 1.265 | 508 | 1960 |  |
| Raw 8-bit images | kolmo -m balanced | 576,124 | 1.820 | 194 | 541 |  |
| Raw 8-bit images | kolmo -m balanced --native-leaf | 576,124 | 1.820 | 73.9 | 541 |  |
| Raw 8-bit images | libdeflate -6 | 792,909 | 1.322 | 142 | 823 |  |
| Raw 8-bit images | brotli -5 | 799,485 | 1.312 | 96.4 | 361 |  |
| Raw 8-bit images | kolmo -m eco | 567,921 | 1.846 | 19.6 | 299 |  |
| Raw 8-bit images | zstd -9 | 817,103 | 1.283 | 140 | 2004 |  |
| Raw 8-bit images | kolmo -m hc -l 9 | 565,593 | 1.854 | 40.1 | 528 |  |
| Raw 8-bit images | zstd -19 | 751,812 | 1.395 | 19.0 | 1213 |  |
| Raw 8-bit images | kolmo -m hc | 557,615 | 1.880 | 9.80 | 555 |  |
| Raw 8-bit images | brotli -11 | 657,174 | 1.596 | 0.58 | 237 |  |
| Raw 8-bit images | xz -9 | 661,268 | 1.586 | 12.5 | 54.9 |  |
| Raw 8-bit images | bsc -m0 -e2 | 610,968 | 1.716 | 19.2 | 18.0 |  |
| Raw 8-bit images | bzip3 -5 | 566,992 | 1.849 | 12.9 | 21.1 |  |
| Raw 8-bit images | kolmo -m ratio | 514,448 | 2.038 | 5.88 | 39.5 |  |
| Raw 8-bit images | kolmo -x | 440,131 | 2.382 | 0.84 | 0.91 |  |
| Integer arrays | lz4 | 119,953 | 2.576 | 1777 | 7921 |  |
| Integer arrays | kolmo -m speed | 70,721 | 4.369 | 497 | 7348 |  |
| Integer arrays | zstd -1 | 76,901 | 4.018 | 1088 | 2394 |  |
| Integer arrays | kolmo -m fast -l 1 -Q | 54,363 | 5.684 | 494 | 4688 |  |
| Integer arrays | zstd -3 | 72,788 | 4.245 | 1071 | 2408 |  |
| Integer arrays | kolmo -m balanced | 43,313 | 7.134 | 71.2 | 3571 |  |
| Integer arrays | kolmo -m balanced --native-leaf | 43,313 | 7.134 | 32.5 | 3576 |  |
| Integer arrays | libdeflate -6 | 81,597 | 3.787 | 259 | 2182 |  |
| Integer arrays | brotli -5 | 64,719 | 4.775 | 168 | 1191 |  |
| Integer arrays | kolmo -m eco | 41,904 | 7.374 | 30.5 | 1705 |  |
| Integer arrays | zstd -9 | 65,578 | 4.712 | 131 | 3305 |  |
| Integer arrays | kolmo -m hc -l 9 | 42,296 | 7.306 | 42.3 | 3464 |  |
| Integer arrays | zstd -19 | 67,590 | 4.572 | 12.2 | 2405 |  |
| Integer arrays | kolmo -m hc | 41,506 | 7.445 | 6.87 | 3500 |  |
| Integer arrays | brotli -11 | 69,209 | 4.465 | 1.54 | 601 |  |
| Integer arrays | xz -9 | 61,920 | 4.990 | 18.4 | 194 |  |
| Integer arrays | bsc -m0 -e2 | 79,584 | 3.883 | 46.3 | 39.8 |  |
| Integer arrays | bzip3 -5 | 80,045 | 3.860 | 26.4 | 44.8 |  |
| Integer arrays | kolmo -m ratio | 40,398 | 7.649 | 5.97 | 351 |  |
| Integer arrays | kolmo -x | 40,255 | 7.676 | 0.76 | 3.53 |  |
| NumPy arrays | lz4 | 4,276,744 | 5.831 | 2276 | 12867 |  |
| NumPy arrays | kolmo -m speed | 4,278,017 | 5.829 | 2179 | 12504 |  |
| NumPy arrays | zstd -1 | 4,457,095 | 5.595 | 1565 | 3824 |  |
| NumPy arrays | kolmo -m fast -l 1 -Q | 4,467,324 | 5.582 | 1450 | 3557 |  |
| NumPy arrays | zstd -3 | 3,147,157 | 7.924 | 1245 | 4974 |  |
| NumPy arrays | kolmo -m balanced | 1,897,958 | 13.140 | 709 | 3124 |  |
| NumPy arrays | kolmo -m balanced --native-leaf | 1,897,958 | 13.140 | 603 | 3130 |  |
| NumPy arrays | libdeflate -6 | 3,188,477 | 7.821 | 407 | 3090 |  |
| NumPy arrays | brotli -5 | 2,859,848 | 8.720 | 235 | 1400 |  |
| NumPy arrays | kolmo -m eco | 1,747,149 | 14.274 | 159 | 1310 |  |
| NumPy arrays | zstd -9 | 2,875,805 | 8.672 | 270 | 5107 |  |
| NumPy arrays | kolmo -m hc -l 9 | 1,781,903 | 13.995 | 244 | 2724 |  |
| NumPy arrays | zstd -19 | 2,547,351 | 9.790 | 30.4 | 5425 |  |
| NumPy arrays | kolmo -m hc | 1,708,242 | 14.599 | 22.2 | 2931 |  |
| NumPy arrays | brotli -11 | 2,342,930 | 10.644 | 2.93 | 1509 |  |
| NumPy arrays | xz -9 | 2,119,332 | 11.767 | 20.3 | 455 |  |
| NumPy arrays | bsc -m0 -e2 | 1,991,958 | 12.519 | 123 | 119 |  |
| NumPy arrays | bzip3 -5 | 2,018,661 | 12.354 | 41.8 | 80.7 |  |
| NumPy arrays | kolmo -m ratio | 1,648,514 | 15.128 | 71.9 | 236 |  |
| NumPy arrays | kolmo -x | 1,462,859 | 17.048 | 4.70 | 22.1 |  |
| Pickle | lz4 | 3,711,764 | 6.511 | 2449 | 13451 |  |
| Pickle | kolmo -m speed | 3,714,382 | 6.506 | 2443 | 13150 |  |
| Pickle | zstd -1 | 4,025,789 | 6.003 | 1654 | 3988 |  |
| Pickle | kolmo -m fast -l 1 -Q | 4,039,202 | 5.983 | 1579 | 3716 |  |
| Pickle | zstd -3 | 2,790,332 | 8.661 | 1358 | 5284 |  |
| Pickle | kolmo -m balanced | 1,585,658 | 15.240 | 925 | 3274 |  |
| Pickle | kolmo -m balanced --native-leaf | 1,585,658 | 15.240 | 882 | 3254 |  |
| Pickle | libdeflate -6 | 2,835,439 | 8.523 | 447 | 3225 |  |
| Pickle | brotli -5 | 2,522,650 | 9.580 | 255 | 1478 |  |
| Pickle | kolmo -m eco | 1,416,621 | 17.059 | 198 | 1454 |  |
| Pickle | zstd -9 | 2,530,736 | 9.549 | 291 | 5432 |  |
| Pickle | kolmo -m hc -l 9 | 1,449,717 | 16.669 | 331 | 2921 |  |
| Pickle | zstd -19 | 2,243,021 | 10.774 | 32.4 | 5902 |  |
| Pickle | kolmo -m hc | 1,381,768 | 17.489 | 22.2 | 3254 |  |
| Pickle | brotli -11 | 2,142,159 | 11.281 | 3.04 | 1603 |  |
| Pickle | xz -9 | 1,997,832 | 12.096 | 21.9 | 454 |  |
| Pickle | bsc -m0 -e2 | 1,763,912 | 13.700 | 136 | 131 |  |
| Pickle | bzip3 -5 | 1,795,055 | 13.462 | 45.0 | 86.7 |  |
| Pickle | kolmo -m ratio | 1,363,471 | 17.724 | 238 | 264 |  |
| Pickle | kolmo -x | 1,211,156 | 19.953 | 7.10 | 612 |  |
| Model weights | lz4 | 85,091,479 | 0.997 | 35146 | 47605 |  |
| Model weights | kolmo -m speed | 84,721,757 | 1.001 | 12376 | 30563 |  |
| Model weights | zstd -1 | 69,660,039 | 1.217 | 1499 | 1893 |  |
| Model weights | kolmo -m fast -l 1 -Q | 62,509,194 | 1.357 | 1579 | 2610 |  |
| Model weights | zstd -3 | 70,116,732 | 1.210 | 1025 | 1848 |  |
| Model weights | kolmo -m balanced | 61,424,226 | 1.381 | 674 | 3004 |  |
| Model weights | kolmo -m balanced --native-leaf | 61,424,226 | 1.381 | 654 | 2962 |  |
| Model weights | libdeflate -6 | 70,683,373 | 1.200 | 141 | 606 |  |
| Model weights | brotli -5 | 67,813,421 | 1.251 | 156 | 362 |  |
| Model weights | kolmo -m eco | 60,731,740 | 1.396 | 33.3 | 713 |  |
| Model weights | zstd -9 | 69,462,256 | 1.221 | 209 | 1629 |  |
| Model weights | kolmo -m hc -l 9 | 61,017,965 | 1.390 | 177 | 3069 |  |
| Model weights | zstd -19 | 67,870,486 | 1.250 | 8.47 | 1384 |  |
| Model weights | kolmo -m hc | 59,860,058 | 1.417 | 11.5 | 3447 |  |
| Model weights | brotli -11 | 63,817,348 | 1.329 | 0.91 | 204 |  |
| Model weights | xz -9 | 58,850,100 | 1.441 | 3.85 | 76.4 |  |
| Model weights | bsc -m0 -e2 | 63,399,136 | 1.338 | 17.6 | 14.4 |  |
| Model weights | bzip3 -5 | 63,073,428 | 1.345 | 11.0 | 14.3 |  |
| Model weights | kolmo -m ratio | 58,572,652 | 1.448 | 5.44 | 102 |  |
| Model weights | kolmo -x | 58,376,590 | 1.453 | 0.85 | 3.22 |  |

### A2, the 12 Silesia files compressed one by one (the form the public Silesia table uses)

| Setting | Sum of the 12 compressed sizes | Ratio | Compress MB/s | Decompress MB/s | Mark |
|---|---|---|---|---|---|
| zstd -3 | 66,137,723 | 3.205 | 539 | 2199 |  |
| kolmo -m balanced | 58,487,839 | 3.624 | 447 | 1839 |  |
| zstd -19 | 52,891,946 | 4.007 | 6.22 | 2332 |  |
| kolmo -m hc | 47,871,778 | 4.427 | 8.93 | 1659 |  |
| brotli -11 | 50,333,347 | 4.211 | 1.08 | 679 |  |
| xz -9 | 48,795,480 | 4.343 | 5.45 | 173 |  |
| bsc -m0 -e2 | 46,536,700 | 4.554 | 40.7 | 38.0 |  |
| bzip3 -5 | 46,930,008 | 4.516 | 15.4 | 27.2 |  |
| kolmo -m ratio | 41,146,453 | 5.151 | 9.86 | 116 |  |
| kolmo -x | 36,290,170 | 5.840 | 0.75 | 0.76 |  |

### A2, file by file: compressed bytes

| File | Bytes | zstd -3 | kolmo -m balanced | zstd -19 | kolmo -m hc | brotli -11 | xz -9 | bsc -m0 -e2 | bzip3 -5 | kolmo -m ratio | kolmo -x |
|---|---|---|---|---|---|---|---|---|---|---|---|
| log_timestamps_ms.i64 | 64,344 | 6,880 | 2,628 | 6,542 | **2,360** | 6,176 | 6,096 | 11,194 | 11,905 | 2,372 | 2,372 |
| year.i32 | 93,508 | 1,168 | 902 | 855 | 729 | 834 | 952 | 832 | 686 | 661 | **518** |
| population.i64 | 151,152 | 64,740 | 39,783 | 60,193 | 38,417 | 62,199 | 54,872 | 67,558 | 67,454 | **37,365** | **37,365** |
| img_camera.u8 | 262,144 | 173,291 | 144,259 | 160,444 | 138,832 | 148,066 | 142,592 | 136,822 | 132,839 | 132,883 | **116,664** |
| img_astronaut.u8 | 786,432 | 655,688 | 431,865 | 591,368 | 418,783 | 509,108 | 518,676 | 474,146 | 434,153 | 381,565 | **323,467** |
| lfw_subset.npy | 1,000,080 | 386,048 | 321,037 | 313,577 | 303,200 | 267,444 | 255,376 | 239,808 | 238,334 | 245,200 | **203,747** |
| magika.gguf | 3,138,752 | 2,913,494 | 2,614,223 | 2,914,203 | 2,614,083 | 2,878,627 | 2,895,616 | 2,818,872 | 2,788,821 | **2,607,722** | **2,607,722** |
| magika.safetensors | 3,139,140 | 2,913,900 | 2,614,491 | 2,914,005 | 2,614,351 | 2,878,968 | 2,896,240 | 2,819,038 | 2,789,132 | **2,608,222** | **2,608,222** |
| magika.onnx | 3,163,737 | 2,918,312 | 2,669,294 | 2,918,201 | 2,645,422 | 2,881,520 | 2,898,720 | 2,823,766 | 2,794,848 | 2,644,257 | **2,640,821** |
| api.json | 3,737,294 | 327,475 | 365,846 | 222,527 | 221,589 | 205,139 | 222,140 | 210,538 | 206,953 | 213,374 | **132,674** |
| loghub_all.log | 4,458,875 | 451,445 | 316,254 | 330,712 | 266,825 | 307,364 | 323,564 | 355,158 | 355,014 | 246,107 | **185,557** |
| xml | 5,345,280 | 635,299 | 652,714 | 452,941 | 443,624 | 430,369 | 453,260 | 393,162 | 405,881 | 399,262 | **293,903** |
| ooffice | 6,152,192 | 3,126,146 | 2,793,733 | 2,595,003 | 2,246,133 | 2,479,448 | 2,426,816 | 2,559,090 | 2,526,886 | 2,093,384 | **1,674,777** |
| reymont | 6,627,202 | 1,937,973 | 1,967,300 | 1,348,458 | 1,348,510 | 1,336,162 | 1,317,152 | 982,346 | 980,463 | 994,018 | **862,412** |
| energy_test.csv | 6,922,645 | 2,075,357 | 735,707 | 1,662,596 | 664,573 | 1,575,646 | 1,616,168 | 1,463,742 | 1,422,261 | **631,255** | 635,318 |
| sao | 7,251,944 | 5,527,504 | 2,302,435 | 5,000,572 | 2,214,262 | 4,590,042 | 4,415,072 | 4,679,746 | 4,673,311 | 2,184,759 | **2,119,979** |
| wide_test.csv | 8,227,871 | 2,070,677 | 1,174,169 | 1,546,140 | 968,667 | 1,400,338 | 1,421,052 | 1,382,758 | 1,380,463 | 951,938 | **925,251** |
| x-ray | 8,474,240 | 6,085,208 | 3,657,342 | 5,129,823 | 3,649,636 | 4,686,258 | 4,489,868 | 3,756,566 | 3,657,087 | 3,616,667 | **3,561,093** |
| mr | 9,970,564 | 3,547,526 | 2,316,612 | 3,107,144 | 2,257,516 | 2,826,395 | 2,750,272 | 2,205,994 | 2,119,994 | 2,014,182 | **1,940,725** |
| osdb | 10,085,684 | 3,501,457 | 3,533,740 | 3,100,173 | 3,100,225 | 2,825,161 | 2,849,908 | **2,224,762** | 2,261,858 | 2,276,454 | 2,228,799 |
| dickens | 10,192,446 | 3,663,504 | 3,721,196 | 2,849,941 | 2,728,251 | 2,852,894 | 2,830,604 | 2,259,116 | 2,233,938 | 2,274,350 | **2,064,749** |
| owid_test.csv | 10,377,936 | 3,298,466 | 1,063,753 | 2,516,766 | 906,634 | 2,268,015 | 2,269,096 | 2,271,824 | 2,260,177 | 855,265 | **848,707** |
| llm_bf16_10mb.safetensors | 10,485,880 | 8,191,998 | 6,992,125 | 8,089,140 | 6,991,662 | 7,531,700 | 7,480,796 | 7,031,044 | 6,984,198 | **6,967,579** | **6,967,579** |
| owid-co2-data.csv | 14,377,942 | 4,536,630 | 1,344,603 | 3,443,074 | 1,196,842 | 3,113,459 | 3,112,804 | 3,108,344 | 3,094,160 | 1,131,224 | **1,119,830** |
| llm_nvfp4_17mb.safetensors | 17,695,172 | 16,746,014 | 16,749,257 | 16,642,589 | 16,646,154 | 16,613,294 | 16,697,164 | 16,602,576 | 16,573,330 | 16,692,289 | **16,499,663** |
| owid_arr.json | 19,997,939 | 1,251,968 | 605,643 | 666,363 | 468,064 | 617,216 | 753,928 | 608,384 | 613,969 | 391,956 | **297,704** |
| samba | 21,606,400 | 4,955,117 | 5,044,560 | 3,897,788 | 3,959,908 | 3,822,632 | 3,763,616 | 3,909,910 | 3,976,072 | 3,739,524 | **2,925,424** |
| owid_energy_f64.npy | 23,938,176 | 2,761,109 | 1,576,921 | 2,233,774 | 1,405,042 | 2,075,486 | 1,863,956 | 1,752,150 | 1,780,327 | 1,403,314 | **1,259,112** |
| owid_energy.pkl | 24,165,812 | 2,790,332 | 1,585,658 | 2,243,021 | 1,381,768 | 2,142,159 | 1,997,832 | 1,763,912 | 1,795,055 | 1,363,471 | **1,211,156** |
| ld30.txt | 29,997,097 | 5,780,265 | 5,769,332 | 3,836,072 | 3,985,250 | 4,089,253 | 3,631,012 | 2,628,412 | 3,082,558 | 2,663,306 | **2,394,958** |
| nci | 33,553,445 | 2,833,843 | 2,027,737 | 1,664,984 | 1,645,823 | 1,616,005 | 1,738,884 | 1,210,318 | 1,425,960 | 1,225,386 | **1,102,888** |
| webster | 41,458,703 | 12,096,850 | 12,151,927 | 8,679,359 | 8,858,048 | 8,788,752 | 8,385,868 | 6,448,514 | 6,786,984 | 6,492,700 | **5,397,872** |
| llm_bf16_47mb.safetensors | 47,186,040 | 36,433,014 | 29,784,836 | 34,392,348 | 28,348,386 | 31,033,239 | **25,981,564** | 31,303,840 | 31,143,099 | 27,052,583 | 27,052,583 |
| mozilla | 51,220,480 | 18,227,296 | 18,318,543 | 15,065,760 | 15,419,842 | 14,079,229 | 13,374,160 | 15,907,176 | 15,881,574 | 13,835,767 | **12,117,549** |
| chr22.fa | 51,834,845 | 12,721,555 | 12,724,397 | 9,886,686 | 9,988,697 | 9,833,584 | 9,505,144 | 9,136,998 | 9,076,308 | 9,210,838 | **8,158,539** |

### A2, inputs under 1 MB: compress / decompress MB/s of the fast settings

| File | Bytes | lz4 | kolmo -m speed | zstd -1 | kolmo -m fast -l 1 -Q | zstd -3 | kolmo -m balanced |
|---|---|---|---|---|---|---|---|
| log_timestamps_ms.i64 | 64,344 | 2570 / 7839 | 446 / 8680 | 1895 / 3880 | 458 / 3591 | 1935 / 3642 | 68.6 / 5063 |
| year.i32 | 93,508 | 17060 / 65574 | 756 / 71654 | 8501 / 24876 | 727 / 16870 | 8282 / 24594 | 78.3 / 5829 |
| population.i64 | 151,152 | 1055 / 5146 | 428 / 4535 | 632 / 1390 | 425 / 3560 | 619 / 1415 | 68.4 / 2615 |
| img_camera.u8 | 262,144 | 1718 / 8619 | 1354 / 8861 | 868 / 1667 | 755 / 1621 | 466 / 1419 | 167 / 337 |
| img_astronaut.u8 | 786,432 | 4970 / 39989 | 3607 / 26398 | 1435 / 2587 | 1292 / 2479 | 524 / 2245 | 205 / 679 |

## A3. Typed arrays (lzbench, in memory)

| File | Bytes | zstd -3 | kolmo -m balanced | kolmo -m balanced --native-leaf | kolmo -m balanced --typed-huf | zstd -19 | xz -9 | bsc -m0 -e2 | bzip3 -5 | kolmo -m ratio | kolmo -x | kolmo -x -l 2 | OpenZL 0.2.3 numeric profile |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| mitdb-100-ch0.i16 | 1,300,000 | 473,128 | 339,821 | 290,693 | 339,821 | 410,230 | 362,908 | 287,042 | 275,215 | 305,080 | 285,193 | 267,027 | 407,417 |
| mitdb-100-ch1.i16 | 1,300,000 | 470,382 | 333,162 | 333,162 | 333,162 | 406,864 | 359,532 | 287,448 | 276,444 | 300,194 | 286,545 | 265,384 | 404,089 |
| mitdb-100.i16 | 2,600,000 | 1,172,818 | 864,758 | 864,758 | 864,758 | 993,480 | 777,104 | 630,566 | 605,084 | 641,778 | 587,015 | 587,015 | 982,208 |
| mitdb-100-ch0.i32 | 2,600,000 | 627,565 | 316,683 | 292,394 | 316,683 | 449,113 | 363,700 | 287,658 | 275,900 | 305,080 | 294,711 | 267,027 | 457,623 |
| year.i32 | 93,508 | 1,168 | 902 | 902 | 902 | 855 | 952 | 832 | 686 | 661 | 518 | 518 | 936 |
| log_timestamps_ms.i64 | 64,344 | 6,880 | 2,628 | 2,628 | 2,628 | 6,542 | 6,096 | 11,194 | 11,905 | 2,372 | 2,372 | 2,082 | 4,125 |
| population.i64 | 151,152 | 64,740 | 39,783 | 39,783 | 39,783 | 60,193 | 54,872 | 67,558 | 67,454 | 37,365 | 37,365 | 36,617 | 46,039 |
| ecg_2ch_i16.npy | 2,600,128 | 1,172,801 | 864,881 | 864,881 | 1,295,136 | 993,622 | 777,560 | 630,706 | 605,379 | 641,932 | 587,080 | 587,080 |  |
| ecg_ch0_i16.npy | 1,300,128 | 473,118 | 339,974 | 290,784 | 316,710 | 411,008 | 363,172 | 287,168 | 275,582 | 305,172 | 285,265 | 267,119 |  |
| ecg_ch0_i32.npy | 2,600,128 | 627,591 | 316,774 | 292,485 | 316,710 | 449,515 | 363,412 | 287,780 | 276,117 | 305,172 | 294,786 | 267,119 |  |
| ecg_ch1_i16.npy | 1,300,128 | 470,536 | 333,319 | 333,319 | 308,126 | 406,880 | 359,452 | 287,578 | 276,712 | 300,286 | 286,614 | 265,476 |  |
| lfw_subset.npy | 1,000,080 | 386,048 | 321,037 | 321,037 | 321,037 | 313,577 | 255,376 | 239,808 | 238,334 | 245,200 | 203,747 | 203,747 |  |
| log_timestamps_ms_i64.npy | 64,472 | 6,959 | 2,717 | 2,717 | 5,740 | 6,618 | 6,168 | 11,286 | 11,994 | 2,464 | 2,464 | 2,174 |  |
| mr_u16.npy | 9,970,692 | 3,548,050 | 2,316,287 | 2,316,287 | 3,540,672 | 3,107,328 | 2,750,068 | 2,206,062 | 2,120,084 | 2,014,294 | 1,940,820 | 1,940,820 |  |
| owid_energy_f64.npy | 23,938,176 | 2,761,109 | 1,576,921 | 1,576,921 | 1,576,921 | 2,233,774 | 1,863,956 | 1,752,150 | 1,780,327 | 1,403,314 | 1,259,112 | 1,258,182 |  |
| population_i64.npy | 151,280 | 64,838 | 39,873 | 39,873 | 45,295 | 60,250 | 54,820 | 67,646 | 67,541 | 37,458 | 37,458 | 36,710 |  |
| xray_u16.npy | 8,474,368 | 6,084,395 | 3,657,595 | 3,657,595 | 4,274,198 | 5,122,323 | 4,489,672 | 3,756,762 | 3,657,196 | 3,616,738 | 3,561,247 | 3,561,247 |  |
| year_i32.npy | 93,636 | 1,249 | 992 | 992 | 1,301 | 954 | 1,024 | 948 | 780 | 754 | 618 | 618 |  |

| Setting, all 18 arrays | Compressed bytes | Ratio | Compress MB/s | Decompress MB/s | Mark |
|---|---|---|---|---|---|
| zstd -3 | 18,413,375 | 3.237 | 556 | 2231 |  |
| kolmo -m balanced | 11,668,107 | 5.108 | 330 | 1694 |  |
| kolmo -m balanced --native-leaf | 11,521,211 | 5.173 | 218 | 1586 |  |
| kolmo -m balanced --typed-huf | 13,899,583 | 4.288 | 474 | 2022 |  |
| zstd -19 | 15,433,126 | 3.862 | 11.0 | 2286 |  |
| xz -9 | 13,209,844 | 4.512 | 8.51 | 160 |  |
| bsc -m0 -e2 | 11,100,192 | 5.369 | 48.0 | 52.7 |  |
| bzip3 -5 | 10,822,734 | 5.507 | 16.9 | 35.7 |  |
| kolmo -m ratio | 10,465,314 | 5.695 | 13.0 | 129 |  |
| kolmo -x | 9,952,930 | 5.988 | 1.33 | 1.70 |  |
| kolmo -x -l 2 | 9,815,962 | 6.072 | 0.96 | 1.84 |  |
| kolmo control build -m balanced | 11,668,107 | 5.108 | 331 | 1580 |  |
| kolmo control build -m balanced --typed-huf | 13,899,583 | 4.288 | 476 | 1188 |  |

Against OpenZL's numeric profile (lzbench's OpenZL 0.2.3) on the 7 raw integer arrays:

| Setting | Size | Compress speed | Decompress speed | Standing |
|---|---|---|---|---|
| kolmo -m balanced | 17.6% smaller | 1.29× | 0.81× | Trade: smaller, slower to decompress |
| kolmo -m balanced --native-leaf | 20.8% smaller | 0.74× | 0.65× | Trade: smaller, slower to compress and decompress |
| kolmo -m ratio | 30.8% smaller | 0.074× | 0.066× | Trade: smaller, slower to compress and decompress |
| kolmo -x | 35.1% smaller | 0.0062× | 0.0005× | Trade: smaller, slower to compress and decompress |
| zstd -19 | 1.1% larger | 0.064× | 0.91× | Behind: larger |
| bsc -m0 -e2 | 31.7% smaller | 0.32× | 0.026× | Trade: smaller, slower to compress and decompress |

## S1. Specialists called in one Python process beside Kolmo's binding (one thread)

### Arrays of numbers: 10 files, 50,192,064 bytes

| Setting | Bytes | Compress MB/s | Decompress MB/s | Size against pcodec level 8 | Speed against it (compress / decompress) | Files where it is smaller | Mark |
|---|---|---|---|---|---|---|---|
| pcodec level 8 | 10,407,710 | 320 | 1843 |  |  |  |  |
| pcodec level 12 | 10,227,430 | 214 | 1805 | 1.7% smaller | 0.67× / 0.98× | 10 of 10 |  |
| kolmo -m balanced | 9,436,487 | 403 | 1551 | 9.3% smaller | 1.26× / 0.84× | 3 of 10 |  |
| kolmo -m balanced --native-leaf | 9,363,070 | 279 | 1489 | 10.0% smaller | 0.87× / 0.81× | 5 of 10 |  |
| kolmo -m balanced --typed-huf | 9,436,487 | 403 | 1549 | 9.3% smaller | 1.26× / 0.84× | 3 of 10 |  |
| kolmo -m hc | 9,101,432 | 21.4 | 1505 | 12.6% smaller | 0.067× / 0.82× | 4 of 10 |  |
| kolmo -m ratio | 8,571,699 | 14.0 | 127 | 17.6% smaller | 0.044× / 0.069× | 8 of 10 |  |
| kolmo -x | 8,171,851 | 1.55 | 1.95 | 21.5% smaller | 0.0048× / 0.0011× | 10 of 10 |  |
| kolmo -x -l 2 | 8,124,033 | 0.94 | 1.90 | 21.9% smaller | 0.0029× / 0.0010× | 10 of 10 |  |

Kolmo's bare library call (buffers allocated beforehand) against its Python binding, speed ratio compress / decompress: -m balanced 1.01× / 1.04×; -m balanced --native-leaf 1.01× / 1.04×; -m balanced --typed-huf 1.01× / 1.05×; -m hc 1.00× / 1.04×; -m ratio 1.00× / 1.00×; -x 1.01× / 1.00×; -x -l 2 1.03× / 1.00×.

| File | Setting | Bytes | Ratio | Compress MB/s | Decompress MB/s | Mark |
|---|---|---|---|---|---|---|
| lfw_subset.npy | pcodec level 8 | 280,490 | 3.565 | 216 | 1990 |  |
| lfw_subset.npy | pcodec level 12 | 278,253 | 3.594 | 89.9 | 1885 |  |
| lfw_subset.npy | kolmo -m balanced | 321,037 | 3.115 | 97.3 | 1618 |  |
| lfw_subset.npy | kolmo -m balanced --native-leaf | 321,037 | 3.115 | 96.9 | 1646 |  |
| lfw_subset.npy | kolmo -m balanced --typed-huf | 321,037 | 3.115 | 96.9 | 1645 |  |
| lfw_subset.npy | kolmo -m hc | 303,200 | 3.298 | 16.5 | 1546 |  |
| lfw_subset.npy | kolmo -m ratio | 245,200 | 4.079 | 4.07 | 67.0 |  |
| lfw_subset.npy | kolmo -x | 203,747 | 4.908 | 0.55 | 0.92 |  |
| lfw_subset.npy | kolmo -x -l 2 | 203,747 | 4.908 | 0.55 | 0.92 |  |
| log_timestamps_ms.i64 | pcodec level 8 | 2,549 | 25.243 | 658 | 4053 |  |
| log_timestamps_ms.i64 | pcodec level 12 | 2,530 | 25.432 | 559 | 2930 |  |
| log_timestamps_ms.i64 | kolmo -m balanced | 2,628 | 24.484 | 66.0 | 3851 |  |
| log_timestamps_ms.i64 | kolmo -m balanced --native-leaf | 2,628 | 24.484 | 40.4 | 3900 |  |
| log_timestamps_ms.i64 | kolmo -m balanced --typed-huf | 2,628 | 24.484 | 66.1 | 4185 |  |
| log_timestamps_ms.i64 | kolmo -m hc | 2,360 | 27.264 | 6.82 | 4555 |  |
| log_timestamps_ms.i64 | kolmo -m ratio | 2,372 | 27.126 | 5.77 | 1093 |  |
| log_timestamps_ms.i64 | kolmo -x | 2,372 | 27.126 | 1.01 | 1091 |  |
| log_timestamps_ms.i64 | kolmo -x -l 2 | 2,082 | 30.905 | 0.69 | 2.17 |  |
| mitdb-100-ch0.i16 | pcodec level 8 | 313,437 | 4.148 | 305 | 1604 |  |
| mitdb-100-ch0.i16 | pcodec level 12 | 312,434 | 4.161 | 286 | 1572 |  |
| mitdb-100-ch0.i16 | kolmo -m balanced | 339,821 | 3.826 | 166 | 1544 |  |
| mitdb-100-ch0.i16 | kolmo -m balanced --native-leaf | 290,693 | 4.472 | 54.8 | 958 |  |
| mitdb-100-ch0.i16 | kolmo -m balanced --typed-huf | 339,821 | 3.826 | 166 | 1525 |  |
| mitdb-100-ch0.i16 | kolmo -m hc | 316,579 | 4.106 | 15.5 | 2644 |  |
| mitdb-100-ch0.i16 | kolmo -m ratio | 305,080 | 4.261 | 12.9 | 161 |  |
| mitdb-100-ch0.i16 | kolmo -x | 285,193 | 4.558 | 1.03 | 1.05 |  |
| mitdb-100-ch0.i16 | kolmo -x -l 2 | 267,027 | 4.868 | 1.85 | 2.00 |  |
| mitdb-100-ch0.i32 | pcodec level 8 | 313,586 | 8.291 | 592 | 3000 |  |
| mitdb-100-ch0.i32 | pcodec level 12 | 312,599 | 8.317 | 556 | 2879 |  |
| mitdb-100-ch0.i32 | kolmo -m balanced | 316,683 | 8.210 | 148 | 4483 |  |
| mitdb-100-ch0.i32 | kolmo -m balanced --native-leaf | 292,394 | 8.892 | 98.1 | 1808 |  |
| mitdb-100-ch0.i32 | kolmo -m balanced --typed-huf | 316,683 | 8.210 | 149 | 4489 |  |
| mitdb-100-ch0.i32 | kolmo -m hc | 316,579 | 8.213 | 30.0 | 4618 |  |
| mitdb-100-ch0.i32 | kolmo -m ratio | 305,080 | 8.522 | 25.2 | 315 |  |
| mitdb-100-ch0.i32 | kolmo -x | 294,711 | 8.822 | 1.14 | 1.12 |  |
| mitdb-100-ch0.i32 | kolmo -x -l 2 | 267,027 | 9.737 | 3.67 | 3.94 |  |
| mitdb-100.i16 | pcodec level 8 | 621,315 | 4.185 | 89.5 | 716 |  |
| mitdb-100.i16 | pcodec level 12 | 618,684 | 4.202 | 83.6 | 691 |  |
| mitdb-100.i16 | kolmo -m balanced | 864,758 | 3.007 | 171 | 799 |  |
| mitdb-100.i16 | kolmo -m balanced --native-leaf | 864,758 | 3.007 | 124 | 799 |  |
| mitdb-100.i16 | kolmo -m balanced --typed-huf | 864,758 | 3.007 | 172 | 796 |  |
| mitdb-100.i16 | kolmo -m hc | 811,374 | 3.204 | 20.0 | 734 |  |
| mitdb-100.i16 | kolmo -m ratio | 641,778 | 4.051 | 4.58 | 67.6 |  |
| mitdb-100.i16 | kolmo -x | 587,015 | 4.429 | 0.45 | 0.97 |  |
| mitdb-100.i16 | kolmo -x -l 2 | 587,015 | 4.429 | 0.49 | 0.97 |  |
| mr | pcodec level 8 | 2,898,096 | 3.440 | 256 | 1365 |  |
| mr | pcodec level 12 | 2,885,679 | 3.455 | 226 | 1333 |  |
| mr | kolmo -m balanced | 2,316,612 | 4.304 | 373 | 797 |  |
| mr | kolmo -m balanced --native-leaf | 2,316,612 | 4.304 | 295 | 795 |  |
| mr | kolmo -m balanced --typed-huf | 2,316,612 | 4.304 | 372 | 795 |  |
| mr | kolmo -m hc | 2,257,516 | 4.417 | 14.4 | 757 |  |
| mr | kolmo -m ratio | 2,014,182 | 4.950 | 6.49 | 80.6 |  |
| mr | kolmo -x | 1,940,725 | 5.138 | 1.13 | 1.05 |  |
| mr | kolmo -x -l 2 | 1,940,725 | 5.138 | 1.06 | 1.06 |  |
| owid_energy_f64.npy | pcodec level 8 | 2,189,006 | 10.936 | 620 | 2742 |  |
| owid_energy_f64.npy | pcodec level 12 | 2,044,728 | 11.707 | 341 | 2711 |  |
| owid_energy_f64.npy | kolmo -m balanced | 1,576,921 | 15.180 | 936 | 2952 |  |
| owid_energy_f64.npy | kolmo -m balanced --native-leaf | 1,576,921 | 15.180 | 748 | 2973 |  |
| owid_energy_f64.npy | kolmo -m balanced --typed-huf | 1,576,921 | 15.180 | 932 | 2964 |  |
| owid_energy_f64.npy | kolmo -m hc | 1,405,042 | 17.037 | 22.6 | 2842 |  |
| owid_energy_f64.npy | kolmo -m ratio | 1,403,314 | 17.058 | 231 | 261 |  |
| owid_energy_f64.npy | kolmo -x | 1,259,112 | 19.012 | 6.84 | 570 |  |
| owid_energy_f64.npy | kolmo -x -l 2 | 1,258,182 | 19.026 | 0.92 | 8.35 |  |
| population.i64 | pcodec level 8 | 37,440 | 4.037 | 318 | 3973 |  |
| population.i64 | pcodec level 12 | 37,317 | 4.050 | 56.7 | 3557 |  |
| population.i64 | kolmo -m balanced | 39,783 | 3.799 | 66.2 | 2334 |  |
| population.i64 | kolmo -m balanced --native-leaf | 39,783 | 3.799 | 22.5 | 2331 |  |
| population.i64 | kolmo -m balanced --typed-huf | 39,783 | 3.799 | 66.3 | 2325 |  |
| population.i64 | kolmo -m hc | 38,417 | 3.935 | 5.27 | 2241 |  |
| population.i64 | kolmo -m ratio | 37,365 | 4.045 | 4.81 | 184 |  |
| population.i64 | kolmo -x | 37,365 | 4.045 | 0.84 | 184 |  |
| population.i64 | kolmo -x -l 2 | 36,617 | 4.128 | 0.46 | 0.95 |  |
| x-ray | pcodec level 8 | 3,751,193 | 2.259 | 233 | 1600 |  |
| x-ray | pcodec level 12 | 3,734,638 | 2.269 | 129 | 1578 |  |
| x-ray | kolmo -m balanced | 3,657,342 | 2.317 | 502 | 1334 |  |
| x-ray | kolmo -m balanced --native-leaf | 3,657,342 | 2.317 | 370 | 1332 |  |
| x-ray | kolmo -m balanced --typed-huf | 3,657,342 | 2.317 | 502 | 1329 |  |
| x-ray | kolmo -m hc | 3,649,636 | 2.322 | 46.7 | 1323 |  |
| x-ray | kolmo -m ratio | 3,616,667 | 2.343 | 9.65 | 78.0 |  |
| x-ray | kolmo -x | 3,561,093 | 2.380 | 0.98 | 0.96 |  |
| x-ray | kolmo -x -l 2 | 3,561,093 | 2.380 | 0.98 | 0.96 |  |
| year.i32 | pcodec level 8 | 598 | 156.368 | 674 | 2918 |  |
| year.i32 | pcodec level 12 | 568 | 164.627 | 594 | 2393 |  |
| year.i32 | kolmo -m balanced | 902 | 103.667 | 78.7 | 4695 |  |
| year.i32 | kolmo -m balanced --native-leaf | 902 | 103.667 | 63.9 | 5009 |  |
| year.i32 | kolmo -m balanced --typed-huf | 902 | 103.667 | 79.7 | 5124 |  |
| year.i32 | kolmo -m hc | 729 | 128.269 | 11.6 | 4847 |  |
| year.i32 | kolmo -m ratio | 661 | 141.464 | 8.44 | 3421 |  |
| year.i32 | kolmo -x | 518 | 180.517 | 0.53 | 1.08 |  |
| year.i32 | kolmo -x -l 2 | 518 | 180.517 | 0.47 | 1.08 |  |

### Raw 8-bit images: 2 files, 1,048,576 bytes

| Setting | Bytes | Compress MB/s | Decompress MB/s | Size against JPEG XL lossless, effort 9 | Speed against it (compress / decompress) | Files where it is smaller | Mark |
|---|---|---|---|---|---|---|---|
| JPEG XL lossless, effort 7 | 444,996 | 3.81 | 28.8 | 0.4% larger | 5.97× / 0.96× | 0 of 2 |  |
| JPEG XL lossless, effort 9 | 443,027 | 0.64 | 29.9 |  |  |  |  |
| kolmo -m balanced | 576,124 | 187 | 469 | 30.0% larger | 294× / 15.7× | 0 of 2 |  |
| kolmo -m balanced --native-leaf | 576,124 | 72.2 | 457 | 30.0% larger | 113× / 15.3× | 0 of 2 |  |
| kolmo -m balanced --typed-huf | 576,124 | 185 | 456 | 30.0% larger | 291× / 15.2× | 0 of 2 |  |
| kolmo -m hc | 557,615 | 9.70 | 468 | 25.9% larger | 15.2× / 15.6× | 0 of 2 |  |
| kolmo -m ratio | 514,448 | 5.83 | 38.6 | 16.1% larger | 9.15× / 1.29× | 0 of 2 |  |
| kolmo -x | 440,131 | 0.84 | 0.92 | 0.7% smaller | 1.32× / 0.031× | 2 of 2 |  |
| kolmo -x -l 2 | 440,131 | 0.48 | 0.92 | 0.7% smaller | 0.75× / 0.031× | 2 of 2 |  |

Kolmo's bare library call (buffers allocated beforehand) against its Python binding, speed ratio compress / decompress: -m balanced 1.01× / 0.98×; -m balanced --native-leaf 1.00× / 1.01×; -m balanced --typed-huf 1.01× / 1.01×; -m hc 1.00× / 1.01×; -m ratio 1.00× / 1.00×; -x 1.01× / 1.00×; -x -l 2 1.00× / 1.00×.

| File | Setting | Bytes | Ratio | Compress MB/s | Decompress MB/s | Mark |
|---|---|---|---|---|---|---|
| img_astronaut.u8 | JPEG XL lossless, effort 7 | 325,421 | 2.417 | 3.63 | 28.9 |  |
| img_astronaut.u8 | JPEG XL lossless, effort 9 | 324,317 | 2.425 | 0.62 | 30.4 |  |
| img_astronaut.u8 | kolmo -m balanced | 431,865 | 1.821 | 199 | 547 |  |
| img_astronaut.u8 | kolmo -m balanced --native-leaf | 431,865 | 1.821 | 74.4 | 525 |  |
| img_astronaut.u8 | kolmo -m balanced --typed-huf | 431,865 | 1.821 | 196 | 524 |  |
| img_astronaut.u8 | kolmo -m hc | 418,783 | 1.878 | 10.8 | 555 |  |
| img_astronaut.u8 | kolmo -m ratio | 381,565 | 2.061 | 5.71 | 35.0 |  |
| img_astronaut.u8 | kolmo -x | 323,467 | 2.431 | 0.82 | 0.90 |  |
| img_astronaut.u8 | kolmo -x -l 2 | 323,467 | 2.431 | 0.47 | 0.91 |  |
| img_camera.u8 | JPEG XL lossless, effort 7 | 119,575 | 2.192 | 4.46 | 28.5 |  |
| img_camera.u8 | JPEG XL lossless, effort 9 | 118,710 | 2.208 | 0.68 | 28.6 |  |
| img_camera.u8 | kolmo -m balanced | 144,259 | 1.817 | 160 | 329 |  |
| img_camera.u8 | kolmo -m balanced --native-leaf | 144,259 | 1.817 | 66.2 | 329 |  |
| img_camera.u8 | kolmo -m balanced --typed-huf | 144,259 | 1.817 | 160 | 329 |  |
| img_camera.u8 | kolmo -m hc | 138,832 | 1.888 | 7.48 | 318 |  |
| img_camera.u8 | kolmo -m ratio | 132,883 | 1.973 | 6.22 | 55.5 |  |
| img_camera.u8 | kolmo -x | 116,664 | 2.247 | 0.89 | 0.96 |  |
| img_camera.u8 | kolmo -x -l 2 | 116,664 | 2.247 | 0.49 | 0.96 |  |

### Audio, 16-bit PCM: 40 files, 17,641,760 bytes

| Setting | Bytes | Compress MB/s | Decompress MB/s | Size against FLAC level 8 | Speed against it (compress / decompress) | Files where it is smaller | Mark |
|---|---|---|---|---|---|---|---|
| FLAC level 5 | 7,327,271 | 297 | 327 | 2.9% larger | 1.65× / 1.00× | 0 of 40 |  |
| FLAC level 8 | 7,121,613 | 180 | 326 |  |  |  |  |
| FLAC level 5, through a file | 7,327,271 | 263 | 321 | 2.9% larger | 1.47× / 0.98× | 0 of 40 |  |
| FLAC level 8, through a file | 7,121,613 | 167 | 319 | same size | 0.93× / 0.98× | 0 of 40 |  |
| pcodec level 8 | 8,663,769 | 214 | 1313 | 21.7% larger | 1.19× / 4.03× | 0 of 40 |  |
| pcodec level 12 | 8,630,372 | 41.1 | 1307 | 21.2% larger | 0.23× / 4.01× | 0 of 40 |  |
| kolmo -m balanced | 9,847,874 | 126 | 1614 | 38.3% larger | 0.70× / 4.95× | 0 of 40 |  |
| kolmo -m balanced --native-leaf | 9,646,556 | 66.5 | 1057 | 35.5% larger | 0.37× / 3.24× | 0 of 40 |  |
| kolmo -m balanced --typed-huf | 9,270,808 | 758 | 1683 | 30.2% larger | 4.22× / 5.16× | 0 of 40 |  |
| kolmo -m hc | 8,927,283 | 14.1 | 1405 | 25.4% larger | 0.078× / 4.31× | 0 of 40 |  |
| kolmo -m ratio | 8,630,460 | 6.25 | 95.0 | 21.2% larger | 0.035× / 0.29× | 0 of 40 |  |
| kolmo -x | 8,321,491 | 1.32 | 1.58 | 16.8% larger | 0.0073× / 0.0048× | 0 of 40 |  |
| kolmo -x -l 2 | 8,168,539 | 0.64 | 0.68 | 14.7% larger | 0.0036× / 0.0021× | 0 of 40 |  |

Kolmo's bare library call (buffers allocated beforehand) against its Python binding, speed ratio compress / decompress: -m balanced 1.01× / 1.04×; -m balanced --native-leaf 1.02× / 1.03×; -m balanced --typed-huf 1.03× / 1.04×; -m hc 1.00× / 1.04×; -m ratio 1.00× / 1.00×; -x 1.00× / 1.00×; -x -l 2 1.00× / 1.00×.

## S2. Command-line tools, whole process, one thread (OpenZL `zli`, 7-Zip, Kolmo, zstd)

| Data type (files, size) | Rival | Kolmo setting | Size | Compress speed | Decompress speed | Standing |
|---|---|---|---|---|---|---|
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 49.2% smaller | 3.31× | 2.42× | Ahead |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 56.0% smaller | 0.45× | 2.37× | Trade: smaller, slower to compress |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 58.0% smaller | 0.11× | 0.79× | Trade: smaller, slower to compress and decompress |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 6 | kolmo -x | 58.5% smaller | 0.016× | 0.0066× | Trade: smaller, slower to compress and decompress |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 48.5% smaller | 7.42× | 2.60× | Ahead |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 55.4% smaller | 1.00× | 2.55× | Ahead |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 57.4% smaller | 0.25× | 0.85× | Trade: smaller, slower to compress and decompress |
| tables (CSV) (4, 39.9 MB) | zli, its best profile for the file, -l 9 | kolmo -x | 57.9% smaller | 0.036× | 0.0071× | Trade: smaller, slower to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli trained compressor | kolmo -m balanced | 58.7% larger | 3.42× | 1.73× | Behind: larger, though faster to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli trained compressor | kolmo -m hc | 12.9% larger | 0.057× | 1.75× | Behind: larger, though faster to decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli trained compressor | kolmo -m ratio | 4.7% smaller | 0.039× | 0.24× | Trade: smaller, slower to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli trained compressor | kolmo -x | 26.9% smaller | 0.0060× | 0.0014× | Trade: smaller, slower to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 6.6% larger | 3.58× | 1.69× | Behind: larger, though faster to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 24.2% smaller | 0.060× | 1.71× | Trade: smaller, slower to compress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 36.0% smaller | 0.041× | 0.24× | Trade: smaller, slower to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 6 | kolmo -x | 50.9% smaller | 0.0062× | 0.0014× | Trade: smaller, slower to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 10.7% larger | 5.98× | 1.80× | Behind: larger, though faster to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 21.3% smaller | 0.10× | 1.82× | Trade: smaller, slower to compress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 33.5% smaller | 0.068× | 0.25× | Trade: smaller, slower to compress and decompress |
| tables (CSV), OpenZL trained on other rows (3, 26.6 MB) | zli, its best profile for the file, -l 9 | kolmo -x | 49.0% smaller | 0.010× | 0.0014× | Trade: smaller, slower to compress and decompress |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 28.2% smaller | 5.91× | 1.78× | Ahead |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 29.0% smaller | 0.35× | 1.55× | Trade: smaller, slower to compress |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 32.3% smaller | 0.13× | 0.18× | Trade: smaller, slower to compress and decompress |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 6 | kolmo -x | 33.8% smaller | 0.017× | 0.0024× | Trade: smaller, slower to compress and decompress |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 7.9% smaller | 7.55× | 2.19× | Ahead |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 8.9% smaller | 0.45× | 1.90× | Trade: smaller, slower to compress |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 13.2% smaller | 0.16× | 0.22× | Trade: smaller, slower to compress and decompress |
| 16-bit medical images (2, 18.4 MB) | zli, its best profile for the file, -l 9 | kolmo -x | 15.2% smaller | 0.022× | 0.0029× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sao -l 6 | kolmo -m balanced | 33.7% smaller | 1.49× | 2.23× | Ahead |
| star catalogue (1, 7.3 MB) | zli -p sao -l 6 | kolmo -m hc | 36.3% smaller | 0.14× | 2.05× | Trade: smaller, slower to compress |
| star catalogue (1, 7.3 MB) | zli -p sao -l 6 | kolmo -m ratio | 37.1% smaller | 0.096× | 0.28× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sao -l 6 | kolmo -x | 39.0% smaller | 0.011× | 0.0037× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sao -l 9 | kolmo -m balanced | 2.7% smaller | 4.07× | 2.18× | Ahead |
| star catalogue (1, 7.3 MB) | zli -p sao -l 9 | kolmo -m hc | 6.5% smaller | 0.40× | 2.01× | Trade: smaller, slower to compress |
| star catalogue (1, 7.3 MB) | zli -p sao -l 9 | kolmo -m ratio | 7.7% smaller | 0.26× | 0.28× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sao -l 9 | kolmo -x | 10.5% smaller | 0.030× | 0.0037× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 6 | kolmo -m balanced | 42.3% smaller | 1.90× | 2.41× | Ahead |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 6 | kolmo -m hc | 44.5% smaller | 0.19× | 2.22× | Trade: smaller, slower to compress |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 6 | kolmo -m ratio | 45.2% smaller | 0.12× | 0.30× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 6 | kolmo -x | 46.9% smaller | 0.014× | 0.0040× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 9 | kolmo -m balanced | 4.2% smaller | 4.12× | 2.20× | Ahead |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 9 | kolmo -m hc | 7.9% smaller | 0.40× | 2.03× | Trade: smaller, slower to compress |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 9 | kolmo -m ratio | 9.1% smaller | 0.27× | 0.28× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 9 | kolmo -x | 11.8% smaller | 0.030× | 0.0037× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 33.7% smaller | 1.49× | 2.23× | Ahead |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 36.3% smaller | 0.14× | 2.05× | Trade: smaller, slower to compress |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 37.1% smaller | 0.096× | 0.28× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 6 | kolmo -x | 39.0% smaller | 0.011× | 0.0037× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 2.7% smaller | 4.07× | 2.18× | Ahead |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 6.5% smaller | 0.40× | 2.01× | Trade: smaller, slower to compress |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 7.7% smaller | 0.26× | 0.28× | Trade: smaller, slower to compress and decompress |
| star catalogue (1, 7.3 MB) | zli, its best profile for the file, -l 9 | kolmo -x | 10.5% smaller | 0.030× | 0.0037× | Trade: smaller, slower to compress and decompress |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 29.8% smaller | 1.42× | 1.33× | Ahead |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 32.1% smaller | 0.13× | 1.29× | Trade: smaller, slower to compress |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 37.4% smaller | 0.083× | 0.27× | Trade: smaller, slower to compress and decompress |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 6 | kolmo -x | 46.4% smaller | 0.012× | 0.0073× | Trade: smaller, slower to compress and decompress |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 27.7% smaller | 2.40× | 1.33× | Ahead |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 30.1% smaller | 0.23× | 1.29× | Trade: smaller, slower to compress |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 35.5% smaller | 0.14× | 0.27× | Trade: smaller, slower to compress and decompress |
| raw 8-bit images (2, 1.0 MB) | zli, its best profile for the file, -l 9 | kolmo -x | 44.8% smaller | 0.020× | 0.0073× | Trade: smaller, slower to compress and decompress |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 15.4% smaller | 1.04× | 1.54× | Ahead |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 18.9% smaller | 0.23× | 1.44× | Trade: smaller, slower to compress |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 21.1% smaller | 0.20× | 1.28× | Trade: smaller, slower to compress |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 6 | kolmo -x | 21.4% smaller | 0.027× | 0.087× | Trade: smaller, slower to compress and decompress |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 18.3% smaller | 1.67× | 1.53× | Ahead |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 21.7% smaller | 0.37× | 1.43× | Trade: smaller, slower to compress |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 23.8% smaller | 0.33× | 1.27× | Trade: smaller, slower to compress |
| integer arrays (3, 0.3 MB) | zli, its best profile for the file, -l 9 | kolmo -x | 24.0% smaller | 0.044× | 0.086× | Trade: smaller, slower to compress and decompress |
| NumPy arrays (2, 24.9 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 7.5% smaller | 5.31× | 1.75× | Ahead |
| NumPy arrays (2, 24.9 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 16.8% smaller | 0.21× | 1.45× | Trade: smaller, slower to compress |
| NumPy arrays (2, 24.9 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 19.7% smaller | 0.65× | 0.23× | Trade: smaller, slower to compress and decompress |
| NumPy arrays (2, 24.9 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 7.5% smaller | 5.33× | 1.75× | Ahead |
| NumPy arrays (2, 24.9 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 16.7% smaller | 0.21× | 1.45× | Trade: smaller, slower to compress |
| NumPy arrays (2, 24.9 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 19.7% smaller | 0.65× | 0.23× | Trade: smaller, slower to compress and decompress |
| pickle (1, 24.2 MB) | zli -p pickle -l 6 | kolmo -m balanced | 46.8% smaller | 5.64× | 1.84× | Ahead |
| pickle (1, 24.2 MB) | zli -p pickle -l 6 | kolmo -m hc | 53.6% smaller | 0.17× | 1.49× | Trade: smaller, slower to compress |
| pickle (1, 24.2 MB) | zli -p pickle -l 6 | kolmo -m ratio | 54.2% smaller | 1.67× | 0.22× | Trade: smaller, slower to decompress |
| pickle (1, 24.2 MB) | zli -p pickle -l 9 | kolmo -m balanced | 46.7% smaller | 6.46× | 1.84× | Ahead |
| pickle (1, 24.2 MB) | zli -p pickle -l 9 | kolmo -m hc | 53.6% smaller | 0.19× | 1.50× | Trade: smaller, slower to compress |
| pickle (1, 24.2 MB) | zli -p pickle -l 9 | kolmo -m ratio | 54.2% smaller | 1.91× | 0.23× | Trade: smaller, slower to decompress |
| pickle (1, 24.2 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 46.8% smaller | 5.64× | 1.84× | Ahead |
| pickle (1, 24.2 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 53.6% smaller | 0.17× | 1.49× | Trade: smaller, slower to compress |
| pickle (1, 24.2 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 54.2% smaller | 1.67× | 0.22× | Trade: smaller, slower to decompress |
| pickle (1, 24.2 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 46.7% smaller | 6.46× | 1.84× | Ahead |
| pickle (1, 24.2 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 53.6% smaller | 0.19× | 1.50× | Trade: smaller, slower to compress |
| pickle (1, 24.2 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 54.2% smaller | 1.91× | 0.23× | Trade: smaller, slower to decompress |
| model weights (6, 84.8 MB) | zli, its best profile for the file, -l 6 | kolmo -m balanced | 2.1% smaller | 1.78× | 3.98× | Ahead |
| model weights (6, 84.8 MB) | zli, its best profile for the file, -l 6 | kolmo -m hc | 4.6% smaller | 0.038× | 3.69× | Trade: smaller, slower to compress |
| model weights (6, 84.8 MB) | zli, its best profile for the file, -l 6 | kolmo -m ratio | 6.6% smaller | 0.018× | 0.24× | Trade: smaller, slower to compress and decompress |
| model weights (6, 84.8 MB) | zli, its best profile for the file, -l 9 | kolmo -m balanced | 2.1% smaller | 1.84× | 3.97× | Ahead |
| model weights (6, 84.8 MB) | zli, its best profile for the file, -l 9 | kolmo -m hc | 4.6% smaller | 0.039× | 3.69× | Trade: smaller, slower to compress |
| model weights (6, 84.8 MB) | zli, its best profile for the file, -l 9 | kolmo -m ratio | 6.6% smaller | 0.019× | 0.24× | Trade: smaller, slower to compress and decompress |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -myx9 -mmt1 (LZMA2, filter picked by 7-Zip) | kolmo -m balanced | 36.7% larger | 68.1× | 7.69× | Behind: larger, though faster to compress and decompress (marked: threads) |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -myx9 -mmt1 (LZMA2, filter picked by 7-Zip) | kolmo -m hc | 13.0% larger | 1.54× | 6.76× | Behind: larger, though faster to compress and decompress (marked: threads) |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -myx9 -mmt1 (LZMA2, filter picked by 7-Zip) | kolmo -m ratio | 2.8% larger | 0.98× | 0.76× | Behind: larger (marked: threads) |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -myx9 -mmt1 -mmtf=off (the same, filter on the one thread) | kolmo -m balanced | 36.3% larger | 68.9× | 7.81× | Behind: larger, though faster to compress and decompress |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -myx9 -mmt1 -mmtf=off (the same, filter on the one thread) | kolmo -m hc | 12.7% larger | 1.55× | 6.87× | Behind: larger, though faster to compress and decompress |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -myx9 -mmt1 -mmtf=off (the same, filter on the one thread) | kolmo -m ratio | 2.5% larger | 0.99× | 0.77× | Behind: larger |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -mf=off -mmt1 (LZMA2, no filter) | kolmo -m balanced | 33.9% larger | 68.5× | 7.90× | Behind: larger, though faster to compress and decompress |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -mf=off -mmt1 (LZMA2, no filter) | kolmo -m hc | 10.7% larger | 1.54× | 6.95× | Behind: larger, though faster to compress and decompress |
| executables and mixed (3, 79.0 MB) | 7zz -t7z -mx9 -mf=off -mmt1 (LZMA2, no filter) | kolmo -m ratio | 0.7% larger | 0.99× | 0.78× | Behind: same size, slower to decompress |

### S2, every setting by data type

| Data type | Setting | Bytes | Ratio | Compress MB/s | Decompress MB/s | Peak memory MB (compress / decompress) | Mark |
|---|---|---|---|---|---|---|---|
| tables (CSV) | zstd -3 | 12,007,377 | 3.323 | 323 | 875 | 8 / 8 |  |
| tables (CSV) | kolmo -m balanced | 4,318,232 | 9.241 | 256 | 637 | 38 / 15 |  |
| tables (CSV) | kolmo -m balanced --native-leaf | 4,318,232 | 9.241 | 257 | 639 | 38 / 15 |  |
| tables (CSV) | kolmo -m hc | 3,736,716 | 10.680 | 34.7 | 625 | 200 / 30 |  |
| tables (CSV) | kolmo -m ratio | 3,569,682 | 11.179 | 8.77 | 207 | 254 / 41 |  |
| tables (CSV) | kolmo -x | 3,529,106 | 11.308 | 1.24 | 1.75 | 5756 / 4480 |  |
| tables (CSV) | zli, its best profile for the file, -l 6 | 8,498,849 | 4.696 | 77.5 | 264 | 350 / 143 |  |
| tables (CSV) | zli, its best profile for the file, -l 9 | 8,379,805 | 4.762 | 34.5 | 245 | 313 / 152 |  |
| tables (CSV), OpenZL trained on other rows | zstd -3 | 5,144,333 | 5.180 | 471 | 1110 | 8 / 8 |  |
| tables (CSV), OpenZL trained on other rows | kolmo -m balanced | 4,603,835 | 5.789 | 417 | 925 | 12 / 8 |  |
| tables (CSV), OpenZL trained on other rows | kolmo -m balanced --native-leaf | 4,603,835 | 5.789 | 421 | 922 | 13 / 8 |  |
| tables (CSV), OpenZL trained on other rows | kolmo -m hc | 3,274,912 | 8.138 | 6.98 | 935 | 137 / 41 |  |
| tables (CSV), OpenZL trained on other rows | kolmo -m ratio | 2,765,392 | 9.637 | 4.74 | 131 | 278 / 86 |  |
| tables (CSV), OpenZL trained on other rows | kolmo -x | 2,120,773 | 12.566 | 0.73 | 0.74 | 4767 / 4761 |  |
| tables (CSV), OpenZL trained on other rows | zli trained compressor | 2,900,550 | 9.188 | 122 | 535 | 242 / 97 |  |
| tables (CSV), OpenZL trained on other rows | zli, its best profile for the file, -l 6 | 4,318,650 | 6.171 | 117 | 546 | 199 / 84 |  |
| tables (CSV), OpenZL trained on other rows | zli, its best profile for the file, -l 9 | 4,159,923 | 6.406 | 69.7 | 513 | 139 / 87 |  |
| 16-bit medical images | zstd -3 | 9,631,709 | 1.915 | 293 | 823 | 8 / 8 |  |
| 16-bit medical images | kolmo -m balanced | 5,973,954 | 3.088 | 363 | 765 | 11 / 6 |  |
| 16-bit medical images | kolmo -m balanced --native-leaf | 5,973,954 | 3.088 | 282 | 762 | 20 / 6 |  |
| 16-bit medical images | kolmo -m hc | 5,907,152 | 3.122 | 21.4 | 663 | 132 / 34 |  |
| 16-bit medical images | kolmo -m ratio | 5,630,849 | 3.276 | 7.91 | 76.6 | 187 / 46 |  |
| 16-bit medical images | kolmo -x | 5,501,818 | 3.352 | 1.07 | 1.03 | 4504 / 4442 |  |
| 16-bit medical images | zli, its best profile for the file, -l 6 | 8,316,183 | 2.218 | 61.5 | 429 | 166 / 55 |  |
| 16-bit medical images | zli, its best profile for the file, -l 9 | 6,486,975 | 2.843 | 48.1 | 350 | 191 / 59 |  |
| star catalogue | zstd -3 | 5,543,783 | 1.308 | 275 | 733 | 8 / 8 |  |
| star catalogue | kolmo -m balanced | 2,302,435 | 3.150 | 233 | 1021 | 18 / 13 |  |
| star catalogue | kolmo -m balanced --native-leaf | 2,302,435 | 3.150 | 193 | 1021 | 19 / 13 |  |
| star catalogue | kolmo -m hc | 2,214,262 | 3.275 | 22.7 | 942 | 80 / 19 |  |
| star catalogue | kolmo -m ratio | 2,184,759 | 3.319 | 15.0 | 129 | 71 / 23 |  |
| star catalogue | kolmo -x | 2,119,979 | 3.421 | 1.69 | 1.71 | 4501 / 4446 |  |
| star catalogue | zli -p sao -l 6 | 3,475,261 | 2.087 | 157 | 459 | 70 / 41 |  |
| star catalogue | zli -p sao -l 9 | 2,367,470 | 3.063 | 57.3 | 468 | 104 / 33 |  |
| star catalogue | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 6 | 3,989,003 | 1.818 | 122 | 424 | 73 / 40 |  |
| star catalogue | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 9 | 2,403,676 | 3.017 | 56.6 | 465 | 102 / 34 |  |
| star catalogue | zli, its best profile for the file, -l 6 | 3,475,261 | 2.087 | 157 | 459 | 70 / 41 |  |
| star catalogue | zli, its best profile for the file, -l 9 | 2,367,470 | 3.063 | 57.3 | 468 | 104 / 33 |  |
| raw 8-bit images | zstd -3 | 830,714 | 1.262 | 117 | 161 | 6 / 5 |  |
| raw 8-bit images | kolmo -m balanced | 576,124 | 1.820 | 95.3 | 157 | 8 / 5 |  |
| raw 8-bit images | kolmo -m balanced --native-leaf | 576,124 | 1.820 | 50.9 | 154 | 9 / 5 |  |
| raw 8-bit images | kolmo -m hc | 557,615 | 1.880 | 9.02 | 152 | 29 / 5 |  |
| raw 8-bit images | kolmo -m ratio | 514,448 | 2.038 | 5.60 | 32.3 | 25 / 11 |  |
| raw 8-bit images | kolmo -x | 440,131 | 2.382 | 0.79 | 0.85 | 1223 / 1205 |  |
| raw 8-bit images | zli, its best profile for the file, -l 6 | 821,228 | 1.277 | 67.2 | 118 | 14 / 7 |  |
| raw 8-bit images | zli, its best profile for the file, -l 9 | 797,261 | 1.315 | 39.7 | 118 | 22 / 7 |  |
| integer arrays | zstd -3 | 72,800 | 4.245 | 31.2 | 38.6 | 3 / 3 |  |
| integer arrays | kolmo -m balanced | 43,313 | 7.134 | 25.5 | 52.4 | 6 / 3 |  |
| integer arrays | kolmo -m balanced --native-leaf | 43,313 | 7.134 | 16.3 | 49.8 | 6 / 3 |  |
| integer arrays | kolmo -m hc | 41,506 | 7.445 | 5.65 | 49.0 | 11 / 3 |  |
| integer arrays | kolmo -m ratio | 40,398 | 7.649 | 4.98 | 43.5 | 7 / 3 |  |
| integer arrays | kolmo -x | 40,255 | 7.676 | 0.67 | 2.94 | 385 / 124 |  |
| integer arrays | zli, its best profile for the file, -l 6 | 51,205 | 6.035 | 24.5 | 34.0 | 9 / 6 |  |
| integer arrays | zli, its best profile for the file, -l 9 | 52,983 | 5.832 | 15.3 | 34.3 | 9 / 7 |  |
| NumPy arrays | zstd -3 | 3,152,102 | 7.912 | 744 | 1674 | 8 / 8 |  |
| NumPy arrays | kolmo -m balanced | 1,897,958 | 13.140 | 568 | 1630 | 30 / 12 |  |
| NumPy arrays | kolmo -m balanced --native-leaf | 1,897,958 | 13.140 | 489 | 1619 | 31 / 12 |  |
| NumPy arrays | kolmo -m hc | 1,708,242 | 14.599 | 22.1 | 1355 | 88 / 62 |  |
| NumPy arrays | kolmo -m ratio | 1,648,514 | 15.128 | 69.6 | 212 | 74 / 90 |  |
| NumPy arrays | zli, its best profile for the file, -l 6 | 2,051,967 | 12.153 | 107 | 934 | 174 / 94 |  |
| NumPy arrays | zli, its best profile for the file, -l 9 | 2,051,927 | 12.154 | 106 | 934 | 174 / 94 |  |
| pickle | zstd -3 | 2,795,090 | 8.646 | 879 | 2177 | 8 / 8 |  |
| pickle | kolmo -m balanced | 1,585,658 | 15.240 | 755 | 1981 | 30 / 12 |  |
| pickle | kolmo -m balanced --native-leaf | 1,585,658 | 15.240 | 735 | 1965 | 31 / 12 |  |
| pickle | kolmo -m hc | 1,381,768 | 17.489 | 22.2 | 1611 | 89 / 59 |  |
| pickle | kolmo -m ratio | 1,363,471 | 17.724 | 224 | 242 | 74 / 87 |  |
| pickle | zli -p pickle -l 6 | 2,979,695 | 8.110 | 134 | 1079 | 94 / 54 |  |
| pickle | zli -p pickle -l 9 | 2,975,276 | 8.122 | 117 | 1074 | 94 / 54 |  |
| pickle | zli, its best profile for the file, -l 6 | 2,979,695 | 8.110 | 134 | 1079 | 94 / 54 |  |
| pickle | zli, its best profile for the file, -l 9 | 2,975,276 | 8.122 | 117 | 1074 | 94 / 54 |  |
| model weights | zstd -3 | 70,124,359 | 1.209 | 669 | 1113 | 9 / 9 |  |
| model weights | kolmo -m balanced | 61,424,226 | 1.381 | 540 | 1618 | 58 / 13 |  |
| model weights | kolmo -m balanced --native-leaf | 61,424,226 | 1.381 | 527 | 1618 | 58 / 13 |  |
| model weights | kolmo -m hc | 59,860,058 | 1.417 | 11.5 | 1504 | 200 / 46 |  |
| model weights | kolmo -m ratio | 58,572,652 | 1.448 | 5.53 | 98.7 | 656 / 171 |  |
| model weights | zli, its best profile for the file, -l 6 | 62,729,453 | 1.352 | 303 | 407 | 171 / 184 |  |
| model weights | zli, its best profile for the file, -l 9 | 62,729,450 | 1.352 | 294 | 408 | 171 / 184 |  |
| executables and mixed | zstd -3 | 26,511,977 | 2.979 | 477 | 1282 | 8 / 8 |  |
| executables and mixed | kolmo -m balanced | 26,156,836 | 3.019 | 400 | 1418 | 25 / 11 |  |
| executables and mixed | kolmo -m balanced --native-leaf | 26,156,836 | 3.019 | 394 | 1415 | 25 / 11 |  |
| executables and mixed | kolmo -m hc | 21,625,883 | 3.652 | 9.03 | 1248 | 164 / 22 |  |
| executables and mixed | kolmo -m ratio | 19,668,675 | 4.015 | 5.77 | 141 | 338 / 49 |  |
| executables and mixed | 7zz -t7z -mx9 -myx9 -mmt1 (LZMA2, filter picked by 7-Zip) | 19,136,038 | 4.127 | 5.88 | 184 | 532 / 56 | threads |
| executables and mixed | 7zz -t7z -mx9 -myx9 -mmt1 -mmtf=off (the same, filter on the one thread) | 19,186,427 | 4.116 | 5.81 | 182 | 532 / 56 |  |
| executables and mixed | 7zz -t7z -mx9 -mf=off -mmt1 (LZMA2, no filter) | 19,528,860 | 4.044 | 5.85 | 180 | 532 / 56 |  |

| File | OpenZL profile | Bytes | Compress MB/s | Decompress MB/s | Mark |
|---|---|---|---|---|---|
| appl.test | zli -p csv -l 6 | 992,376 | 139 | 666 |  |
| appl.test | zli -p csv -l 9 | 950,802 | 97.0 | 646 |  |
| energy_test.csv | zli -p csv -l 6 | 1,660,002 | 70.0 | 243 |  |
| energy_test.csv | zli -p csv -l 9 | 1,633,364 | 32.8 | 221 |  |
| img_astronaut.u8 | zli -p u8 -l 6 | 647,480 | 81.1 | 148 |  |
| img_astronaut.u8 | zli -p u8 -l 9 | 647,241 | 52.8 | 151 |  |
| img_camera.u8 | zli -p u8 -l 6 | 173,748 | 44.4 | 72.8 |  |
| img_camera.u8 | zli -p u8 -l 9 | 150,020 | 22.8 | 70.8 |  |
| lfw_subset.npy | zli -p numpy -l 6 | 224,663 | 62.9 | 238 |  |
| lfw_subset.npy | zli -p numpy -l 9 | 224,659 | 61.0 | 233 |  |
| llm_bf16_10mb.safetensors | zli -p safetensors -l 6 | 6,964,512 | 308 | 393 |  |
| llm_bf16_10mb.safetensors | zli -p safetensors -l 9 | 6,964,512 | 308 | 393 |  |
| llm_bf16_47mb.safetensors | zli -p safetensors -l 6 | 31,208,618 | 337 | 450 |  |
| llm_bf16_47mb.safetensors | zli -p safetensors -l 9 | 31,208,618 | 337 | 454 |  |
| llm_nvfp4_17mb.safetensors | zli -p safetensors -l 6 | 16,726,197 | 263 | 389 |  |
| llm_nvfp4_17mb.safetensors | zli -p safetensors -l 9 | 16,726,245 | 232 | 381 |  |
| log_timestamps_ms.i64 | zli -p le-i64 -l 6 | 4,108 | 15.7 | 21.4 |  |
| log_timestamps_ms.i64 | zli -p le-i64 -l 9 | 3,124 | 15.3 | 22.2 |  |
| magika.gguf | zli -p gguf -l 6 | 2,609,121 | 247 | 296 |  |
| magika.gguf | zli -p gguf -l 9 | 2,609,111 | 249 | 308 |  |
| magika.onnx | zli -p onnx -l 6 | 2,612,786 | 240 | 307 |  |
| magika.onnx | zli -p onnx -l 9 | 2,612,745 | 240 | 304 |  |
| magika.safetensors | zli -p safetensors -l 6 | 2,608,219 | 249 | 302 |  |
| magika.safetensors | zli -p safetensors -l 9 | 2,608,219 | 251 | 308 |  |
| mr | zli -p le-u16 -l 6 | 3,137,351 | 77.8 | 580 |  |
| mr | zli -p le-u16 -l 9 | 2,730,044 | 41.6 | 428 |  |
| mr | zli -p be-u16 -l 6 | 3,136,882 | 75.9 | 551 |  |
| mr | zli -p be-u16 -l 9 | 3,329,430 | 69.2 | 343 |  |
| owid-co2-data.csv | zli -p csv -l 6 | 3,113,453 | 84.1 | 298 |  |
| owid-co2-data.csv | zli -p csv -l 9 | 3,076,041 | 35.7 | 283 |  |
| owid_energy.pkl | zli -p pickle -l 6 | 2,979,695 | 134 | 1079 |  |
| owid_energy.pkl | zli -p pickle -l 9 | 2,975,276 | 117 | 1074 |  |
| owid_energy_f64.npy | zli -p numpy -l 6 | 1,827,304 | 110 | 1064 |  |
| owid_energy_f64.npy | zli -p numpy -l 9 | 1,827,268 | 110 | 1069 |  |
| owid_test.csv | zli -p csv -l 6 | 2,318,911 | 78.1 | 284 |  |
| owid_test.csv | zli -p csv -l 9 | 2,266,750 | 35.1 | 264 |  |
| population.i64 | zli -p le-i64 -l 6 | 46,275 | 31.5 | 47.2 |  |
| population.i64 | zli -p le-i64 -l 9 | 49,198 | 12.8 | 47.2 |  |
| quakes2.test | zli -p csv -l 6 | 521,040 | 76.9 | 272 |  |
| quakes2.test | zli -p csv -l 9 | 493,930 | 47.3 | 276 |  |
| rides2.test | zli -p csv -l 6 | 2,805,234 | 114 | 565 |  |
| rides2.test | zli -p csv -l 9 | 2,715,191 | 63.5 | 513 |  |
| sao | zli -p sao -l 6 | 3,475,261 | 157 | 459 |  |
| sao | zli -p sao -l 9 | 2,367,470 | 57.3 | 468 |  |
| sao | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 6 | 3,989,003 | 122 | 424 |  |
| sao | zli -p sddl2 --profile-arg <sao_silesia.sddl> -l 9 | 2,403,676 | 56.6 | 465 |  |
| wide_test.csv | zli -p csv --profile-arg ; -l 6 | 1,406,483 | 73.1 | 217 |  |
| wide_test.csv | zli -p csv --profile-arg ; -l 9 | 1,403,650 | 33.5 | 199 |  |
| x-ray | zli -p le-u16 -l 6 | 5,179,590 | 50.8 | 347 |  |
| x-ray | zli -p le-u16 -l 9 | 3,756,931 | 58.8 | 288 |  |
| x-ray | zli -p be-u16 -l 6 | 5,179,301 | 50.3 | 340 |  |
| x-ray | zli -p be-u16 -l 9 | 5,669,626 | 75.2 | 346 |  |
| year.i32 | zli -p le-i32 -l 6 | 822 | 25.3 | 32.2 |  |
| year.i32 | zli -p le-i32 -l 9 | 661 | 22.3 | 32.2 |  |

## Further comparisons

Made for the report from the same rows. The data-type tables repeat the pairs above with one comparison per line and its standing. The table "Pairs not fixed before measuring" holds pairs chosen after the results were seen.

### A2 by data type, one comparison per line

| Data type (files, size) | Kolmo setting | Rival | Size | Compress speed | Decompress speed | Standing | Files where Kolmo is smaller / within 1% / larger |
|---|---|---|---|---|---|---|---|
| Plain text (4, 88.3 MB) | kolmo -m balanced | zstd -3 | 0.6% larger | 1.00× | 0.83× | Behind: same size, slower to decompress | 0 / 2 / 2 |
| Plain text (4, 88.3 MB) | kolmo -m ratio | bsc -m0 -e2 | 0.9% larger | 1.22× | 2.02× | Ahead: same size, faster to compress and decompress | 0 / 2 / 2 |
| Plain text (4, 88.3 MB) | kolmo -x | bsc -m0 -e2 | 13.0% smaller | 0.013× | 0.012× | Trade: smaller, slower to compress and decompress | 4 / 0 / 0 |
| Executables and mixed (3, 79.0 MB) | kolmo -m balanced | zstd -3 | 0.6% smaller | 0.74× | 0.92× | Behind: same size, slower to compress | 1 / 1 / 1 |
| Executables and mixed (3, 79.0 MB) | kolmo -m ratio | xz -9 | 0.5% larger | 0.95× | 0.97× | Level | 1 / 1 / 1 |
| Executables and mixed (3, 79.0 MB) | kolmo -x | xz -9 | 14.6% smaller | 0.14× | 0.0058× | Trade: smaller, slower to compress and decompress | 3 / 0 / 0 |
| Database and markup (3, 49.0 MB) | kolmo -m balanced | zstd -3 | 10.9% smaller | 0.63× | 0.56× | Trade: smaller, slower to compress and decompress | 1 / 1 / 1 |
| Database and markup (3, 49.0 MB) | kolmo -m ratio | bsc -m0 -e2 | 1.9% larger | 0.20× | 2.15× | Behind: larger, though faster to decompress | 0 / 0 / 3 |
| Database and markup (3, 49.0 MB) | kolmo -x | bsc -m0 -e2 | 5.3% smaller | 0.011× | 0.011× | Trade: smaller, slower to compress and decompress | 2 / 1 / 0 |
| JSON (2, 23.7 MB) | kolmo -m balanced | zstd -3 | 38.5% smaller | 0.39× | 0.63× | Trade: smaller, slower to compress and decompress | 1 / 0 / 1 |
| JSON (2, 23.7 MB) | kolmo -m ratio | bsc -m0 -e2 | 26.1% smaller | 1.52× | 2.73× | Ahead | 1 / 0 / 1 |
| JSON (2, 23.7 MB) | kolmo -x | bsc -m0 -e2 | 47.4% smaller | 0.018× | 0.013× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Log (1, 4.5 MB) | kolmo -m balanced | zstd -3 | 29.9% smaller | 0.39× | 0.52× | Trade: smaller, slower to compress and decompress |  |
| Log (1, 4.5 MB) | kolmo -m ratio | brotli -11 | 19.9% smaller | 18.8× | 0.30× | Trade: smaller, slower to decompress |  |
| Log (1, 4.5 MB) | kolmo -x | brotli -11 | 39.6% smaller | 0.50× | 0.0004× | Trade: smaller, slower to compress and decompress |  |
| DNA text (1, 51.8 MB) | kolmo -m balanced | zstd -3 | same size | 0.73× | 0.93× | Behind: same size, slower to compress |  |
| DNA text (1, 51.8 MB) | kolmo -m ratio | bzip3 -5 | 1.5% larger | 3.35× | 3.46× | Behind: larger, though faster to compress and decompress |  |
| DNA text (1, 51.8 MB) | kolmo -x | bzip3 -5 | 10.1% smaller | 0.036× | 0.027× | Trade: smaller, slower to compress and decompress |  |
| 16-bit medical images (2, 18.4 MB) | kolmo -m balanced | zstd -3 | 38.0% smaller | 1.11× | 0.69× | Trade: smaller, slower to decompress | 2 / 0 / 0 |
| 16-bit medical images (2, 18.4 MB) | kolmo -m ratio | bzip3 -5 | 2.5% smaller | 0.66× | 3.61× | Trade: smaller, slower to compress | 2 / 0 / 0 |
| 16-bit medical images (2, 18.4 MB) | kolmo -x | bzip3 -5 | 4.8% smaller | 0.091× | 0.046× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Star catalogue (1, 7.3 MB) | kolmo -m balanced | zstd -3 | 58.3% smaller | 0.75× | 1.50× | Trade: smaller, slower to compress |  |
| Star catalogue (1, 7.3 MB) | kolmo -m ratio | xz -9 | 50.5% smaller | 2.25× | 2.16× | Ahead |  |
| Star catalogue (1, 7.3 MB) | kolmo -x | xz -9 | 52.0% smaller | 0.25× | 0.027× | Trade: smaller, slower to compress and decompress |  |
| Tables (CSV) (4, 39.9 MB) | kolmo -m balanced | zstd -3 | 64.0% smaller | 0.71× | 0.45× | Trade: smaller, slower to compress and decompress | 4 / 0 / 0 |
| Tables (CSV) (4, 39.9 MB) | kolmo -m ratio | bzip3 -5 | 56.2% smaller | 0.56× | 8.45× | Trade: smaller, slower to compress | 4 / 0 / 0 |
| Tables (CSV) (4, 39.9 MB) | kolmo -x | bzip3 -5 | 56.7% smaller | 0.080× | 0.062× | Trade: smaller, slower to compress and decompress | 4 / 0 / 0 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m balanced | zstd -3 | 30.5% smaller | 0.38× | 0.28× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m ratio | bzip3 -5 | 9.3% smaller | 0.46× | 1.87× | Trade: smaller, slower to compress | 1 / 1 / 0 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -x | bzip3 -5 | 22.4% smaller | 0.066× | 0.043× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Integer arrays (3, 0.3 MB) | kolmo -m balanced | zstd -3 | 40.5% smaller | 0.066× | 1.48× | Trade: smaller, slower to compress | 3 / 0 / 0 |
| Integer arrays (3, 0.3 MB) | kolmo -m ratio | xz -9 | 34.8% smaller | 0.33× | 1.81× | Trade: smaller, slower to compress | 3 / 0 / 0 |
| Integer arrays (3, 0.3 MB) | kolmo -x | xz -9 | 35.0% smaller | 0.041× | 0.018× | Trade: smaller, slower to compress and decompress | 3 / 0 / 0 |
| NumPy arrays (2, 24.9 MB) | kolmo -m balanced | zstd -3 | 39.7% smaller | 0.57× | 0.63× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| NumPy arrays (2, 24.9 MB) | kolmo -m ratio | bsc -m0 -e2 | 17.2% smaller | 0.58× | 1.98× | Trade: smaller, slower to compress | 1 / 0 / 1 |
| NumPy arrays (2, 24.9 MB) | kolmo -x | bsc -m0 -e2 | 26.6% smaller | 0.038× | 0.19× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Pickle (1, 24.2 MB) | kolmo -m balanced | zstd -3 | 43.2% smaller | 0.68× | 0.62× | Trade: smaller, slower to compress and decompress |  |
| Pickle (1, 24.2 MB) | kolmo -m ratio | bsc -m0 -e2 | 22.7% smaller | 1.75× | 2.02× | Ahead |  |
| Pickle (1, 24.2 MB) | kolmo -x | bsc -m0 -e2 | 31.3% smaller | 0.052× | 4.69× | Trade: smaller, slower to compress |  |
| Model weights (6, 84.8 MB) | kolmo -m balanced | zstd -3 | 12.4% smaller | 0.66× | 1.63× | Trade: smaller, slower to compress | 5 / 1 / 0 |
| Model weights (6, 84.8 MB) | kolmo -m ratio | xz -9 | 0.5% smaller | 1.41× | 1.34× | Ahead: same size, faster to compress and decompress | 4 / 1 / 1 |
| Model weights (6, 84.8 MB) | kolmo -x | xz -9 | 0.8% smaller | 0.22× | 0.042× | Behind: same size, slower to compress and decompress | 5 / 0 / 1 |

### A2, all 35 files together: the settings measured on every file

| Setting | Sum of the compressed sizes | Ratio | Compress MB/s | Decompress MB/s | Input bytes | Mark |
|---|---|---|---|---|---|---|
| lz4 | 258,171,743 | 1.926 | 1303 | 7709 | 497,141,393 |  |
| kolmo -m speed | 257,932,037 | 1.927 | 1261 | 7094 | 497,141,393 |  |
| zstd -1 | 186,642,497 | 2.664 | 957 | 2425 | 497,141,393 |  |
| kolmo -m fast -l 1 -Q | 177,309,948 | 2.804 | 943 | 2490 | 497,141,393 |  |
| zstd -3 | 175,607,549 | 2.831 | 630 | 2340 | 497,141,393 |  |
| kolmo -m balanced | 148,114,822 | 3.356 | 476 | 1922 | 497,141,393 |  |
| kolmo -m balanced --native-leaf | 148,114,822 | 3.356 | 455 | 1916 | 497,141,393 |  |
| libdeflate -6 | 179,227,525 | 2.774 | 162 | 1243 | 497,141,393 |  |
| brotli -5 | 163,052,487 | 3.049 | 125 | 699 | 497,141,393 |  |
| kolmo -m eco | 135,698,076 | 3.664 | 40.4 | 833 | 497,141,393 |  |
| zstd -9 | 163,343,288 | 3.044 | 142 | 2463 | 497,141,393 |  |
| kolmo -m hc -l 9 | 138,826,815 | 3.581 | 134 | 1924 | 497,141,393 |  |
| zstd -19 | 150,483,142 | 3.304 | 6.43 | 2362 | 497,141,393 |  |
| kolmo -m hc | 130,088,108 | 3.822 | 9.47 | 1950 | 497,141,393 |  |
| brotli -11 | 142,772,181 | 3.482 | 1.13 | 512 | 497,141,393 |  |
| xz -9 | 135,340,840 | 3.673 | 4.94 | 162 | 497,141,393 |  |
| bsc -m0 -e2 | 135,548,416 | 3.668 | 37.8 | 34.0 | 497,141,393 |  |
| bzip3 -5 | 135,956,052 | 3.657 | 16.0 | 25.5 | 497,141,393 |  |
| kolmo -m ratio | 119,581,199 | 4.157 | 11.3 | 127 | 497,141,393 |  |
| kolmo -x | 112,519,699 | 4.418 | 0.88 | 1.05 | 497,141,393 |  |

### Pairs not fixed before measuring

| Kolmo setting | Rival | Input | Size | Compress speed | Decompress speed | Standing | Files where Kolmo is smaller / within 1% / larger |
|---|---|---|---|---|---|---|---|
| kolmo -m fast -q | zstd -9 | silesia.tar | same size | 4.00× | 0.82× | Same size: faster to compress, slower to decompress |  |
| kolmo -m balanced | zstd -9 | silesia.tar | 1.4% smaller | 3.75× | 0.77× | Trade: smaller, slower to decompress |  |
| kolmo -m balanced | zstd -9 | 35 files | 9.3% smaller | 3.36× | 0.78× | Trade: smaller, slower to decompress | 23 / 1 / 11 |
| kolmo -m hc -l 9 | zstd -19 | silesia.tar | 0.9% smaller | 19.2× | 0.76× | Same size: faster to compress, slower to decompress |  |
| kolmo -m hc -l 9 | zstd -19 | 35 files | 7.7% smaller | 20.9× | 0.81× | Trade: smaller, slower to decompress | 23 / 1 / 11 |
| kolmo -m max -l 1 | bzip3 -5 | silesia.tar | 7.0% smaller | 3.55× | 2.68× | Ahead |  |
| kolmo -m max -l 1 | kanzi -7 | silesia.tar | 7.1% smaller | 1.99× | 2.31× | Ahead |  |
| kolmo -m max -l 1 | xz -9 | silesia.tar | 9.9% smaller | 10.1× | 0.42× | Trade: smaller, slower to decompress |  |
| kolmo -m ratio | zpaq -5 | silesia.tar | 0.5% larger | 11.5× | 163× | Ahead: same size, faster to compress and decompress |  |

### A3, all 18 typed arrays, one comparison per line

| Kolmo setting | Rival | Size | Compress speed | Decompress speed | Standing | Files where Kolmo is smaller / within 1% / larger |
|---|---|---|---|---|---|---|
| kolmo -m balanced | zstd -3 | 36.6% smaller | 0.59× | 0.76× | Trade: smaller, slower to compress and decompress | 18 / 0 / 0 |
| kolmo -m balanced --native-leaf | zstd -3 | 37.4% smaller | 0.39× | 0.71× | Trade: smaller, slower to compress and decompress | 18 / 0 / 0 |
| kolmo -m balanced --typed-huf | zstd -3 | 24.5% smaller | 0.85× | 0.91× | Trade: smaller, slower to compress | 15 / 1 / 2 |
| kolmo -m ratio | bzip3 -5 | 3.3% smaller | 0.77× | 3.61× | Trade: smaller, slower to compress | 9 / 0 / 9 |
| kolmo -x | bzip3 -5 | 8.0% smaller | 0.079× | 0.048× | Trade: smaller, slower to compress and decompress | 12 / 0 / 6 |
| kolmo -x -l 2 | bzip3 -5 | 9.3% smaller | 0.057× | 0.052× | Trade: smaller, slower to compress and decompress | 18 / 0 / 0 |

### S1, one comparison per line

| Data (files, size) | Kolmo setting | Rival | Size | Compress speed | Decompress speed | Standing | Files where Kolmo is smaller / within 1% / larger |
|---|---|---|---|---|---|---|---|
| Arrays of numbers (10, 50.2 MB) | kolmo -m balanced | pcodec level 8 | 9.3% smaller | 1.26× | 0.84× | Trade: smaller, slower to decompress | 3 / 1 / 6 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m balanced --native-leaf | pcodec level 8 | 10.0% smaller | 0.87× | 0.81× | Trade: smaller, slower to compress and decompress | 5 / 0 / 5 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m hc | pcodec level 8 | 12.6% smaller | 0.067× | 0.82× | Trade: smaller, slower to compress and decompress | 4 / 1 / 5 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m ratio | pcodec level 8 | 17.6% smaller | 0.044× | 0.069× | Trade: smaller, slower to compress and decompress | 7 / 1 / 2 |
| Arrays of numbers (10, 50.2 MB) | kolmo -x | pcodec level 8 | 21.5% smaller | 0.0048× | 0.0011× | Trade: smaller, slower to compress and decompress | 9 / 1 / 0 |
| Arrays of numbers (10, 50.2 MB) | kolmo -x -l 2 | pcodec level 8 | 21.9% smaller | 0.0029× | 0.0010× | Trade: smaller, slower to compress and decompress | 10 / 0 / 0 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m balanced | pcodec level 12 | 7.7% smaller | 1.88× | 0.86× | Trade: smaller, slower to decompress | 3 / 0 / 7 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m balanced --native-leaf | pcodec level 12 | 8.5% smaller | 1.30× | 0.82× | Trade: smaller, slower to decompress | 5 / 0 / 5 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m hc | pcodec level 12 | 11.0% smaller | 0.10× | 0.83× | Trade: smaller, slower to compress and decompress | 4 / 0 / 6 |
| Arrays of numbers (10, 50.2 MB) | kolmo -m ratio | pcodec level 12 | 16.2% smaller | 0.065× | 0.070× | Trade: smaller, slower to compress and decompress | 7 / 1 / 2 |
| Arrays of numbers (10, 50.2 MB) | kolmo -x | pcodec level 12 | 20.1% smaller | 0.0072× | 0.0011× | Trade: smaller, slower to compress and decompress | 9 / 1 / 0 |
| Arrays of numbers (10, 50.2 MB) | kolmo -x -l 2 | pcodec level 12 | 20.6% smaller | 0.0044× | 0.0011× | Trade: smaller, slower to compress and decompress | 10 / 0 / 0 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -m balanced | pcodec level 8 | 23.1% smaller | 1.21× | 1.06× | Ahead | 1 / 0 / 1 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -m hc | pcodec level 8 | 30.8% smaller | 0.039× | 1.02× | Trade: smaller, slower to compress | 1 / 0 / 1 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -m ratio | pcodec level 8 | 33.2% smaller | 0.12× | 0.087× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -x | pcodec level 8 | 40.8% smaller | 0.0081× | 0.0082× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -m balanced | pcodec level 12 | 18.3% smaller | 2.27× | 1.07× | Ahead | 1 / 0 / 1 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -m hc | pcodec level 12 | 26.5% smaller | 0.072× | 1.03× | Trade: smaller, slower to compress | 1 / 0 / 1 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -m ratio | pcodec level 12 | 29.0% smaller | 0.23× | 0.088× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 64-bit floats in NumPy files (2, 24.9 MB) | kolmo -x | pcodec level 12 | 37.0% smaller | 0.015× | 0.0083× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -m balanced | pcodec level 8 | 10.2% smaller | 1.73× | 0.67× | Trade: smaller, slower to decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -m hc | pcodec level 8 | 11.2% smaller | 0.086× | 0.64× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -m ratio | pcodec level 8 | 15.3% smaller | 0.031× | 0.054× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -x | pcodec level 8 | 17.3% smaller | 0.0043× | 0.0007× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -m balanced | pcodec level 12 | 9.8% smaller | 2.51× | 0.68× | Trade: smaller, slower to decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -m hc | pcodec level 12 | 10.8% smaller | 0.13× | 0.66× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -m ratio | pcodec level 12 | 14.9% smaller | 0.045× | 0.055× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: 16-bit images (2, 18.4 MB) | kolmo -x | pcodec level 12 | 16.9% smaller | 0.0063× | 0.0007× | Trade: smaller, slower to compress and decompress | 2 / 0 / 0 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m balanced | pcodec level 8 | 21.4% larger | 0.85× | 1.13× | Behind: larger, though faster to decompress | 0 / 1 / 5 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m balanced --native-leaf | pcodec level 8 | 15.7% larger | 0.47× | 0.88× | Behind: larger | 2 / 0 / 4 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m hc | pcodec level 8 | 15.3% larger | 0.11× | 1.15× | Behind: larger, though faster to decompress | 1 / 1 / 4 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m ratio | pcodec level 8 | 0.3% larger | 0.047× | 0.097× | Behind: same size, slower to compress and decompress | 3 / 1 / 2 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -x | pcodec level 8 | 6.3% smaller | 0.0040× | 0.0009× | Trade: smaller, slower to compress and decompress | 5 / 1 / 0 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -x -l 2 | pcodec level 8 | 10.0% smaller | 0.0052× | 0.0013× | Trade: smaller, slower to compress and decompress | 6 / 0 / 0 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m balanced | pcodec level 12 | 21.8% larger | 0.96× | 1.17× | Behind: larger, though faster to decompress | 0 / 0 / 6 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m balanced --native-leaf | pcodec level 12 | 16.1% larger | 0.53× | 0.91× | Behind: larger | 2 / 0 / 4 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m hc | pcodec level 12 | 15.7% larger | 0.12× | 1.19× | Behind: larger, though faster to decompress | 1 / 0 / 5 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -m ratio | pcodec level 12 | 0.6% larger | 0.052× | 0.10× | Behind: same size, slower to compress and decompress | 3 / 1 / 2 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -x | pcodec level 12 | 6.0% smaller | 0.0045× | 0.0009× | Trade: smaller, slower to compress and decompress | 5 / 1 / 0 |
| Arrays of numbers: raw signed integers (6, 6.8 MB) | kolmo -x -l 2 | pcodec level 12 | 9.6% smaller | 0.0059× | 0.0013× | Trade: smaller, slower to compress and decompress | 6 / 0 / 0 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m balanced | JPEG XL lossless, effort 7 | 29.5% larger | 49.2× | 16.3× | Behind: larger, though faster to compress and decompress | 0 / 0 / 2 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m hc | JPEG XL lossless, effort 7 | 25.3% larger | 2.55× | 16.3× | Behind: larger, though faster to compress and decompress | 0 / 0 / 2 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m ratio | JPEG XL lossless, effort 7 | 15.6% larger | 1.53× | 1.34× | Behind: larger, though faster to compress and decompress | 0 / 0 / 2 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -x | JPEG XL lossless, effort 7 | 1.1% smaller | 0.22× | 0.032× | Trade: smaller, slower to compress and decompress | 1 / 1 / 0 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m balanced | JPEG XL lossless, effort 9 | 30.0% larger | 294× | 15.7× | Behind: larger, though faster to compress and decompress | 0 / 0 / 2 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m hc | JPEG XL lossless, effort 9 | 25.9% larger | 15.2× | 15.6× | Behind: larger, though faster to compress and decompress | 0 / 0 / 2 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -m ratio | JPEG XL lossless, effort 9 | 16.1% larger | 9.15× | 1.29× | Behind: larger, though faster to compress and decompress | 0 / 0 / 2 |
| Raw 8-bit images (2, 1.0 MB) | kolmo -x | JPEG XL lossless, effort 9 | 0.7% smaller | 1.32× | 0.031× | Same size: faster to compress, slower to decompress | 1 / 1 / 0 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m balanced | FLAC level 5 | 34.4% larger | 0.42× | 4.93× | Behind: larger, though faster to decompress | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m balanced --typed-huf | FLAC level 5 | 26.5% larger | 2.55× | 5.14× | Behind: larger, though faster to compress and decompress | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m hc | FLAC level 5 | 21.8% larger | 0.047× | 4.29× | Behind: larger, though faster to decompress | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m ratio | FLAC level 5 | 17.8% larger | 0.021× | 0.29× | Behind: larger | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -x | FLAC level 5 | 13.6% larger | 0.0044× | 0.0048× | Behind: larger | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -x -l 2 | FLAC level 5 | 11.5% larger | 0.0022× | 0.0021× | Behind: larger | 0 / 1 / 39 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m balanced | FLAC level 8 | 38.3% larger | 0.70× | 4.95× | Behind: larger, though faster to decompress | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m balanced --typed-huf | FLAC level 8 | 30.2% larger | 4.22× | 5.16× | Behind: larger, though faster to compress and decompress | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m hc | FLAC level 8 | 25.4% larger | 0.078× | 4.31× | Behind: larger, though faster to decompress | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -m ratio | FLAC level 8 | 21.2% larger | 0.035× | 0.29× | Behind: larger | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -x | FLAC level 8 | 16.8% larger | 0.0073× | 0.0048× | Behind: larger | 0 / 0 / 40 |
| Audio, 16-bit PCM (40, 17.6 MB) | kolmo -x -l 2 | FLAC level 8 | 14.7% larger | 0.0036× | 0.0021× | Behind: larger | 0 / 1 / 39 |

### A2, inputs under 1 MB: time of one call in milliseconds, compress / decompress

| File | Bytes | lz4 | kolmo -m speed | zstd -1 | kolmo -m fast -l 1 -Q | zstd -3 | kolmo -m balanced |
|---|---|---|---|---|---|---|---|
| log_timestamps_ms.i64 | 64,344 | 0.025 / 0.008 | 0.144 / 0.007 | 0.034 / 0.017 | 0.141 / 0.018 | 0.033 / 0.018 | 0.938 / 0.013 |
| year.i32 | 93,508 | 0.005 / 0.001 | 0.124 / 0.001 | 0.011 / 0.004 | 0.129 / 0.006 | 0.011 / 0.004 | 1.194 / 0.016 |
| population.i64 | 151,152 | 0.143 / 0.029 | 0.353 / 0.033 | 0.239 / 0.109 | 0.356 / 0.042 | 0.244 / 0.107 | 2.209 / 0.058 |
| img_camera.u8 | 262,144 | 0.153 / 0.030 | 0.194 / 0.030 | 0.302 / 0.157 | 0.347 / 0.162 | 0.562 / 0.185 | 1.573 / 0.778 |
| img_astronaut.u8 | 786,432 | 0.158 / 0.020 | 0.218 / 0.030 | 0.548 / 0.304 | 0.609 / 0.317 | 1.501 / 0.350 | 3.840 / 1.159 |

## Checks

Drift: the control settings at the start and again at the end of each tier.

| Tier | File | Setting | Compress MB/s, first | end | change | Decompress MB/s, first | end | change |
|---|---|---|---|---|---|---|---|---|
| A1 | silesia.tar | kolmo -m speed | 1055 | 1016 | -3.7% | 6066 | 5829 | -3.9% |
| A1 | silesia.tar | kolmo -m balanced | 481 | 472 | -1.9% | 1857 | 1804 | -2.8% |
| A1 | silesia.tar | lz4 | 1067 | 1028 | -3.6% | 6945 | 6620 | -4.7% |
| A1 | silesia.tar | zstd -3 | 542 | 531 | -2.1% | 2187 | 2145 | -2.0% |
| A2 | api.json | kolmo -m speed | 1786 | 1730 | -3.2% | 11053 | 10686 | -3.3% |
| A2 | api.json | kolmo -m balanced | 1157 | 1150 | -0.6% | 4740 | 4671 | -1.5% |
| A2 | api.json | lz4 | 1946 | 1868 | -4.0% | 9120 | 8775 | -3.8% |
| A2 | api.json | zstd -3 | 1547 | 1539 | -0.5% | 5644 | 5565 | -1.4% |
| A2 | dickens | kolmo -m speed | 669 | 667 | -0.3% | 5044 | 5026 | -0.4% |
| A2 | dickens | kolmo -m balanced | 324 | 322 | -0.8% | 1723 | 1720 | -0.2% |
| A2 | dickens | lz4 | 663 | 663 | -0.0% | 5417 | 5374 | -0.8% |
| A2 | dickens | zstd -3 | 345 | 342 | -0.9% | 1823 | 1822 | -0.0% |
| A2 | mozilla | kolmo -m speed | 1052 | 1046 | -0.5% | 6013 | 6022 | +0.2% |
| A2 | mozilla | kolmo -m balanced | 401 | 400 | -0.2% | 1862 | 1860 | -0.1% |
| A2 | mozilla | lz4 | 1081 | 1081 | +0.0% | 6592 | 6584 | -0.1% |
| A2 | mozilla | zstd -3 | 577 | 577 | -0.0% | 1923 | 1919 | -0.2% |
| A3 | ecg_2ch_i16.npy | kolmo -m balanced | 171 | 172 | +0.6% | 854 | 905 | +5.9% |
| A3 | ecg_2ch_i16.npy | zstd -3 | 320 | 321 | +0.1% | 1494 | 1495 | +0.1% |

| Tier | Rows | Clean | Marked `threads` | Marked `busy` | Failed | Rows with another busy process | Which (rows) |
|---|---|---|---|---|---|---|---|
| A1 | 45 | 45 | 0 | 0 | 0 | 41 | spotlightknowledged (41), signpost_reporter (5), corespotlightd (1) |
| A2 | 782 | 782 | 0 | 0 | 0 | 47 | corespotlightd (16), contactsd (8), searchpartyuseragent (6), remindd (4) |
| A3 | 263 | 263 | 0 | 0 | 0 | 27 | logd (9), contactsd (6), XProtectRemediatorBundlore (5), XProtectRemediatorPirrit (4) |
| S1 | 628 | 628 | 0 | 0 | 0 | 103 | XProtectRemediatorSheepSwap (22), XProtectRemediatorDolittle (19), XProtectRemediatorMRTv3 (17), corespotlightd (13) |
| S2 | 222 | 221 | 1 | 0 | 0 | 30 | corespotlightd (17), trustd (4), sysmond (4), ScreenTimeAgent (3) |

- S2, ooffice, 7zz -t7z -mx9 -myx9 -mmt1 (LZMA2, filter picked by 7-Zip): threads

Release build against the control build (generic integer inverse in the decoder), same frames:

| Tier | Files | Setting | Same sizes | Compress speed, release / control | Decompress speed, release / control |
|---|---|---|---|---|---|
| A1 | 1 | -m balanced | yes | 1.01× | 1.02× |
| A2 | 35 | -m balanced | yes | 1.00× | 1.00× |
| A3 | 18 | -m balanced | yes | 1.00× | 1.07× |
| A3 | 18 | -m balanced --typed-huf | yes | 1.00× | 1.70× |

