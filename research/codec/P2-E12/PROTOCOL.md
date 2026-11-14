# P2-E12 — type-first production-format frames, decoded by the production CLI (N1 boundary; development)

Registered before any P2-E12 measurement. Research encoder `research/codec/P2-E11/tpcli.c` writes standard `CZN1`
frames; **every research policy is decoded by the production `kolmo -d -T1` CLI** built from the repaired source.
Same pinned rivals (OpenZL v0.3.0, zstd 1.5.7 CLI), Kolmo control build and flags, prepared N1 inputs and unchanged N1
harness as P2-E3/E7/E8 (`run_e12.py` replaces only `policies()`/`command()`).

Research policies (frozen): `tp_phuf` (type-first, per-plane Huffman as plane-aligned chunks in one `C_HUF` leaf),
`tp_phuf_pg16` (16 KiB pre-guard, fallback = production zstd frame), `tp_prod3` (production leaf rule; payload
identical to `kg_encode_num`), `tp_table3` (production table graph forced for comma-separated input). Rival: zstd-3 CLI
(speed). Gate: ASan/UBSan/LSan-built `tpcli` encodes every input with every research policy and the production CLI
decodes it byte-exactly before timing. Two seeded runs + N1 `confirm`. Observed development data: no acceptance.
