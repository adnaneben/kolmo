# C1: independently audited CSV competitive claims

This directory contains a retrospective evidence auditor, not a new codec or a new benchmark. Only the authorized research branch is writable. See PROTOCOL.md and RESULTS.md.

## Reproduce
Python 3.10+ standard library only. Download the exact workflow artifacts:
- E26: run 43403830702, artifact 14027086734, ZIP SHA-256 606ab9dbf841f86bc9c69b1c185e898c168bf929bb351db4ffb4d5938cf1af08.
- E27: run 43407137403, artifact 14028561769, ZIP SHA-256 6ca1ee6612ee1f5b92a79cf189af2ce3c55c1fedc3d2f1cc3e67b29b0a81a3ef.

```sh
python3 -S -m unittest discover -s research/codec/C1-csv-claims-20261113 -p 'test_*.py' -v
python3 -S research/codec/C1-csv-claims-20261113/audit_csv.py \
  --e26 /absolute/path/E26.zip --e27 /absolute/path/E27.zip \
  --output /absolute/path/new-audit.json
```

The output must not already exist. The auditor never extracts ZIP entries into the filesystem. It verifies every member and original input, then recomputes costs and grouped outcomes. Existing experiments and results are not modified.

SUMMARY.json and TEST_RECEIPT.json are compact Git records. The attached C1 evidence package contains the full generated audit, per-input CSV ledger, test log, source and environment-warning log. The two original workflow ZIPs remain separate, hash-pinned artifacts and are not claimed to be permanently stored in Git. No new codec timing or decoding is performed by this program.
