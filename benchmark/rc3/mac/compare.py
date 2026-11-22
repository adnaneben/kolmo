"""rc3 against rc2: per group, mode and thread count, total size change and speed ratios (rc3 MB/s / rc2 MB/s) over
the files both versions measured. usage: compare.py ROWS.jsonl ..."""
import collections, json, sys
rows = {}
for p in sys.argv[1:]:
    for l in open(p):
        r = json.loads(l)
        if r.get("type") == "row" and "error" not in r and r.get("roundtrip", True):
            rows[(r["group"], r["file"], r["label"])] = r
groups = sorted({g for g, _, _ in rows})
bad = [k for k, r in rows.items() if r.get("roundtrip") is False]
for g in groups:
    print(f"\n### {g}\n\n| mode | threads | files | size rc3 vs rc2 | compress speed | decompress speed |\n|---|---|---|---:|---:|---:|")
    for T in ("1", "10"):
        for m in ("speed", "balanced", "eco", "hc", "ratio", "x"):
            a = {f: r for (gg, f, l), r in rows.items() if gg == g and l == f"rc2 {m} -T {T}"}
            b = {f: r for (gg, f, l), r in rows.items() if gg == g and l == f"rc3 {m} -T {T}"}
            fs = sorted(set(a) & set(b))
            if not fs: continue
            n = sum(a[f]["input"] for f in fs)
            za, zb = sum(a[f]["compressed"] for f in fs), sum(b[f]["compressed"] for f in fs)
            ca, cb = sum(a[f]["c_s"] for f in fs), sum(b[f]["c_s"] for f in fs)
            da, db = sum(a[f]["d_s"] for f in fs), sum(b[f]["d_s"] for f in fs)
            print(f"| {'-x' if m == 'x' else m} | {T} | {len(fs)} | {100 * (zb / za - 1):+.2f}% | {ca / cb:.2f}x | {da / db:.2f}x |")
print("\nround-trip failures:", bad or "none")
