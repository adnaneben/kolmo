# N1 exact typed OpenZL comparator for complete NPY files

Research-only experimental wrapper, not a production Kolmo codec or acceptance.

Official OpenZL v0.3.0 (commit 32246b48faee46807f84183dac4db479089f5445) provides le-u32, le-i16 etc. numeric payload profiles but not a numpy profile. Prior N1/P2-E7 attempts using OpenZL -p numpy were invalid configurations, not measured typed competitor results. OpenZL serial is only an informational generic control.

The experimental npy_openzl.py parses bounded NPY v1/v2/v3 headers, validates exact shape/length, and handles explicit little/big endian int16/32/64 C-order arrays. It stores the complete original NPY header, frame lengths, complete-original-byte SHA256, and the genuine OpenZL typed-payload frame. Decompression reconstructs the exact original NPY bytes or rejects wrong lengths/corruption/unsupported metadata. All header/metadata bytes count toward size. No production source or old wire IDs change.

run_npy_adapter.py reuses N1-baselines/baselines.py unmodified, replacing unsupported OpenZL numpy policies for NPY only with openzl_exact_npy_6/7/9. It retains Kolmo, zstd-3, OpenZL serial and valid CSV controls. It measures complete subprocess CLI including Python typing, I/O, planning, temporary files, model setup, framing and decode. This wrapper has overhead compared to a native OpenZL API and must be reported transparently; it must not be interpreted as a weak comparator or used for general claims. A strong OpenZL numeric level/graph frontier remains required.

The source was prepared using an equivalent local prototype: 22 infrastructure/mock tests passed, including full subprocess reconstruction, endian/version/shape checks and corruption tests. These local tests used mock zlib in place of OpenZL, NOT compression-performance evidence. The exact committed source is tested in the new read-only workflow; status must be checked rather than assumed.

An independent integrity check of P2-E7 ZIP artifact 13986083533 (SHA-256 ae7ad41f0403265fa30a7cc73bcc0a5cf033e69a506f4026aa8afba40f7dd8dc) verified 1,880 top-level manifest entries plus 10 source input files via their separate prepared/manifest.json hashes. All eight observed development NPY files parsed as le-u32 with original 128-byte headers and round-tripped under the MOCK backend. They are not fresh validation inputs.

Run exact repository tests: python -m unittest discover -s research/codec/N1-exact-npy -p 'test_*.py' -v

The dedicated N1 exact-NPY GitHub Actions experiment uses pinned OpenZL, zstd and Kolmo on the N1 identical original-byte complete-CLI boundary, two seeded passes, three paired encode/decode repeats, exact reconstruction, hashes and retained failure logs. No RL or learned Kolmo selector was built. New typed-profile competitor evidence does not itself prove an implemented type-first Kolmo archive planner or fresh-policy acceptance.

Next: inspect that workflow and audit the matched archive costs, then compare existing layered, sparse and cross-column candidate portfolios, source disjointness, and any decoder CPU or metadata regressions. Do not pool in-memory leaf timings with complete CLI controls.
