## Per input and arm (medians over rounds; MB/s = 10^6 raw bytes / wall seconds; RSS in MiB)

| input | arm | rounds | stored B | enc MB/s | dec MB/s | combined MB/s | enc / dec RSS MiB | enc / dec CPU s | init enc / dec s | 1-min load per round | round trips |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| dev_dickens4m | stock | 3 | 886,241 | 0.9309 | 0.9834 | 0.4774 | 186.0 / 151.3 | 4.50 / 4.26 | — | [4.9, 2.9, 2.5] | all SHA-256 OK |
| dev_dickens4m | control | 3 | 886,193 | 1.0653 | 0.9820 | 0.5112 | 155.1 / 151.1 | 3.93 / 4.27 | — | [8.5, 3.0, 2.6] | all SHA-256 OK |
| dev_dickens4m | candidate | 3 | 880,692 | 0.0301 | 0.0300 | 0.0150 | 157.3 / 153.3 | 139.18 / 139.90 | 0.0006 / 0.0006 | [7.2, 4.2, 2.7] | all SHA-256 OK |
| dev_dickens4m | zpaq | 3 | 892,624 | 0.6536 | 0.6245 | 0.3188 | 195.8 / 197.4 | 6.39 / 6.71 | — | [6.8, 4.3, 2.4] | all SHA-256 OK |
| holdout_reymont | stock | 3 | 874,420 | 1.0618 | 1.0123 | 0.5177 | 262.4 / 229.8 | 6.24 / 6.54 | — | [7.7, 3.1, 3.0] | all SHA-256 OK |
| holdout_reymont | control | 3 | 874,372 | 1.1010 | 1.0126 | 0.5277 | 235.9 / 229.5 | 6.01 / 6.54 | — | [5.5, 4.5, 2.4] | all SHA-256 OK |
| holdout_reymont | candidate | 3 | 832,051 | 0.0304 | 0.0304 | 0.0152 | 238.5 / 232.1 | 218.12 / 217.66 | 0.0006 / 0.0006 | [2.6, 3.2, 3.0] | all SHA-256 OK |
| holdout_reymont | zpaq | 3 | 956,498 | 0.6248 | 0.6034 | 0.3064 | 211.3 / 211.5 | 10.60 / 10.98 | — | [6.1, 3.0, 2.3] | all SHA-256 OK |
| tiny1 | stock | 3 | 53 | 0.0003 | 0.0004 | 0.0002 | 4.6 / 1.9 | 0.00 / 0.00 | — | [6.1, 4.2, 2.3] | all SHA-256 OK |
| tiny1 | control | 3 | 20 | 0.0003 | 0.0004 | 0.0002 | 3.4 / 3.5 | 0.00 / 0.00 | — | [7.7, 3.1, 2.4] | all SHA-256 OK |
| tiny1 | candidate | 3 | 28 | 0.0003 | 0.0003 | 0.0002 | 4.4 / 4.4 | 0.00 / 0.00 | 0.0005 / 0.0005 | [7.2, 3.2, 2.4] | all SHA-256 OK |
| tiny1 | zpaq | 3 | 1,014 | 0.0001 | 0.0001 | 0.0000 | 73.4 / 73.2 | 0.01 / 0.01 | — | [6.8, 4.3, 2.4] | all SHA-256 OK |

## Startup: a 1-byte input (process start, model allocation, initialisation; medians)

| arm | encode ms | decode ms | enc / dec RSS MiB | in-process init enc / dec ms |
|---|---:|---:|---:|---:|
| stock | 3.5 | 2.4 | 4.6 / 1.9 | — |
| control | 3.1 | 2.4 | 3.4 / 3.5 | — |
| candidate | 3.7 | 2.9 | 4.4 / 4.4 | 0.54 / 0.54 |
| zpaq | 13.4 | 12.3 | 73.4 / 73.2 | — |

## Gates: candidate against stock Ultra (prior-routing archival gate)

| input | bytes ratio (<= 0.98) | enc thr (>= 0.67) | dec thr (>= 0.95) | enc RSS (<= 1.10) | dec RSS (<= 1.10) | combined latency (<= 1.50) | verdict |
|---|---:|---:|---:|---:|---:|---:|---|
| dev_dickens4m | 0.994 **FAIL** | 0.032 **FAIL** | 0.030 **FAIL** | 0.846 pass | 1.013 pass | 31.772 **FAIL** | **REJECT** |
| holdout_reymont | 0.952 pass | 0.029 **FAIL** | 0.030 **FAIL** | 0.909 pass | 1.010 pass | 34.126 **FAIL** | **REJECT** |

## Isolating the LSTM: candidate against the harness control (the same container and process, LSTM off)

