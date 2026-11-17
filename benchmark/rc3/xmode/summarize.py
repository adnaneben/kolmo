"""Markdown tables from run.py CSVs: one row per (file, threads, label).

usage: python3 -I benchmark/rc3/xmode/summarize.py A.csv [B.csv ...]
"""
import csv, sys
rows = []
for p in sys.argv[1:]:
    with open(p) as f: rows += list(csv.DictReader(f))
order = {k: i for i, k in enumerate(dict.fromkeys(r["label"] for r in rows))}
sz = {"100k": 0, "1m": 1, "10m": 2, "100m": 3}
kind = {"text": 0, "csv": 1, "bin": 2, "osdb": 3, "fq": 4}
def key(r):
    k, s = r["file"].split("_")
    return (sz[s], kind[k], int(r["threads"]), order[r["label"]])
print("| file | T | step | size | ratio | enc MB/s | enc peak MiB | dec MB/s | dec peak MiB |")
print("|---|---:|---|---:|---:|---:|---:|---:|---:|")
for r in sorted(rows, key=key):
    print(f"| {r['file']} | {r['threads']} | {r['label']} | {int(r['size']):,} | {r['ratio']} | {float(r['enc_MBps']):.3f} | {r['enc_MiB']} | "
          f"{float(r['dec_MBps']):.3f} | {r['dec_MiB']} |" if r['dec_MBps'] else "| | |")
