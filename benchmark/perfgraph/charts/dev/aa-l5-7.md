# Interleaved A/B check (255 rounds, max load1 8.6)

Change = median over rounds of (arm time / A time) - 1, within the same round; negative = faster than A. Speed change is the inverse. 'All inputs' sums each round's times over the inputs first.

## Level 5

| Input | B encode time | B decode time | A2 encode time | A2 decode time | bytes same |
|---|---:|---:|---:|---:|---|
| api | +14.5% | +2.6% | +0.4% | +0.4% | yes |
| bf16-10mb | +37.0% | -2.2% | +0.1% | +0.1% | yes |
| canterbury | +28.9% | -4.6% | -0.1% | -1.0% | yes |
| dickens | +11.6% | -4.7% | +5.8% | +0.3% | yes |
| energy-130col | +4.6% | -1.3% | +0.1% | -2.0% | yes |
| flac-c | +23.8% | -1.4% | +0.7% | +0.4% | yes |
| ghcn | +8.6% | +0.4% | -0.1% | +0.5% | yes |
| ghcn-rows | +8.4% | +4.4% | -0.2% | +0.5% | yes |
| hubble | +8.2% | -7.1% | -2.3% | +0.8% | yes |
| loghub | +27.0% | -2.8% | -0.3% | -0.6% | yes |
| magika-onnx | +50.5% | -5.6% | +0.5% | -2.1% | yes |
| ooffice-x86 | +7.2% | -9.2% | +0.3% | +0.2% | yes |
| owid-f64 | +84.2% | +0.9% | +0.0% | +0.3% | yes |
| population-i64 | +123.1% | +1.9% | +0.3% | +2.8% | yes |
| retina | +17.7% | -0.6% | -0.4% | +0.6% | yes |
| silesia-xml | +11.5% | -1.3% | +0.6% | -0.5% | yes |
| x-ray | +9.1% | +0.6% | -0.5% | +1.4% | yes |
| **All inputs** | +19.1% | -2.0% | +0.2% | +0.1% | yes |

## Level 6

| Input | B encode time | B decode time | A2 encode time | A2 decode time | bytes same |
|---|---:|---:|---:|---:|---|
| api | +17.3% | -2.4% | +0.1% | -0.6% | yes |
| bf16-10mb | +2.4% | -2.8% | -0.1% | -2.9% | yes |
| canterbury | +29.2% | -6.0% | -0.0% | -2.1% | yes |
| dickens | +11.3% | -6.5% | -0.4% | -1.4% | yes |
| energy-130col | +4.6% | -3.0% | +0.5% | -0.4% | yes |
| flac-c | +25.1% | -5.6% | +0.9% | -4.1% | yes |
| ghcn | +9.7% | +3.4% | +0.4% | +1.5% | yes |
| ghcn-rows | +11.2% | +3.6% | +0.0% | +0.6% | yes |
| hubble | +6.9% | -7.8% | -2.4% | +1.5% | yes |
| loghub | +28.9% | -1.8% | -1.7% | +0.6% | yes |
| magika-onnx | +8.6% | -0.4% | -0.3% | +0.0% | yes |
| ooffice-x86 | +7.8% | -9.6% | +1.6% | +2.0% | yes |
| owid-f64 | +106.1% | +0.9% | +0.2% | -1.3% | yes |
| population-i64 | +144.4% | +3.7% | +0.3% | -0.6% | yes |
| retina | +13.5% | -2.3% | -0.7% | +1.1% | yes |
| silesia-xml | +15.3% | -1.5% | +0.5% | +3.2% | yes |
| x-ray | +9.4% | +1.6% | +2.3% | +0.1% | yes |
| **All inputs** | +15.6% | -3.3% | +0.2% | -0.5% | yes |

## Level 7

| Input | B encode time | B decode time | A2 encode time | A2 decode time | bytes same |
|---|---:|---:|---:|---:|---|
| api | +20.5% | -0.9% | +0.3% | -0.3% | yes |
| bf16-10mb | +1.2% | -2.4% | +0.1% | -1.5% | yes |
| canterbury | +19.8% | -5.0% | -0.0% | -0.2% | yes |
| dickens | +11.2% | -6.1% | +1.5% | -0.3% | yes |
| energy-130col | +14.4% | +1.9% | +0.2% | +1.5% | yes |
| flac-c | +18.9% | -1.0% | -0.9% | +0.9% | yes |
| ghcn | +6.2% | +1.5% | +0.2% | +0.4% | yes |
| ghcn-rows | +6.1% | +2.8% | -0.1% | -0.8% | yes |
| hubble | +10.3% | -9.4% | -0.9% | -0.3% | yes |
| loghub | +31.8% | +1.1% | +0.1% | +1.5% | yes |
| magika-onnx | +7.2% | -0.4% | -0.1% | -2.6% | yes |
| ooffice-x86 | +5.5% | -8.8% | +0.5% | +0.3% | yes |
| owid-f64 | +95.3% | -0.1% | +0.1% | +1.3% | yes |
| population-i64 | +177.9% | +1.4% | +0.1% | -2.9% | yes |
| retina | +16.5% | -3.3% | -0.3% | -0.5% | yes |
| silesia-xml | +16.1% | -3.1% | -0.3% | -1.8% | yes |
| x-ray | +0.6% | +0.9% | -0.1% | +1.7% | yes |
| **All inputs** | +23.4% | -2.5% | -0.4% | +0.1% | yes |

