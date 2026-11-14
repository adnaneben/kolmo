# S2 conditional plane layers

Research-only continuation of S1; no production files or defaults changed. Read PROTOCOL.md, RESULTS.md and EVIDENCE.json. Write only to the authorized isolated branch and keep PR #7 draft/unmerged.

Prerequisites: full Kolmo checkout with reference 4c78ea9893a7882bc4d427cca29076623312d66a, committed S1-stack-order source, GCC/Clang, Python standard library and the normal vendor build requirements. Commands use new disposable output directories.

```sh
make -j2 build/libvendor.a CFLAGS='-O3 -DNDEBUG -fno-plt -ffp-contract=off'
python research/codec/S2-conditional-planes/build.py --repo . --output /tmp/s2-release
python research/codec/S2-conditional-planes/build.py --repo . --output /tmp/s2-asan --sanitize
/tmp/s2-release/s2 selftest
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/s2-asan/s2 selftest
python -m unittest discover -s research/codec/S2-conditional-planes -p 'test_*.py' -v
```

Restore the original S1 evidence archive into a directory named `s1` within a new experiment root. Verify its SHA-256 against the protocol. It supplies INPUTS.json, exact inputs and reference frames; neither old timings nor old test passes are reused.

```sh
python research/codec/S2-conditional-planes/run.py --binary /tmp/s2-release/s2 --s1 /tmp/s2-run/s1 --output /tmp/s2-run/golden --rounds 0 --seed 20261113
python research/codec/S2-conditional-planes/run.py --binary /tmp/s2-asan/s2 --s1 /tmp/s2-run/s1 --output /tmp/s2-run/sanitized --rounds 0 --seed 20261113 --reference /tmp/s2-run/golden
python research/codec/S2-conditional-planes/run.py --binary /tmp/s2-release/s2 --s1 /tmp/s2-run/s1 --output /tmp/s2-run/first --rounds 7 --seed 202611131 --reference /tmp/s2-run/golden
python research/codec/S2-conditional-planes/run.py --binary /tmp/s2-release/s2 --s1 /tmp/s2-run/s1 --output /tmp/s2-run/confirmation --rounds 5 --seed 202611132 --reference /tmp/s2-run/first
```

For each input, save `/tmp/s2-release/s2 profile INPUT 3` to `profile/INPUT_NAME.jsonl` in the experiment root; profiling is separate from normal benchmark timing. Then run `audit.py /tmp/s2-run`. The existing independent S1 reference parser supports mixed per-group stages without modification. No decoder-only optimization is part of S2.

The shared library/source dependency identities are verified at build time. A reusable bridge object is optional only when its recorded dependency identities and sanitizer regime match. The default build compiles a fresh bridge.
