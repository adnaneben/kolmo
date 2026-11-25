# Full enwik8 PAQ8PX process recovery

The initial PAQ8PX v217 -8L encode stopped at 10.83%. The original run directory retains the incomplete 1,892,352-byte archive and its progress log. A fresh one-shot process started just the PAQ comparison at 22:01 Riyadh on 25 November. Its input, compressor and the three completed archive digests were checked against the original receipts. The existing zstd, xz and Kolmo timings and compressed files were preserved. The PAQ run was observed advancing in a subsequent invocation. The partial encoding yields no final archive size, encode time, decode time or combined rate.

The Mac work directory is `work/ppp-overnight-20261125/full-enwik8-live` relative to the repository root. Its `paq-recovery.json` lists attempts. Within the active attempt, `attempt.json` and `encode.log` or `decode.log` identify the phase and current progress. The one-shot worker obtains an exclusive lock. A poll can safely run the following from the repository root.

```sh
python3 work/ppp-overnight-20261125/recover_paq.py --ensure work/ppp-overnight-20261125/full-enwik8-live
```

A repeated call while the worker owns the lock returns immediately. Once a full PAQ encode completes, a hash-checked checkpoint allows interrupted decode to be retried independently. A stopped partial encode is kept in its own attempt directory. The publisher requires the full recovered 100,000,000-byte SHA-256 and checks the tracker for an existing entry. A publication error can be retried from the completed result using the same command. [Recovery snapshot](recovery.json) · [Recovery runner](recover_paq.py) · [Publisher](record_completed.py)
