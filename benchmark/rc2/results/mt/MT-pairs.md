
## general
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m speed -T 10 | lz4 -1 -T0 | 7 | -0.1% | 1.40x | 1.92x |
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 7 | -15.8% | 0.66x | 2.62x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 7 | -8.7% | 1.15x | 2.73x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 7 | -6.5% | 2.10x | 1.77x |
| kolmo -m hc -T 10 | zstd -19 --long=27 -T0 | 7 | -6.0% | 2.96x | 1.79x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 7 | -12.7% | 2.54x | 1.68x |
| kolmo -m ratio -T 10 | 7-Zip LZMA2 -mx9 -mmt=10 | 7 | -12.6% | 1.80x | 1.27x |
| kolmo -m ratio -T 10 | bsc -m0 -e2 (its own threads) | 7 | -12.5% | 0.31x | 5.84x |
| kolmo -m ratio -T 10 | bzip3 -b 16 -j 10 | 7 | -12.5% | 0.31x | 4.07x |
| kolmo -x -T 10 | zpaq -m5 -t10 | 7 | -7.4% | 0.58x | 0.52x |
| krans order 0/1 auto -T 10 (entropy coding only) | zstd -3 -T0 | 7 | +56.5% | 0.45x | 0.96x |
| kolmo -m eco -T 10 | brotli -11 (no threads) | 7 | +5.6% | 79.81x | 2.54x |

## genomics
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m speed -T 10 | lz4 -1 -T0 | 4 | +0.2% | 1.24x | 2.26x |
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 4 | -2.9% | 0.76x | 2.85x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 4 | -0.0% | 1.13x | 3.07x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 4 | +2.4% | 1.63x | 1.85x |
| kolmo -m hc -T 10 | zstd -19 --long=27 -T0 | 4 | +10.1% | 2.29x | 1.76x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 4 | -4.0% | 21.13x | 0.46x |
| kolmo -m ratio -T 10 | 7-Zip LZMA2 -mx9 -mmt=10 | 4 | -4.4% | 17.82x | 0.33x |
| kolmo -m ratio -T 10 | bsc -m0 -e2 (its own threads) | 4 | -3.9% | 1.23x | 1.56x |
| kolmo -m ratio -T 10 | bzip3 -b 16 -j 10 | 4 | -4.5% | 1.53x | 1.64x |
| kolmo -x -T 10 | zpaq -m5 -t10 | 4 | -18.0% | 1.06x | 3.41x |
| kfam -T 10 | kolmo -x -T 10 | 4 | +9.9% | 23.38x | 13.33x |
| kfam -T 10 | kolmo -m ratio -T 10 | 4 | -16.2% | 0.26x | 0.35x |
| kolmo -m eco -T 10 | brotli -11 (no threads) | 4 | +27.4% | 45.35x | 2.68x |

## numeric
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m speed -T 10 | lz4 -1 -T0 | 6 | -7.9% | 1.20x | 1.32x |
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 6 | -42.5% | 0.49x | 1.53x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 6 | -36.7% | 1.27x | 1.68x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 6 | -27.3% | 2.60x | 0.95x |
| kolmo -m hc -T 10 | zstd -19 --long=27 -T0 | 6 | -27.3% | 2.77x | 0.93x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 6 | -21.9% | 1.13x | 0.91x |
| kolmo -m ratio -T 10 | 7-Zip LZMA2 -mx9 -mmt=10 | 6 | -21.7% | 0.58x | 0.79x |
| kolmo -m ratio -T 10 | bsc -m0 -e2 (its own threads) | 6 | -5.4% | 0.22x | 2.28x |
| kolmo -m ratio -T 10 | bzip3 -b 16 -j 10 | 6 | -2.3% | 0.67x | 3.89x |
| kolmo -x -T 10 | zpaq -m5 -t10 | 6 | -5.7% | 1.25x | 1.40x |
| kolmo -x -l 2 -T 10 | zpaq -m5 -t10 | 6 | -5.7% | 1.24x | 1.39x |
| kolmo -m eco -T 10 | brotli -11 (no threads) | 6 | -20.0% | 55.30x | 1.14x |

## numeric_npy
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 11 | -46.5% | 0.47x | 1.45x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 11 | -37.4% | 0.96x | 1.49x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 11 | -28.1% | 2.10x | 1.00x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 11 | -21.4% | 1.57x | 0.89x |
| kbin -C -T 10 | kolmo -m ratio -T 10 | 11 | +10.1% | 28.60x | 9.39x |
| kbin -C -T 10 | kolmo -m balanced -T 10 | 11 | +0.0% | 0.93x | 0.91x |
| kbin -T 10 | zstd -19 -T0 | 11 | -21.0% | 35.57x | 1.46x |

