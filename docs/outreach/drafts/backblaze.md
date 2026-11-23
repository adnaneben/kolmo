# Backblaze Drive Stats: blog comment (not posted)

**Where:** a comment on the latest Drive Stats post on backblaze.com/blog. Keep it short; link to the report.

---

Thanks for publishing Drive Stats. We measured an open-source compressor, Kolmo (Apache-2.0), on one day of the raw
data (`data_Q4_2025/2025-12-31.csv`, 133 MB). All tools ran on one thread, and every output was decompressed and
compared byte for byte:

- **Your zip:** ratio 10.49.
- **zstd -19:** 13.60, at 1.9 MiB/s.
- **Kolmo's default level:** 18.03, at 106 MiB/s.
- **Kolmo -9:** 22.72, about 2.2x smaller than the zip.

The catch: Kolmo decodes this file at
200-300 MiB/s, slower than zip.

Report and method:
<https://github.com/adnaneben/kolmo/blob/main/docs/outreach/evidence/first-wave/backblaze.md>.
If it is useful, we are glad to run the free report on a whole quarter.
