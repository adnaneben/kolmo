# N2 real-codec execution and independent audit

Read RESULTS.md and EVIDENCE.json first. The frozen matrix lives in ../N2-matched-native/. This directory adds real-codec sanitizer/stock-reader support and an independent timing audit; it is not a codec algorithm change.

The scripts use a staging root passed with `--root`, containing source/kolmo, source/openzl, build-release2 (the existing N2 build.py output), prepared (the eight verified inputs), logs, and empty/new build-checks and correctness output paths.

```
python build_checks.py --root /absolute/staging/root
python real_gate.py --root /absolute/staging/root
python ../N2-matched-native/run.py --binary /absolute/staging/root/build-release2/cmake/n2_native --artifact /absolute/Kolmo_R3_Fresh_Validation_43276455708.zip --policy ../competitor-policy-v2/policy.py --output /absolute/staging/root/measured
python audit_results.py /absolute/staging/root/measured /absolute/staging/root/audited
python -m unittest -v test_audit_results
```

A complete verified Git checkout is preferred. The separately attached source checkpoint provides one when network access is unavailable. OpenZL must be at the registered pin with dependencies available. Source recovery provenance and raw measurement files are in the separate evidence attachment. All real inputs remain observed development data.
