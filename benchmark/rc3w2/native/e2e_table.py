"""End-to-end table: Kolmo default against KOLMO_NATIVE, per mode (native lane, rc3 wave 2).

usage: python3 benchmark/rc3w2/native/e2e_table.py RESULTS_DIR
1 thread: the _total_ rows of libprof-{silesia,struct}{,-native,-strong}.tsv (one process per file and phase, summed).
4 threads: e2e-T4.tsv (best of 2 per file and phase)."""
import os, sys, collections
R = sys.argv[1]
def totals(path):
    t = {}
    if not os.path.exists(path): return t
    for l in list(open(path))[1:]:
        m, ph, e, calls, by, ms, outb, zb = l.rstrip("\n").split("\t")
        if e == "_total_":
            d = t.setdefault(m, {}); d[ph] = float(ms) / 1000; d["out"] = int(outb)
    return t
rows = []
for st, name in (("silesia", "Silesia"), ("struct", "structured")):
    lib = totals(f"{R}/libprof-{st}.tsv"); nat = totals(f"{R}/libprof-{st}-native.tsv"); strong = totals(f"{R}/libprof-{st}-strong.tsv")
    for m in ("speed", "balanced", "eco", "hc", "ratio"):
        for label, d in (("all", nat), ("all,strong", strong)):
            if m in lib and m in d:
                a, b = lib[m], d[m]
                rows.append((name, m, "1", label, a["out"], b["out"], a["c"], b["c"], a["d"], b["d"]))
t4 = collections.defaultdict(dict)
if os.path.exists(f"{R}/e2e-T4.tsv"):
    for l in open(f"{R}/e2e-T4.tsv"):
        ds, m, th, label, out, tc, td = l.rstrip("\n").split("\t")
        t4[(ds, m)][label] = (int(out), float(tc), float(td))
for (ds, m), d in t4.items():
    name = "Silesia" if ds == "silesia" else "structured"
    for label in ("all", "all,strong"):
        if "library" in d and label in d:
            a, b = d["library"], d[label]
            rows.append((name, m, "4", label, a[0], b[0], a[1], b[1], a[2], b[2]))
order = {"speed": 0, "balanced": 1, "eco": 2, "hc": 3, "ratio": 4}
rows.sort(key=lambda r: (r[0] != "Silesia", order[r[1]], r[3], r[2]))
print("| set | mode | threads | `KOLMO_NATIVE` | output, library | output, native | size | compress s (library → native) | decompress s (library → native) |")
print("|---|---|---|---|---|---|---|---|---|")
for name, m, th, label, ao, bo, ac, bc, ad, bd in rows:
    print(f"| {name} | {m} | {th} | {label} | {ao:,} | {bo:,} | {100 * (bo / ao - 1):+.2f}% | {ac:.2f} → {bc:.2f} ({bc / ac:.2f}×) | {ad:.2f} → {bd:.2f} ({bd / ad:.2f}×) |")
