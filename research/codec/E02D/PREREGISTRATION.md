# E02D preregistration — frozen specialization validation

Registered before downloading/parsing the selected measurement data or measuring its compression. Only the publishers' metadata was inspected to choose sources and fields. This is a new validation set relative to E02C, not a final holdout, and cannot later be called untouched after these results are inspected.

## Frozen reference and isolation

Only `fix/planner-record-ownership-isolated-20261112` may receive commits/pushes. PR #7 remains draft/unmerged. No changes to other branches, PR #6, production source, vendored code, existing formats, defaults or release acceptance criteria.

Research executable/representation reference: `706cdab65caace2ce698e5dfe4c5dd99e8fcbe00`. Documentation head before this registration: `57c337dde7a06892c2cf1c31ff4b8f01553c41d5`. Production reference: `2f3308a3380a140ad0f6873ae228de7715e5eef5`. Freeze every existing `research/codec/E02C/` file and all `src/`, `vendor/`, Makefile, and `benchmark/accept/TARGETS.md` bytes. E02D may add orchestration, source preparation, evidence and reporting, but may not tune the frozen codec. A genuine codec defect invalidates the affected experiment; document it rather than modifying settings against this validation set.

## Sources selected from publisher metadata, not compression outcomes

All four are UCI-hosted source families absent from E02C's digits, Gapminder, Nile and sunspots set. Correlated channels/prefixes from a family do not count as independent sources.

| Family | Publisher download | Exact selected fields | Rows used |
|---|---|---|---|
| Household power, UCI 235 | https://archive.ics.uci.edu/static/public/235/individual+household+electric+power+consumption.zip | household_power_consumption.txt: Sub_metering_1, Sub_metering_2, Sub_metering_3 | first 524288 source data rows, before missing-value filtering |
| Metro traffic, UCI 492 | https://archive.ics.uci.edu/static/public/492/metro+interstate+traffic+volume.zip | Metro_Interstate_Traffic_Volume.csv.gz: traffic_volume, clouds_all | all rows |
| Bike sharing, UCI 275 | https://archive.ics.uci.edu/static/public/275/bike+sharing+dataset.zip | hour.csv: casual, registered, cnt, instant | all rows |
| Appliances energy, UCI 374 | https://archive.ics.uci.edu/static/public/374/appliances+energy+prediction.zip | energydata_complete.csv: Appliances, lights | all rows |

Attribution: Hebrail & Berard, Individual Household Electric Power Consumption, DOI 10.24432/C58K54; Hogue, Metro Interstate Traffic Volume, DOI 10.24432/C5X60B; Fanaee-T, Bike Sharing, UCI 275; Candanedo, Appliances Energy Prediction, DOI 10.24432/C5VC8G. Retain the publishers' CC BY 4.0 attribution with any redistributed derived inputs. No confidential/user deployment data.

Preserve publisher row order, including repeated timestamps. Parse numeric text via Decimal, require finite exact nonnegative integral values within uint32, and write little-endian uint32 NPY files. No rounding, scaling, imputation, sorting, artificial outliers, duplication, resampling or distribution-driven window selection. Skip only blank/`?` tokens independently in each selected field, recording original row indices, counts, and hashes. Do not replace missing data with extreme sentinels. These are losslessly compressed DERIVED VALID-VALUE NUMERIC FILES: reconstruct their NPY bytes, not original CSV spelling or missing positions. Source-row maps are provenance outside the compression boundary; no full-source-CSV compression claim is permitted.

For each of the eleven streams, create the full selected valid-value array plus exactly its first 128 and first 1024 values as nested small-message cases. Fail rather than silently reduce these sizes when insufficient values exist. Expected matrix: 33 derived files x 5 representations x 4 backend policies = 660 configurations per pass. Do not substitute a different source on download/parser failure; report incomplete coverage. Record actual source/archive/member/derived hashes before invoking a codec. A confirmation pass must reuse those exact bytes; a changed upstream download is not silently accepted.

## Frozen experiment and controls

Use E02C's unchanged probe and independent reference. Methods: original residual layout, global bitpack, local bitpack, patched, local byte planes. Policies: stored, existing Huffman, existing rANS, zstd-1, with the same recorded stored fallback. Local segmentation stays 128 values. Keep production predictor/GCD choices unchanged; record parameter, residual and rest hashes across every configuration.

