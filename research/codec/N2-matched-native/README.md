# N2 matched native benchmark

This is a research-only complete-input in-memory benchmark, not a new production codec or an online planner. Read PROTOCOL.md before execution. All inputs are observed R3 development files. The fixed candidate and rival configurations are unchanged from their pinned source.

```sh
python3 -m unittest discover -s research/codec/N2-matched-native -p 'test_*.py' -v
python3 research/codec/N2-matched-native/build.py \
  --repo /absolute/path/to/kolmo \
  --openzl /absolute/path/to/verified-openzl-32246b48 \
  --output /new/absolute/path/n2-build
python3 research/codec/N2-matched-native/run.py \
  --binary /new/absolute/path/n2-build/cmake/n2_native \
  --artifact /absolute/path/Kolmo_R3_Fresh_Validation_43276455708.zip \
  --policy research/codec/competitor-policy-v2/policy.py \
  --output /new/absolute/path/n2-results
```

Use a disposable checkout whose source files match `4c78ea9893a7882bc4d427cca29076623312d66a`; a newer checkout is accepted only when each pinned dependency is byte-identical. OpenZL must be at its exact registered release commit with dependencies accessible. Normal production source/defaults/formats are not changed. Temporary includes omit unrelated CLI entry points without altering codec functions. Compiler/linker/build failures must be retained, not described as a benchmark pass.

`STATUS.json` distinguishes complete matrix execution from failure. `SUMMARY.json` retains each sample's original cost-model result rather than accepting an offline oracle. `MANIFEST.json` hashes all retained outputs, inputs and logs; exact executable/source receipts are separate. A source/policy receipt alone is not a completed run.

The tests intentionally use copy-codec fixtures for the bridge/driver. Their timings are never written to the real benchmark results and provide NO compression-performance evidence.
