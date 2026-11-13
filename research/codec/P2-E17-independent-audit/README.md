# P2-E17 independent archive evidence audit

Research-only self-contained Python 3 standard-library receipt validator. Does not implement a new compressor or selector.

Verify the exact GitHub Actions artifact [43341163635](https://github.com/adnaneben/kolmo/actions/runs/43341163635) from [artifact 14001980117](https://github.com/adnaneben/kolmo/actions/runs/43341163635): download it as a local ZIP file, then run:

```shell
python3 -m unittest discover -s research/codec/P2-E17-independent-audit -p 'test_*.py' -v
python3 research/codec/P2-E17-independent-audit/audit.py /path/to/p2e17-artifact.zip --output /tmp/p2e17-independent.json
```

The validator checks complete manifest hashes, independently retained source bytes, repeated frame/source identities, original-byte reconstruction receipt flags, all 2,928 paired time/cost calculations, exact competitor identity and the difference between an uncharged hindsight oracle and each fixed policy across both runs. It refuses invalid or incomplete evidence. A successful validation of supplied receipts is not independent decoding of the frames or fresh-source P2 acceptance. The ZIP is intentionally not committed; it is a GitHub Actions artifact with finite retention.

See RESULTS.md and the overnight iteration receipt for measured findings, limitations and the next substantive task.
