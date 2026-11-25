"""Compute per-cell medians and within-pair changes for replay.py receipts."""
import argparse
from collections import defaultdict
import csv
import json
from pathlib import Path
from statistics import median

p = argparse.ArgumentParser()
p.add_argument("out", type=Path)
p.add_argument("inputs", nargs="+", type=Path)
args = p.parse_args()
groups = defaultdict(lambda: defaultdict(dict))
for path in args.inputs:
    for line in path.read_text().splitlines():
        r = json.loads(line)
        assert r["restore_sha256"] == r["input_sha256"]
        if r["rep"]:
            key = r["file"],r["n"],r["level"],r["threads"]
            assert r["rep"] not in groups[key][r["arm"]]
            groups[key][r["arm"]][r["rep"]] = r

with args.out.open("w") as out:
    csvout = None
    for (name,n,level,threads), cells in groups.items():
        for arm, trials in sorted(cells.items()):
            metric = lambda r,k: n / ((r["enc"] + r["dec"]) if k == "combined" else r[k]) / 1e6
            times = list(trials.values())
            assert len(set(r["archive_sha256"] for r in times)) == 1
            rec = dict(file=name, n=n, whole=times[0]["whole"], level=level, threads=threads,
                       arm=arm, rounds=len(times), bytes=times[0]["bytes"],
                       ratio=round(n/times[0]["bytes"],4))
            for k in ("enc","dec","combined"):
                rec[k+"_MBps"] = round(median(metric(r,k) for r in times),3)
            for k in ("encode_rss","decode_rss"):
                rec[k+"_MiB"] = round(median(r[k] for r in times)/(1<<20),3)
            speed, cpu = [], []
            for rep,r in trials.items():
                refs = [cells[a][rep] for a in ("A","A2")]
                speed.append(median(x["enc"]+x["dec"] for x in refs)/(r["enc"]+r["dec"]))
                cpu.append((r["encode_cpu"]+r["decode_cpu"])/median(x["encode_cpu"]+x["decode_cpu"] for x in refs))
            rec["speed_pct_vs_AA"] = round(100*(median(speed)-1),3)
            rec["cpu_pct_vs_AA"] = round(100*(median(cpu)-1),3)
            rec["faster_pairs"] = sum(x>1 for x in speed)
            if csvout is None:
                csvout = csv.DictWriter(out,fieldnames=rec.keys()); csvout.writeheader()
            csvout.writerow(rec)
