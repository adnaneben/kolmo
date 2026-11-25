# Sampled CSV grouping versus encoding with no grouping

At preset 3 on `c8fc5ac54aa302baf6abc66f210617901f9e6e2e`, grouping the complete Our World in Data CO₂ table by country saved **159,605 bytes**. In the complete NOAA Miami station table, all nine output blocks named a group column, but none used it in a predictor. Suppressing grouping saved **13 bytes** there. Both results held at one and four requested threads. The source-file control emitted an identical frame with either policy.

| Complete input | Input bytes | Chosen grouping | No group | Chosen minus no group |
| --- | ---: | ---: | ---: | ---: |
| OWID CO₂ | 14,377,942 | 1,337,516 | 1,497,121 | −159,605 |
| NOAA GHCN Miami station | 10,134,321 | 567,288 | 567,275 | +13 |
| Kolmo pool.c control | 6,818 | 2,715 | 2,715 | 0 |

The comparator forces only the selected grouping column to −1 at typed-table emission. It preserves planning, formula discovery, predictor evaluation, entropy coding and framing. Predictors depending on group boundaries naturally produce different results. Each chosen-policy frame also matched the unmodified reference executable. Decoding those actual frames exposed each block's stored group and number of columns using it. OWID's nine blocks specified column 0 (`country`) and each used 19–65 group-dependent numeric predictors. NOAA's nine blocks specified column 7 (`PRCP_ATTRIBUTES`) and used zero. Both no-group variants kept the typed-table transform in all nine blocks. The code-file control had one ordinary block. [On-wire evidence](wire.json) · [Source and executable hashes](manifest.json)

## Encode and decode times

Numbers are independent phase medians in milliseconds across three scored CLI runs after a warmup. The same unmodified Homebrew v0.0.2 reader decoded both output variants. Arm order and cell order were randomized. The timings include process startup and ordinary file I/O on a shared Apple M5. This short experiment supports the deterministic file-size findings. Phase-time differences on these six settings need more repetitions for a speed claim.

| Input | Threads | Chosen encode / decode ms | No-group encode / decode ms |
| --- | ---: | ---: | ---: |
| OWID CO₂ | 1 | 79.67 / 24.24 | 81.55 / 26.40 |
| OWID CO₂ | 4 | 40.49 / 12.56 | 36.77 / 15.50 |
| NOAA GHCN | 1 | 53.15 / 22.24 | 49.79 / 20.18 |
| NOAA GHCN | 4 | 23.71 / 11.70 | 27.32 / 10.63 |
| Source control | 1 | 4.46 / 4.25 | 3.79 / 3.99 |
| Source control | 4 | 3.18 / 4.27 | 4.50 / 3.33 |

## Sample-to-file regret

A separate check compressed approximately 256 KiB of body rows from the beginning, middle and end of each table. Each sample retained the exact original header and complete rows. This three-span comparison is an experimental size-selection rule. The shipping planner samples neighboring rows for group discovery and uses its own predictor costs.

On OWID all three sampled frames favored grouping, by **11,538 bytes** in aggregate. On NOAA one sample tied and two favored grouping, by **37 bytes** in aggregate, even though no grouping won the complete file by **13 bytes**. Against the better of these two policies, the three-span decision therefore had whole-file size regret of 0 B on OWID and 13 B on NOAA. This pair of files gives no basis to charge an extra complete encode on every CSV. The emitted-but-unused group metadata on NOAA is a specific case for a future guarded optimization. No production source changed. [Sample hashes, offsets and exact scores](samples.csv)

The 48 timed whole-file readbacks and all 12 sampled readbacks reproduced the source bytes or SHA-256 with the released reader. Twelve additional diagnostic decodes checked the complete original SHA while extracting on-wire decisions. [Per-setting throughput](score.csv) · [Raw observations](raw.csv)

For replay, build the pinned default tree. Run `python3 instrument.py CHECKOUT ISOLATED_BUILD_DIRECTORY`, then run `replay.py --source CHECKOUT --bin ISOLATED_BUILD_DIRECTORY/kolmo --reader STOCK_KOLMO --owid CO2.csv --noaa MIAMI.csv --out FRESH_DIRECTORY`. Run `samples.py --bin ISOLATED_BUILD_DIRECTORY/kolmo --reader STOCK_KOLMO --out FRESH_DIRECTORY` with the same output path. Place `score.py` beside the raw output to regenerate the summary.
