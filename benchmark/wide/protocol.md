# Wide electricity tables

Use the 300-column Open Power System Data hourly file from the previous milestone for development. Reserve Artur Trindade’s ElectricityLoadDiagrams20112014 (UCI 321), which has timestamped readings for 370 Portuguese electricity clients, for the final run. This is a different dataset from the previous UCI household file.

Keep every original byte of each scored row. Share the header between partitions, train on complete rows from the first quarter (at most 4 MiB), score the final three quarters. Train OpenZL 0.3.0 using its CSV profile, the published semicolon separator, four offline workers and ten seconds per training step. Exclude offline training from operational comparisons. Compare the frozen candidate with PR #2 fast, upstream Zstd 1.5.7 -3 and trained OpenZL. One CPU worker per CLI, randomized serial execution, new paths, one warm-up followed by seven rounds. Verify SHA-256 on each decode. Pin binaries and source hashes before scoring. No tuning on the new dataset after scoring.

The primary Zstd pass requires at least 20% fewer complete bytes with median encoding and decoding wall times and maximum measured encode and decode RSS each no greater than Zstd. Report the same five comparisons against OpenZL, and any regression against PR #2. If a wide-table experiment regresses, keep the previous default available. Controls are the earlier ten fast workloads.

The compatible 256-lane layout separates the first 255 fields and keeps the entire remaining row as its final lane. The baseline is the frozen PR #2 build. The development replay uses the exact prior OPSD encoder graph. Check a candidate frame with the published 1.0.0 decoder in each dataset’s warm-up. Run the ten control workloads using five timed pairs per input after a warm-up.
