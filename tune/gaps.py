#!/usr/bin/env python3
"""Per-class comparison of cz modes with the leaders, from a czbench TSV (name, setting, T, size, packed, c MB/s, d MB/s, ok)."""
import sys, collections
rows = collections.defaultdict(dict)
for l in open(sys.argv[1]):
    f = l.rstrip('\n').split('\t')
    if len(f) < 8: continue
    rows[f[0]][f[1]] = (int(f[3]) / int(f[4]), float(f[5]), float(f[6]))
pairs = [("cz rt", "lz4"), ("cz fast", "zstd -3"), ("cz eco", "brotli -8"), ("cz hc", "zstd -19"), ("cz max", "xz -9e")]
groups = {"Tables": ["owid-co2-data.csv", "owid-energy-data.csv"], "Logs": ["loghub_all.log"], "Scientific": ["sao", "x-ray"],
          "Text": ["enwik8"], "Mixed": ["silesia.tar"]}
print(f"{'class':22} {'data':11} {'ratio cz/ldr':>14} {'comp cz/ldr':>16} {'dec cz/ldr':>16}")
for cz, ld in pairs:
    for g, fs in groups.items():
        fs = [f for f in fs if f in rows and cz in rows[f] and ld in rows[f]]
        if not fs: continue
        # aggregate: total size / total packed; speeds as harmonic (time-weighted) means
        def agg(name):
            r = sum(rows[f][name][0] for f in fs) / len(fs)
            c = len(fs) / sum(1 / rows[f][name][1] for f in fs); d = len(fs) / sum(1 / rows[f][name][2] for f in fs)
            return r, c, d
        a, b = agg(cz), agg(ld)
        flag = " <-- slower" if a[1] < b[1] * 0.97 or a[2] < b[2] * 0.97 else ""
        print(f"{cz+' vs '+ld:22} {g:11} {a[0]:5.2f}/{b[0]:<5.2f}   {a[1]:7.1f}/{b[1]:<7.1f} {a[2]:7.1f}/{b[2]:<7.1f}{flag}")
