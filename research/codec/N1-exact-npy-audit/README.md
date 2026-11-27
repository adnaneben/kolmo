# Independent N1 exact-NPY contract regression (development)

This new test-only directory adds coverage to the existing experimental `N1-exact-npy/npy_openzl.py` without changing the adapter, production codec, wire format, any older research result, or the parallel P2-E10 work.

Run: `python -m unittest discover -s research/codec/N1-exact-npy-audit -p 'test_*.py' -v`.

Five deterministic unittest methods cover 144 endian/sign/width/version/lexeme/level byte-exact round trips, 64 scalar/zero/multidimensional cases, 13 frame-field mutations plus truncation/trailing cases, seven oversized/unsupported metadata cases, and payload length rejection. The OpenZL action is deliberately replaced by an in-process **zlib MOCK**; the tests cover wrapper mapping/framing/inverse only, **not OpenZL execution, performance, resource limits or competitor acceptance**. The original adapter imported by these tests is the unchanged Git blob `4aa7577a7be03c47a0e04caa22796a4d813c80f8`.

The run retains the existing live P2-E10 job; it must be allowed to finish before any new benchmark workflow. A real typed-NPY OpenZL runner plus full-cost audit and source-disjoint confirmation remain outstanding.