Main pass: five native C timed repetitions; confirmation: three. One thread. Deterministically shuffle configuration order with seed 20261112 in pass one, 20261113 in confirmation. No selecting the fastest run. Retain raw times, medians, requested/actual codecs, checksum-inclusive full research-frame size/hash, post-exec native VmHWM and original diagnostic ru_maxrss. Timing/RSS are descriptive on a shared runner, not M5 or pure-codec memory claims. Keep memory/elapsed boundaries explicit.

Also run unchanged complete Kolmo balanced and hc, and direct single-thread zstd-1, on every derived file as complete-tool SIZE controls. Reconstruct their input exactly. CLI speed is not compared to native in-process timing. Never count a fixed-policy or stored-vs-packed gain as a whole-product win.

Non-target controls are preselected: Metro clouds_all and Bike instant. They remain included even if a candidate loses. All other streams also remain in the full matrix regardless of whether the proposed residual phenotype is actually present.

## Hypotheses and phenotype tests (before seeing outcomes)

Analyze the UNCHANGED production residuals, not manually selected original values. Quantiles use sorted integer bit lengths with rank floor(0.99*(n-1)).

H1 sparse large outliers: at least 4096 residuals; 99th-percentile bit width at most 8; maximum width at least 4 bits above that percentile; fraction exceeding percentile width >0 and <=1%. Candidate: patched. Primary comparator: matched local-byte-plane representation under the same backend, with original-layout comparison also mandatory.

H2 changing local widths: at least 32 complete 128-value residual blocks; 90th minus 10th percentile of block-maximum bit widths at least 4. Candidate: local bitpack. Compare both matched local byte planes and global bitpacking at the same policy, plus original layout. Rank quantiles use floor(p*(blocks-1)).

H3 small integer streams: the fixed 128- and 1024-value prefixes. Candidates: stored global/local/patched packing compared separately with original-layout Huffman/rANS. Include complete framing and minimum absolute savings; disclose stored fallback. Related prefixes are not independent trials.

H4 bounded non-byte-aligned residuals: full streams with max residual width from 1 to 16 inclusive and not divisible by 8. Candidate: global bitpack, compared with original layout at each policy.

If a phenotype is absent, mark its hypothesis NOT EXERCISED, not disproved or confirmed. Do not seek replacement datasets or loosen the phenotype after outcomes are known. Descriptive successes outside these hypotheses must be labelled exploratory.

## Predeclared decisions, not release gates

For a size-specialization validation signal require at least 5% AND 64 bytes complete-frame savings on phenotype-matched data in at least TWO distinct source families for the same candidate/policy comparison. Full-stream H1/H2/H4 support requires at least 16384 input values per supporting stream. H3 may use the prescribed small messages, aggregated within family before counting independent support. One-family evidence is PROVISIONAL ONLY. Absence of phenotype or coverage is INCONCLUSIVE, not a reason to promote.

Report all family totals, pooled totals, equally weighted family mean percentage changes, and worst per-file regression. Keep full streams and nested small-message results separate; do not pool their duplicated bytes into one headline. Offline best-size selection remains diagnostic and its search cost is not zero.

Any fast-path proposal additionally needs nonregressing repeated decode time (median ratio <=1.10 in BOTH passes), encode ratio <=2.0 and measured process-memory ratio <=1.25 against the matched control, alongside the size signal. Shared-runner results alone still cannot qualify production/M5 acceptance. Failure of these provisional research screens means no fast/default integration. A size-only candidate may remain research-only with its cost explicitly stated.

Existing release TARGETS.md is unchanged and authoritative. No production integration/default/selector/SIMD work occurs in this experiment, even on a positive signal. Next work is chosen from the frozen results and fresh validation/holdout is reserved for subsequent algorithm changes.

## Execution integrity

Run the frozen sanitized self-tests and perform exact C plus independent packing/integer inverse reconstruction of every configuration in both passes. Preserve finite-test limitations: the independent reference does not implement entropy decoding. Compare all 660 frame hashes/sizes and capture identities between passes. Export compact evidence with raw records, provenance and verification hashes; avoid duplicating large scratch frames into the repository. Retain failures. This protocol may receive a transparently versioned infrastructure correction, never an outcome-driven representation/parameter change.
