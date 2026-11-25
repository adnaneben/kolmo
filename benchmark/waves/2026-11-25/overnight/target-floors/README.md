# Correct target reports when no setting meets a speed floor

Local commit `8d23095b2f09d5c9e3c3425123bea63641ca5d71` fixes the size and cost reported for a chunk whose calibrated settings all miss the requested floor. It is a separate follow-up based on `c8fc5ac54aa302baf6abc66f210617901f9e6e2e`. The encoder still selects the fastest predicted decoder in that case, including when the requested floor concerns encoding speed.

For the original 7,584-byte GnuTLS handshake PNG, `--target 'encode>=999GB/s'` previously selected `speed`, emitted a valid 7,084-byte frame, and reported a predicted size of zero. It now reports 7,084 bytes and appends the number of chunks on which every modeled setting missed the floor. The existing verbose summary remains in its original order. [Original and fixed CLI output](example-cli.txt) · [Source commit as a patch](target-floor-report.patch)

Fallback chunks have no sample trial, so the report takes their individual emitted frame sizes. A formula target evaluates the same cost expression with that byte count, original bytes, and calibrated encode/decode times. Reports for mixed inputs sum the individual modeled or measured chunk contributions. The final merged frame can be smaller because repeated chunk headers disappear. The API documentation now describes this boundary. The optional internal CLI counter uses a private symbol. The public function and report structure retain their ABI.

## Complete-file verification

The existing B26 holdout has three independent producers. Each was tested against eight encode/decode floor values at T1. Every one of the 24 before/after pairs chose the same setting, emitted the same complete frame SHA-256 as that directly forced B26 setting, and decoded exactly through the stock Homebrew reader. The 18 nonfallback size estimates remained identical. Both impossible floors generated the following corrected results.

| Complete input | Input bytes | Selected fallback | Previously reported bytes | Corrected and actual bytes |
| --- | ---: | --- | ---: | ---: |
| GMP manual | 300,383 | speed | 0 | 121,012 |
| GnuPG gpgv binary | 557,232 | speed | 0 | 335,370 |
| GnuTLS handshake PNG | 7,584 | speed | 0 | 7,084 |

An unchanged C consumer compiled against the original public header was linked to each library. Twelve additional round trips included the size objective and `cost:size*2+enc*3+dec*5` with unattainable floors. All six original calls reported zero size and cost. All six fixed calls reported the emitted bytes and correct objective value. For the PNG the formula produced 14,168.000397625072. Calibrated times were identical between builds. On this arm64 host the public report occupied 104 bytes and the predicted-size field remained at offset 96. [C results](public-cost.csv) · [Source and executable hashes](manifest.json) · [All floor comparisons](holdout.csv)

## Predicted and measured floor feasibility

Among the 18 ordinary-floor cases, 14 had a measured passing direct route in B26. The chosen route passed 11. On the GMP manual the selector chose ratio for the 30 MB/s decode floor, hc for the 80 MB/s decode floor and balanced-n for the 60 MB/s encode floor. The corresponding direct rates were 21.21, 63.41 and 54.19 MB/s. At least one other route met each requested floor in this dataset. On the small GnuTLS PNG, no direct route achieved four of the six ordinary floors, even though the model admitted a route. No eligible direct setting on any source approached 999 GB/s.

These feasibility results compare the selector's one-thread built-in model to historical warm-file, fresh-process rates from Apple M5. Process launch and I/O contribute to B26's direct measurements. The same mismatches occurred before and after this reporting correction. No new calibration or change to route decisions is inferred from these three sources.

## Tests and replay

The optimized and address/undefined-sanitized target suites passed. The new regression checks one unmatched binary chunk and a two-chunk input with one matched text chunk and one unmatched binary chunk under both encode and decode floors. It validates the fallback count, chosen settings, measured size, formula cost, merged size and exact decode. The existing public-report test additionally checks an unsampled fallback larger than the whole-chunk trial cutoff. All 357 frozen v12 decode frames passed. The Python and Rust source mirrors were synchronized. [Optimized tests](test-native.log) · [Sanitizers](test-sanitized.log) · [Frozen frames](test-v12.log)

Use Python 3 to run `replay.py --before OLD_CLI --after FIXED_CLI --reader STOCK_CLI --b26 B26_DIRECTORY --out FRESH_DIRECTORY` and `public-replay.py --baseline-tree OLD_TREE --fixed-tree FIXED_TREE --b26 B26_DIRECTORY --out FRESH_DIRECTORY`. Both runners use the exact inputs in the original B26 freeze. Run `make check-profile` and `build/test-v12-corpus` in the candidate checkout for the code-level suites.