## structured
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m speed -T 10 | lz4 -1 -T0 | 9 | +1.0% | 1.45x | 1.61x |
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 9 | -52.0% | 0.60x | 1.56x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 9 | -57.5% | 1.34x | 1.58x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 9 | -53.0% | 5.08x | 0.77x |
| kolmo -m hc -T 10 | zstd -19 --long=27 -T0 | 9 | -52.9% | 5.44x | 0.78x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 9 | -50.6% | 2.36x | 1.41x |
| kolmo -m ratio -T 10 | 7-Zip LZMA2 -mx9 -mmt=10 | 9 | -50.5% | 1.67x | 1.08x |
| kolmo -m ratio -T 10 | bsc -m0 -e2 (its own threads) | 9 | -49.7% | 0.29x | 3.82x |
| kolmo -m ratio -T 10 | bzip3 -b 16 -j 10 | 9 | -49.6% | 0.84x | 6.40x |
| kolmo -x -T 10 | zpaq -m5 -t10 | 9 | -45.3% | 2.47x | 3.40x |
| kfam -T 10 | kolmo -x -T 10 | 9 | +131.6% | 5.59x | 6.45x |
| kfam -T 10 | kolmo -m ratio -T 10 | 9 | +117.2% | 0.53x | 0.06x |
| kolmo -x -l 2 -T 10 | zpaq -m5 -t10 | 9 | -45.7% | 1.69x | 3.05x |
| kolmo -m eco -T 10 | brotli -11 (no threads) | 9 | -47.4% | 86.53x | 1.18x |

## video
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m speed -T 10 | lz4 -1 -T0 | 3 | -0.0% | 1.37x | 1.84x |
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 3 | -27.1% | 0.40x | 1.02x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 3 | -14.1% | 0.89x | 1.15x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 3 | -8.3% | 1.39x | 1.14x |
| kolmo -m hc -T 10 | zstd -19 --long=27 -T0 | 3 | -8.2% | 1.96x | 1.04x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 3 | -5.6% | 0.70x | 0.56x |
| kolmo -m ratio -T 10 | 7-Zip LZMA2 -mx9 -mmt=10 | 3 | -5.7% | 0.47x | 0.40x |
| kolmo -m ratio -T 10 | bsc -m0 -e2 (its own threads) | 3 | +2.3% | 0.09x | 1.60x |
| kolmo -m ratio -T 10 | bzip3 -b 16 -j 10 | 3 | +4.7% | 0.06x | 0.91x |
| kolmo -x -T 10 | zpaq -m5 -t10 | 3 | -8.6% | 0.78x | 1.15x |
| kvid -m balanced -T 10 | kolmo -m balanced -T 10 | 3 | -17.4% | 0.28x | 0.54x |
| kvid -m x -T 10 | kolmo -x -T 10 | 3 | -4.2% | 0.99x | 1.36x |
| kvid -m ratio -T 10 | xz -9 -T0 | 3 | -16.0% | 0.61x | 1.08x |
| kolmo -m eco -T 10 | brotli -11 (no threads) | 3 | -2.5% | 24.59x | 2.03x |

## weights
| Kolmo / research | Rival | Files | Size | Compress | Decompress |
|---|---|---|---|---|---|
| kolmo -m speed -T 10 | lz4 -1 -T0 | 3 | -0.0% | 1.40x | 1.34x |
| kolmo -m fast -q -T 10 | zstd -1 -T0 | 3 | -11.8% | 0.51x | 2.33x |
| kolmo -m balanced -T 10 | zstd -3 -T0 | 3 | -12.8% | 0.85x | 2.50x |
| kolmo -m hc -T 10 | zstd -19 -T0 | 3 | -12.6% | 2.02x | 2.70x |
| kolmo -m hc -T 10 | zstd -19 --long=27 -T0 | 3 | -12.1% | 3.00x | 2.57x |
| kolmo -m ratio -T 10 | xz -9 -T0 | 3 | +1.1% | 1.41x | 1.28x |
| kolmo -m ratio -T 10 | 7-Zip LZMA2 -mx9 -mmt=10 | 3 | +1.1% | 0.45x | 0.97x |
| kolmo -m ratio -T 10 | bsc -m0 -e2 (its own threads) | 3 | -7.7% | 0.26x | 6.60x |
| kolmo -m ratio -T 10 | bzip3 -b 16 -j 10 | 3 | -7.3% | 0.29x | 4.83x |
| kolmo -x -T 10 | zpaq -m5 -t10 | 3 | -6.8% | 1.30x | 4.73x |
| kolmo -m eco -T 10 | brotli -11 (no threads) | 3 | -4.2% | 62.42x | 4.86x |
