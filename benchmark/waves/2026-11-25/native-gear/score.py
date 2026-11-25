"""Emit medians of independent phases, paired wall ratios and A/A controls."""
import csv, sys
from collections import defaultdict
from statistics import median
d = defaultdict(dict)
for row in csv.DictReader(open(sys.argv[1])):
    if int(row["round"]) < 0:
        continue
    d[row["source"], row["threads"]].setdefault(row["arm"], {})[row["round"]] = row
w = csv.writer(sys.stdout)
w.writerow("source threads arm n bytes encode_MBps decode_MBps combined_MBps paired_gain_vs_main paired_gain_vs_AA paired_gain_vs_C paired_faster_than_main".split())
def total(row):
    return float(row["encode_ms"])+float(row["decode_ms"])
for (source, threads), arms in d.items():
    for name, rows in arms.items():
        first = next(iter(rows.values()))
        z = int(first["size"])/1000
        t = [total(r) for r in rows.values()]
        ratio = lambda ref: median(total(arms[ref][i])/total(r) for i, r in rows.items())
        w.writerow([source, threads, name, len(rows), first["archive_bytes"],
                    round(z/median(float(r["encode_ms"]) for r in rows.values()),2),
                    round(z/median(float(r["decode_ms"]) for r in rows.values()),2),
                    round(z/median(t),2), round(ratio("main"),4), round(ratio("main-AA"),4),
                    round(ratio("paired-C"),4), sum(total(r)<total(arms["main"][i]) for i,r in rows.items())])
