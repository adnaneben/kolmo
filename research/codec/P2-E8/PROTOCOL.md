# P2-E8 — archive frontier on N1's complete-CLI boundary (development)

Registered before any P2-E8 measurement. Question: under P2's archive cost model (CPU at $(3.89/72)/h with 3.3
reads, $0.023/GB-month for 30 days; storage dominates for these sizes), which complete pipeline is smallest within the
designated archive rival's measured cost? N1/P2-E7 measured only Kolmo `hc` among production archive settings.

Same boundary, pinned rivals (OpenZL v0.3.0 `32246b48…`, zstd 1.5.7 CLI), Kolmo control build, flags, prepared N1
inputs and unchanged N1 harness as P2-E3/E7 (`run_e8.py` replaces only `policies()`/`command()`). Added policies:
production `kolmo -m eco`, `-m max`, `-m ultra -l 1` (one thread), informative `zstd --single-thread -19`, and the
typed-CSV archive research policy `mcsv_prod19`. N1's own policies are re-measured.

Rival: OpenZL `csv` profile level 9 for the CSV views. NPY inputs: the designated typed rival is BLOCKED (no `numpy`
profile in v0.3.0); `openzl_serial_9` is reported for them as an informative generic control only.

Gate: ASan/UBSan/LSan round trip of `mcsv_prod19` on every input before timing (production Kolmo modes are the
repaired source, covered by its own correctness gate). Two seeded runs + N1 `confirm`. Per-process limits are N1's
(90 s wall, 6 GiB address space); any limit hit is a recorded failure, not a measurement. Observed development data.