| input | bytes ratio | enc time ratio | dec time ratio | candidate − control stored B |
|---|---:|---:|---:|---:|
| dev_dickens4m | 0.9938 | 35.36 | 32.77 | -5,501 |
| holdout_reymont | 0.9516 | 36.25 | 33.27 | -42,321 |
| tiny1 | 1.4000 | 1.18 | 1.21 | +8 |

## Hashes

| input | arm | input SHA-256 | frame SHA-256 | restored SHA-256 |
|---|---|---|---|---|
| dev_dickens4m | stock | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` | `8f8e47755fa205cac71d29d360eb35d43a8af0d3803e79ea7f11359cf4bb9584` | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` |
| dev_dickens4m | control | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` | `b8799e84be2be0f01c3d443fbb872cddae6d342ef408f0f072affad10acd1d54` | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` |
| dev_dickens4m | candidate | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` | `3df42aad799891f077b7fdfb0284b7b70a0aa353b4246621b2588258997acea2` | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` |
| dev_dickens4m | zpaq | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` | `c4fb527a2bb784f748f55e662b174d1047ea46216c49fff398bfcaed394e3170 e0963193e751b10420e866eaf9ed37a818a151fced4400a8cbd94343f11f68e5 e8da0c14c57814b0b9de8db6029f1122cf10ef882d203b0dc2e8fcebe886469f` | `6c7a2dec937a9b03d80bfc011b3a30b316a961e0633e5d6ccf581710df386215` |
| holdout_reymont | stock | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` | `0137e1b0893dcc7da503356671b4b9e8af61eb0e109971c6e31dcf080a96af16` | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` |
| holdout_reymont | control | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` | `1e21d55b2ff1314ae486c567e832de3c08a96fb40d10f7afecc86726d05e2032` | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` |
| holdout_reymont | candidate | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` | `540493c338fda32a7df9407ac67679fe9b0363cdfa73632dcf69e8f2e2d6529d` | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` |
| holdout_reymont | zpaq | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` | `5ba1e74a8cf29cd506d5d50beb3db8375a67161d50106508f552c859b643c702 8bc23e2dac83fd659b9d4a62253d080a3639579988cac7b8540f3e587a2142af eb4bb75a30b273fa1e550c6527cf7022cd3c062c760924841cf9837a65faf2e1` | `0eac0114a3dfe6e2ee1f345a0f79d653cb26c3bc9f0ed79238af4933422b7578` |
| tiny1 | stock | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` | `65a84c2b31bb57d50596b42ae51b536321c4bfece8fecb901bc6d0c8a1ca401e` | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` |
| tiny1 | control | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` | `bfb9f8cbaec4d51c91675b69a43e6bd18eb58cae7803e3a8b0d050d9520d9af8` | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` |
| tiny1 | candidate | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` | `be028ecb78a3b0cfe6a31adbd436e0ba602aff1e916a00bdae758889db4dd702` | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` |
| tiny1 | zpaq | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` | `64216c1b9e49a64fdffb11c7ffe592d83ee9bf257153f175e7bff2c121a60836 6cc536322ffe3e4e67e8782d0863ccc1acd8144e9f2bdd847f906d8cab5e1b77 e082676e9c7853b1f1c5b803d6de6b28aa6f31fc16f141b8dc63241b78139f3e` | `86be9a55762d316a3026c2836d044f5fc76e34da10e1b45feee5f18be7edb177` |

## Run order

r1__holdout_reymont__candidate → r1__tiny1__candidate → r1__dev_dickens4m__candidate → r1__dev_dickens4m__control → r1__tiny1__control → r1__holdout_reymont__stock → r1__tiny1__zpaq → r1__dev_dickens4m__zpaq → r1__tiny1__stock → r1__holdout_reymont__zpaq → r1__holdout_reymont__control → r1__dev_dickens4m__stock → r2__holdout_reymont__control → r2__tiny1__zpaq → r2__dev_dickens4m__zpaq → r2__tiny1__stock → r2__dev_dickens4m__candidate → r2__tiny1__candidate → r2__holdout_reymont__candidate → r2__dev_dickens4m__stock → r2__holdout_reymont__zpaq → r2__tiny1__control → r2__holdout_reymont__stock → r2__dev_dickens4m__control → r3__holdout_reymont__stock → r3__holdout_reymont__candidate → r3__dev_dickens4m__stock → r3__holdout_reymont__control → r3__dev_dickens4m__zpaq → r3__tiny1__stock → r3__holdout_reymont__zpaq → r3__dev_dickens4m__control → r3__dev_dickens4m__candidate → r3__tiny1__control → r3__tiny1__candidate → r3__tiny1__zpaq
