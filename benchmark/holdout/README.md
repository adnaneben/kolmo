# Structured data comparison

[Results](RESULTS.md) · [Decision chart](decision.png) · [Frozen rules](protocol.md)

The scored binary came from research checkout f3e6f84. The shipping source in integration commit 90f3b2a matched the frozen shipping source. A separate build from that integration commit produced exactly the same compressed frames on all three held-out files. [Integration check](integration-check.json)

Run `python3 benchmark/holdout/score.py` with Matplotlib installed to recompute every pass/fail cell and the chart from the committed individual trials. The protocol, input metadata and receipts include the settings, timing units, corpus splits, archive hashes, per-operation resident peaks and decoder hashes. The registry receipt identifies the exact Homebrew, PyPI and Cargo releases. Raw datasets and machine executables are acquired separately.

For a fresh machine run, use a disposable checkout. Place the three sources listed in `inputs.json` in `build/holdout/downloads/` as power.zip, rides.zip and quakes.csv. To verify release provenance, supply in that directory the GitHub release tarball (brew.tar.gz), Homebrew formula (formula.rb), PyPI 1.0.0 JSON and sdist (pypi.json, kolmoc.tar.gz), and Cargo index and crate (crates-index, kolmo.crate). Run `prepare.py` from this directory or the repository root. Build the single-thread-capable programs named in `freeze.json` and place them in `build/holdout/frozen/` under their recorded names. The hash guard deliberately checks the exact scored binaries; record your own executable fingerprints in the disposable checkout if rebuilding.

The runner resumes its local measurement log. Move the archived holdout-runs.json aside in the disposable checkout before running `python3 benchmark/holdout/measure.py`. It trains OpenZL separately for each file, saves the new encoder graphs in profiles/, and times every fresh encode and decode on macOS. The final three quarters of the source records are scored. Compare the input hashes before comparing results as upstream datasets may receive revisions.
