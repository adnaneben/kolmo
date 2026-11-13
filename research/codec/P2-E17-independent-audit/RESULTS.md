# Independent P2-E17 development evidence audit (13 November 2026)

Executed the standalone offline auditor on **actual** GitHub Actions artifact **14001980117**, run **43341163635**, produced by exact source commit **f4fcadcd762d7d7bc9e484a4730900eb3388d5e9**.

- Artifact SHA-256: `f2e4c589b9e77c3c1cce609f3df120d6a6fc0be6033cc18f2a3cf8ec00669e1a`.
- Verified **7,842 MANIFEST.json-listed members**, plus **61 prepared input hashes**, each checked against both runs' records. The compressed archive has 7,904 members (including MANIFEST).
- Recomputed 2,928 paired costs **across 976 configurations** (61 inputs × 8 policies × 2 independent passes × 3 pairs = **2,928 cost pairs**), including 3% CPU-over-wall accounting, 3.3 read events, 30 days of storage and 1 Gbit/s speed-mode sensitivity.
- Cross-run source SHA256, byte-exact status, complete frame size, frame SHA256, 8-policy cohort, per-pair IDs and OpenZL rival identities match. Rechecked `confirmation.json`: `IDENTITIES_MATCH`, 488 / 488 configurations.
- Per-pass, a **hindsight/uncharged per-file oracle** finds a smaller within-archive-cost Kolmo frame on **47/61** development inputs. Requiring per-pair feasibility in BOTH runs also yields **47/61**. Production-only hindsight frontier: **37/61** robust.
- A single preselected **tp_pent** configuration, *without a measured online selection rule*, fits the archive cost and is smaller than OpenZL on **41/61** in both runs. Single policies: `tb_best` 35, production `kolmo_fast` 25, production `kolmo_max` 19, `kolmo_hc` 11; these counts overlap and must **not** be summed. The problem GHCN family has only **2/9** robust oracle opportunities under the current rival.
- The source code and test protocol do not treat a hindsight oracle as deployable: online trial/search/selector costs are not charged, and cohort data are previously exposed. P2-E17's OpenZL integer profiles compress whole original files **including NPY/WAV headers as numeric values**, not a strong header-exact numeric payload profile. A native fair comparator and source-disjoint validation remain prerequisites. This is **not** P2 acceptance or a release decision.

## Execution / source receipts

Run: `python3 -m unittest discover -s /mnt/data/kolmo_p2e17_audit -p 'test_*.py' -v` → **5/5 tests PASS** (synthetic infrastructure fixtures).
Run: `python3 /mnt/data/kolmo_p2e17_audit/audit.py /mnt/data/kolmo-P2-E17-43341163635.zip --output /mnt/data/kolmo_p2e17_audit/RESULTS.json` → **PASS** (real archived runner evidence).

Local SHA256: audit.py `486d8eb052226b03a2dc9a3e012bcd9c31e39acc7997b5da9dabdc6d0fd24369`, test_audit.py `75bbf7c17c7b94b0f983891a2042b749f21cdd86bb6927758c84f97080aa15b6`, RESULTS.json `193e30a9ab7b2e065e55dde567644f6eb9314480d32a54bb207e4e495ffd537b`.

Scope: **audit and tests only**, zero new experiment runner minutes. No source-disjoint datasets consumed, no trained model, production codec or format change. Results remain development evidence. Next: fair native header-exact OpenZL comparator and a frozen fresh-source protocol, not another uncharged model-search oracle.
