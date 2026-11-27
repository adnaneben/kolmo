# N1 OpenZL v0.3.0 comparator instrumentation (independent preparation)

**Status: implemented and mock-tested, NOT a real OpenZL benchmark.** This track is intentionally independent of Claude's P2-E1/E2 native pipeline work. No long GitHub Actions run is launched by this commit.

## Source and comparator pin

Official release: https://github.com/meta/openzl/releases/tag/v0.3.0 (published 2026-09-29), tag target commit **32246b48faee46807f84183dac4db479089f5445** as returned by the GitHub tags API. OpenZL 0.3.0 includes encoder-side Compression Transformer selection for numeric profiles at level >= 7. The earlier repository comparator used OpenZL 0.3.0 levels 6/9 and applicable profiles: benchmark/openzl/run.py.

At execution, pin the actual zli executable's SHA-256 and --version output, profile, level, source SHA-256, input type, training regime, thread count and cost model. Version strings and release-tag references alone do not prove a binary was built from that commit: preserve the build command and verified checkout in a separate runner-grade receipt.

## This tool and boundary

openzl_cli_probe.py takes a frozen JSON manifest, rejects missing input/binary pins, runs 3–7 full compress/decompress pairs (each command separately capped at 120 s), computes wall and child CPU time, records complete frames and verifies original bytes exactly. It uses a private temporary directory; original files are not modified. Limits: <=50 MB per input; level 6/7/9; untrained, declared profiles only. All subprocess startup and file I/O are charged. It refuses non-deterministic frame sizes/hashes. Linux/POSIX Python only (resource.getrusage).

This is a **CLI/file-I/O/process-startup boundary**: it cannot be spliced into P2-E1's in-memory-only cost table. It explicitly sets comparison_eligible=false and measurement_scope != complete_pipeline. The probe does not prove peak-memory compliance, single-thread purity or compiler provenance; a matched runner must check those independently. Never call this a P2 acceptance result.

A sample input manifest is constructed *before* any trial (hashes from the actual selected input/build; do not invent them):

```json
{
  "schema": 1,
  "scope": "development",
  "openzl_git_sha": "32246b48faee46807f84183dac4db479089f5445",
  "training_regime": "untrained",
  "input_path": "/absolute/path/original.npy",
  "input_sha256": "<actual SHA-256>",
  "zli_binary_sha256": "<actual executable SHA-256>",
  "expected_version_fragment": "0.3.0",
  "profile": "numpy",
  "level": 7,
  "repetitions": 5,
  "timeout_seconds": 30,
  "max_input_bytes": 50000000
}
```

Run: `python3 openzl_cli_probe.py manifest.json --zli /absolute/path/to/zli --output record.json`.

**Comparator selection must be frozen independently of the results.** For a numeric-typed file, the numeric profile at level 7 (Transformer enabled) is an important strong contender. Also evaluate level 6 (unchanged baseline), level 9, and any preregistered applicable strong trained graph in a **separate, cost-accounted protocol**, not by silently choosing the weakest result afterward. For CSV/Parquet/other declared types, use supported documented profiles and appropriate graph. Serial fallback is an informative control, not a substitute for an applicable typed profile. Do not train on test/validation files. Current probe explicitly rejects trained setups until training time, artifacts and reuse horizon are modeled.

## Local verification

`python3 -m unittest discover -s research/codec/overnight-20261112/n1-openzl -p 'test_*.py' -v`

10 tests passed on local Python 3.13.5, with a **zlib-backed fake zli CLI** for subprocess/round-trip plumbing. Mock numbers are not OpenZL measurements. SHA-256 local source:
- openzl_cli_probe.py: e3d36cccd9d1e4f5503d8ab4b291619bcdfb3ba2293fd958623f70e4bd48ad88
- test_openzl_cli_probe.py: 6c8530b406f7e749868dff8881fa7b9890b582749680603e6aa02846c21648eb

## Next dependency

Wait for the existing queued PR/CI checks and Claude's P2-E2 development work. When capacity allows, build pinned OpenZL in a disposable isolated checkout and run this comparator **only alongside a separately instrumented matching Kolmo CLI** (or implement equivalent in-memory C OpenZL API controls). Both require byte-exact whole-file reconstruction, matched ordering and accounting (CPU over wall when >3%), memory caps and source-exposure audit. Only then feed complete comparable records to the active policy evaluator. No overnight runner minutes were consumed by this preparation.
