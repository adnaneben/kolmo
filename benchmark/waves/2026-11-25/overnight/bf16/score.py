"""Recompute speed from raw phase times and validate every original and complete frame."""
import csv, json, statistics
from collections import defaultdict
from pathlib import Path
here = Path(__file__).resolve().parent
manifest = json.loads((here / "inventory.json").read_text())
groups = defaultdict(list)
for fname in ["raw.csv","cold.csv"]:
    with (here / fname).open() as f:
        for row in csv.DictReader(f):
            item=manifest["inputs"][row["source"]]
            assert row["input_sha256"] == row["restored_sha256"] == item["sha256"]
            assert int(row["input_bytes"]) == item["declared_total_bytes"]
            assert int(row["encode_ns"]) > 0 and int(row["decode_ns"]) > 0
            groups[row["source"],row["method"]].append(row)
assert len(groups)==12
fields=["source","method","input_bytes","stored_bytes","stored_sha256","ratio","encode_MBps","decode_MBps","combined_MBps"]
with (here / "score.csv").open("w",newline="") as out:
    writer=csv.writer(out);writer.writerow(fields)
    for (source,method),rows in sorted(groups.items()):
        assert len(rows)==3 and {r["round"] for r in rows}=={"1","2","3"}
        assert len({r["stored_sha256"] for r in rows})==len({r["stored_bytes"] for r in rows})==1
        size,packed=map(int,[rows[0]["input_bytes"],rows[0]["stored_bytes"]])
        e=[int(r["encode_ns"]) for r in rows]; d=[int(r["decode_ns"]) for r in rows]
        speeds=[size*1000/statistics.median(ns) for ns in (e,d,[a+b for a,b in zip(e,d)])]
        writer.writerow([source,method,size,packed,rows[0]["stored_sha256"],f"{size/packed:.6f}",*[f"{x:.3f}" for x in speeds]])
    for source in {k[0] for k in groups}:
        for left,right in [("ZipNN","ZipNN fresh CLI"),("main-L3","#104-L3")]:
            assert groups[source,left][0]["stored_sha256"]==groups[source,right][0]["stored_sha256"]
print("36 full-file restores match both original corpus files. Main and #104 L3 frames match; ZipNN startup modes match.")
