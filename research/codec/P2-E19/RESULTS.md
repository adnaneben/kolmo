# P2-E19: GHCN BWT frame on N1's harness — LOCAL only (runner job not dispatched)

N1's unchanged `baselines.py` (via `run_e19.py`) was run in the local container with locally built pinned OpenZL
v0.3.0 and zstd (N1 flags), on the nine observed GHCN inputs. Their hashes equal P2-E14's. It completed 54/54
configurations with byte-exact verification. ASan/UBSan/LSan `gf` gave 18/18 frames decoded by the production
`kolmo -d`.

Within OpenZL `le-i32`-9's archive cost on every pair (`*` in `results/local_tables.md` and the records):
- `gf_bsc_p0` (KG_INT order 0 + `C_BSC`, production frame) **fits on 0/9**, despite being smaller than OpenZL on
  5/9 (for example 94728 TMIN 35,162 vs 37,357). BWT decode time outweighs the bytes saved at these 120–230 KB
  sizes.
- `tp_pent` fits on 9/9 locally but is larger than OpenZL on 6/9.
- Local timings are not runner timings: P2-E17 on the runner gave `tp_pent` 2/9 and OpenZL 7/9.

Decision: the planned runner job (`p2e19-cli.yml`) was **not dispatched**. The local N1-harness result already
shows the existing-leaf BWT route does not fit, so a runner measurement would not change the conclusion of P2-E18.
GHCN within cost needs the fast order-1 leaf proposal (P2-19), which needs wire review first.

This is development data only, with no acceptance.
