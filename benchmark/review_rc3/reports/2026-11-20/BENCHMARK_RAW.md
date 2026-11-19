# BENCHMARK_RAW index — generated 2026-11-19T22:44:49Z

Every trial (accepted, rejected, warmup) is kept in each suite's trials.jsonl.
Each row records:
- input, tool and config sha256;
- argv, environment and thread policy;
- encode and decode wall time, CPU time, average and peak RSS;
- sampled threads and the pre-window load;
- comp_sha256 and decoded_sha256 (the byte check).

Each suite also keeps run.json (identity), events.jsonl, traces/ (RSS and thread samples), logs/ and artifacts/.

| file | sha256 | bytes |
|---|---|---:|
| `screening-4.noindex/br-decmap/trials.jsonl` | 21cb383b919cabd4279b36a0df7cd7434a0b07b11e0136a17a8e52cc5dfd54e4 | 303077 |
| `screening-4.noindex/br-entropy/trials.jsonl` | 50a3d4d4d8685a7a1d91dcf199494261d8ecfdb34ea0ebbf89b42f9517a29686 | 625690 |
| `screening-4.noindex/br-native/trials.jsonl` | e9ea66c6fb9d94f49c31ec954eaa048c032e407867929d44b101f9d62eb1dc26 | 1210052 |
| `screening-4.noindex/br-ratio/trials.jsonl` | efabed6d68d34ceeb8fb877e2e5542ba5b9abe60a877bcc8e50b3dd83102443e | 409189 |
| `screening-4.noindex/presets/trials.jsonl` | a6b4e8bb2e6a174883fa099f6fe770e67cda8dd3dced982dd1e35bb1f663b8a6 | 6751460 |
| `screening-4.noindex/sep-docs/trials.jsonl` | e103fb57db1e9481137cb2662b848ada45d19451b666cb79a23b44bcaaaabe23 | 337458 |
| `screening-4.noindex/ultra/trials.jsonl` | 4298a35e7ff82f47d90aa9464da93a71a890fa86ed34d50ccd179930cc6b9df9 | 776647 |
| `screening-4.noindex/br-decmap/run.json` | 6417951b23e38751ec5c669fe8632f02d6dd0aabf9d2d4718674ecf883f5cb1a | 1270 |
| `screening-4.noindex/br-entropy/run.json` | 311d8f50c5cdbb8978ed92b3bc53a878a67fedf55fe08b0cc0c4f1f326a8e7fe | 1270 |
| `screening-4.noindex/br-native/run.json` | fbd3587c2b194afcd59718459abb3625ddf9369ccd28477dd3e09831329f22cb | 1267 |
| `screening-4.noindex/br-ratio/run.json` | 01a810d00af5c303bfaa6cae56330e04ad20a7d8c376c77bbe54cbe62e54c95b | 1268 |
| `screening-4.noindex/presets/run.json` | d1d59a5a210fd5bbd7c61a253fadfe0a5cd76192419432ebd7e6444deb0f6e49 | 1268 |
| `screening-4.noindex/sep-docs/run.json` | 7440a0db4d15b8f559f7950203db783b266b5234a52503a416f23b2ba1ffe896 | 1266 |
| `screening-4.noindex/ultra/run.json` | 768f06337ab19ee9d9b8b0d058ee39e4ead9ea4a7c139769f3b9d5c1914c5a5f | 1263 |
| `screening-5.noindex/br-decmap-native/trials.jsonl` | c4832b7ff26b41dcd09fe91087f71adf8f46f11c34c690cad5ad2170be6a5fe9 | 727819 |
| `screening-5.noindex/br-decmap-native/run.json` | 3965d326f8c4977e2ca8e0eb67b5e7ad2b2d5a83ea93d7dca2c8c9cb5183b96f | 1287 |
| `holdout.noindex/holdout-main/trials.jsonl` | 9d4a3ef4e1aac6ea848e8155ec30f6fbcaec72fbcd0afbac056a3abf537a68bd | 6663066 |
| `holdout.noindex/holdout-main/run.json` | df88656ce1d45780daaeda49cbd58d326f97f66663e5ad2e670b82dd63e1bd71 | 1269 |
| `config/compat-spec-c1.json` | c33b905cfa2f36d77a6f28d10a2b11db1b5759f8ae721b9eb2fb534dc51a548e | 10224 |
| `config/holdout-base.json` | 72ff5c191755823c0feebd8cf74697144e39657561e056a93057c1e91a2fdfa2 | 22699 |
| `config/holdout-config.json` | c0988d9b722690e4f9567c5d240dc3526da820d7d9783f1dd75080af3bb96f5d | 23625 |
| `config/screening-base.json` | e325d64f361d9c10eeb92fa2ef151bc7d4a5fd816392868c6eab2a411815fb57 | 44584 |
| `config/screening-config-2.json` | c607733ad5e7035a922cdee567205fc4c5fb50e5d4bb34d4fd120307aa46f62a | 48619 |
| `config/screening-config-3.json` | 5e815ecc1b5edb6ab2a7d84d04f4adbdfc55edca18a281f3ebc5ee94d966998f | 46446 |
| `config/screening-config.json` | 4b987a35f794f49cee1e28862da0d879632659628b98f9ac1f2c99e7f1db7696 | 48228 |
| `../corpus-holdout/MANIFEST.json` | ab8a25e56e09685b3d7198275b4e4ffb9da981f50ecc3737b606cba338dcde7d | 8239 |
| `report-holdout/holdout-main/report.json` | a3ea06b5d8bbca87f7cbb7035bec96ef4f2e898d7efbb32d36092343cdd411ed | 810477 |
| `report-screening/br-decmap-native/report.json` | c02c24fc7da0aac20424f06dd79f7aca1d8e860bfe880e39f1f362def7a24588 | 73267 |
| `report-screening/br-decmap/report.json` | fecaff4f74d4fcb74edd8f4be2ef672997e37f15b657d35083729862cb39fb66 | 31232 |
| `report-screening/br-entropy/report.json` | d8b06a9d42dc738a09dcccb017e93101e51e3581743ebd4cfff52092d6966213 | 62421 |
| `report-screening/br-native/report.json` | bbc11c741cb0d98a3a33a1ebcd44c80efa1cb1d3c3ef271d1a0e1222b6706445 | 132017 |
| `report-screening/br-ratio/report.json` | cd05583df6054399f9e28e3288930a5a9587fc69e69084b9510ba663d92505ea | 44579 |
| `report-screening/presets/report.json` | bddedd2e46a2b0493bf6b64a96985ca7ab85fd26637cf669dc7de5f6378adda6 | 773217 |
| `report-screening/sep-docs/report.json` | 2ed9acf369e8fbf93591f24a63d52270145aee074684467cbc6ad670a65d1532 | 31728 |
| `report-screening/ultra/report.json` | d9dc14450e7d02b3409c5dcf567c2b72accf0ed2f023fb8a55ad36e31f0de317 | 99568 |
| `compat-matrix-c1/rows.jsonl` | eb99ee19b4ffedb201e8dc233e7fed32ab4b9a033d8a6881d92351b1a147cc8f | 3674738 |
| `native-matrix-c1-release/rows.jsonl` | cba68fd78f2ac7e037e75446d5e33914041f00b021682745979698aae2f8436c | 1316423 |
| `../final-builds/BINARIES.sha256` | 723b8bce6e5b1078905f6286c3f662874bdb831dcc9bf66da55f07d8e7d6f3ec | 155 |

Earlier screening attempts (screening, -2, -3) and their logs are retained unchanged; they were superseded because of harness fixes (commits 3d62f6de, ac6e5c9f, 2bdbbb72), not deleted.
