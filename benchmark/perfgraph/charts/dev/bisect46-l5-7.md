# Interleaved A/B check (30 rounds, max load1 9.1)

Change = median over rounds of (arm time / A time) - 1, within the same round; negative = faster than A. Speed change is the inverse. 'All inputs' sums each round's times over the inputs first.

## Level 5

| Input | pr45-5b1327f0 encode time | pr45-5b1327f0 decode time | 00-cda7bf4b encode time | 00-cda7bf4b decode time | 01-r76-5a317a15 encode time | 01-r76-5a317a15 decode time | 02-r144-1fe603dd encode time | 02-r144-1fe603dd decode time | 03-r148-0893f1cc encode time | 03-r148-0893f1cc decode time | 04-r149-33dc6632 encode time | 04-r149-33dc6632 decode time | 05-r64-ce4988aa encode time | 05-r64-ce4988aa decode time | 06-r61-55b3a958 encode time | 06-r61-55b3a958 decode time | 07-r60-e522441d encode time | 07-r60-e522441d decode time | 08-r141-23bc0a69 encode time | 08-r141-23bc0a69 decode time | 09-r62-98791406 encode time | 09-r62-98791406 decode time | 10-r83-e519a2c0 encode time | 10-r83-e519a2c0 decode time | 11-r82-08a4c68d encode time | 11-r82-08a4c68d decode time | A2 encode time | A2 decode time | bytes same |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| owid-f64 | +0.3% | +0.1% | +1.2% | -0.8% | +1.2% | -2.3% | +0.7% | -1.5% | +84.3% | -3.0% | +83.2% | +0.0% | +83.9% | -0.6% | +83.2% | +2.0% | +85.6% | -2.3% | +84.1% | +0.5% | +85.3% | -1.3% | +85.6% | -1.5% | +83.6% | -1.1% | +2.9% | -0.7% | yes |
| population-i64 | -9.0% | +0.5% | +0.6% | -3.5% | +7.6% | -5.6% | -11.9% | -1.7% | +102.1% | -2.4% | +98.1% | +2.5% | +108.4% | -5.0% | +121.3% | +3.1% | +111.9% | -0.8% | +104.8% | -1.1% | +107.7% | +3.1% | +125.6% | -0.2% | +115.5% | -3.6% | +1.7% | +13.0% | yes |
| **All inputs** | -3.9% | +1.1% | +2.1% | -2.0% | +3.8% | -1.7% | -2.9% | -0.3% | +92.7% | -3.1% | +87.9% | +1.3% | +91.8% | -0.5% | +90.8% | +3.0% | +88.3% | -0.1% | +93.9% | -0.3% | +92.0% | +2.8% | +94.1% | -1.2% | +93.7% | -2.4% | +3.3% | +5.4% | yes |

## Level 6

| Input | pr45-5b1327f0 encode time | pr45-5b1327f0 decode time | 00-cda7bf4b encode time | 00-cda7bf4b decode time | 01-r76-5a317a15 encode time | 01-r76-5a317a15 decode time | 02-r144-1fe603dd encode time | 02-r144-1fe603dd decode time | 03-r148-0893f1cc encode time | 03-r148-0893f1cc decode time | 04-r149-33dc6632 encode time | 04-r149-33dc6632 decode time | 05-r64-ce4988aa encode time | 05-r64-ce4988aa decode time | 06-r61-55b3a958 encode time | 06-r61-55b3a958 decode time | 07-r60-e522441d encode time | 07-r60-e522441d decode time | 08-r141-23bc0a69 encode time | 08-r141-23bc0a69 decode time | 09-r62-98791406 encode time | 09-r62-98791406 decode time | 10-r83-e519a2c0 encode time | 10-r83-e519a2c0 decode time | 11-r82-08a4c68d encode time | 11-r82-08a4c68d decode time | A2 encode time | A2 decode time | bytes same |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| owid-f64 | -0.1% | +0.2% | -0.7% | +2.0% | -0.2% | +2.3% | -0.4% | -0.7% | +105.0% | +1.5% | +103.1% | +3.1% | +103.7% | +1.6% | +103.4% | +1.2% | +103.1% | +4.0% | +106.1% | +2.0% | +104.3% | -1.6% | +104.1% | -0.6% | +103.4% | -1.2% | -0.6% | +0.9% | yes |
| population-i64 | +1.2% | +4.8% | +1.1% | +4.4% | +3.0% | +13.9% | +3.8% | +1.9% | +150.1% | +8.1% | +149.7% | +4.6% | +144.1% | +7.1% | +160.9% | +4.7% | +160.2% | +7.2% | +149.1% | +9.7% | +150.2% | +12.0% | +143.9% | -1.4% | +143.8% | +2.6% | +1.4% | -3.3% | yes |
| **All inputs** | +0.1% | +0.8% | -0.8% | +2.8% | +0.4% | +8.7% | +0.4% | -0.4% | +115.5% | +3.9% | +112.9% | +4.1% | +111.4% | +3.1% | +119.2% | +2.6% | +115.2% | +5.7% | +115.1% | +4.3% | +115.9% | +4.7% | +113.2% | +0.9% | +111.6% | -0.3% | -0.2% | +0.9% | yes |

## Level 7

| Input | pr45-5b1327f0 encode time | pr45-5b1327f0 decode time | 00-cda7bf4b encode time | 00-cda7bf4b decode time | 01-r76-5a317a15 encode time | 01-r76-5a317a15 decode time | 02-r144-1fe603dd encode time | 02-r144-1fe603dd decode time | 03-r148-0893f1cc encode time | 03-r148-0893f1cc decode time | 04-r149-33dc6632 encode time | 04-r149-33dc6632 decode time | 05-r64-ce4988aa encode time | 05-r64-ce4988aa decode time | 06-r61-55b3a958 encode time | 06-r61-55b3a958 decode time | 07-r60-e522441d encode time | 07-r60-e522441d decode time | 08-r141-23bc0a69 encode time | 08-r141-23bc0a69 decode time | 09-r62-98791406 encode time | 09-r62-98791406 decode time | 10-r83-e519a2c0 encode time | 10-r83-e519a2c0 decode time | 11-r82-08a4c68d encode time | 11-r82-08a4c68d decode time | A2 encode time | A2 decode time | bytes same |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| owid-f64 | -0.3% | +0.7% | -0.1% | +0.3% | -1.1% | -1.4% | -1.6% | +0.3% | +102.7% | -2.3% | +98.2% | -4.4% | +96.8% | -0.6% | +98.8% | -1.0% | +95.4% | +0.2% | +96.3% | -2.7% | +97.3% | -0.1% | +97.1% | -3.2% | +97.4% | -0.1% | -0.8% | -0.1% | yes |
| population-i64 | +1.1% | +3.4% | +0.5% | +1.9% | -0.3% | +4.6% | -0.6% | +5.2% | +181.5% | +2.5% | +178.5% | +5.5% | +180.2% | +1.5% | +181.0% | +8.7% | +178.0% | +6.0% | +177.6% | +8.4% | +177.9% | +2.1% | +180.8% | +8.7% | +178.1% | +3.0% | +0.6% | +2.6% | yes |
| **All inputs** | -0.3% | +2.1% | -0.2% | -0.5% | -1.1% | -0.7% | -1.7% | +1.0% | +107.5% | +0.0% | +103.2% | -0.8% | +102.3% | +0.4% | +104.2% | +3.8% | +100.8% | -0.2% | +101.1% | +0.3% | +102.6% | +0.3% | +102.6% | -0.9% | +102.0% | +2.3% | -0.7% | +1.1% | yes |

