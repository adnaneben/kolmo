"""Verify full input and archive declarations, then report medians of complete pack/restore pairs."""
import collections
import csv
import json
from pathlib import Path
import statistics

root=Path(__file__).resolve().parent
manifest=json.loads((root/"whole-inputs.json").read_text())
old=json.loads((root/"historical-checkpoints.json").read_text())["measurements"]
groups=collections.defaultdict(list)
with (root/"whole-raw.csv").open() as f:
    for row in csv.DictReader(f):
        groups[row["model"],row["method"]].append(row)
assert {model for model,_ in groups}=={"smollm2-135m","qwen2.5-0.5b"}
assert len(groups)==4
with (root/"whole-main.csv").open() as mf:
    mains={r["model"]:r for r in csv.DictReader(mf)}
assert set(mains)=={k[0] for k in groups}
fields=["model","method","input_bytes","archive_bytes","archive_sha256","bytes_delta_vs_historical","input_over_archive",
        "encode_wall_s","decode_wall_s","paired_wall_s","encode_MBps","decode_MBps","paired_MBps",
        "encode_peak_RSS_MiB","decode_peak_RSS_MiB","zip_ready_encode_s","zip_ready_decode_s","zip_ready_paired_MBps"]
with (root/"whole-score.csv").open("w",newline="") as dest:
    w=csv.writer(dest);w.writerow(fields)
    for (model,method),rows in sorted(groups.items()):
        info=manifest["inputs"][model]
        assert len(rows)==4 and [int(r["trial"]) for r in rows]==list(range(4))
        assert len({r["packed_sha256"] for r in rows})==1 and len({r["packed_bytes"] for r in rows})==1
        if method=="kolmo_L3":
            assert rows[0]["tag_restored_sha256"]==info["sha256"]
            baseline=mains[model]
            assert baseline["commit"]=="a1d39559686a9ec17c8614b9154a64d7b5b98672" and baseline["restored_sha256"]==info["sha256"]
            assert baseline["packed_sha256"]==rows[0]["packed_sha256"] and baseline["packed_bytes"]==rows[0]["packed_bytes"]
        sizes=int(rows[0]["input_bytes"]),int(rows[0]["packed_bytes"])
        n,packed=sizes
        assert n==info["bytes"] and n==8+info["header_bytes"]+info["payload_bytes"]
        assert info["tensor_count"]==sum(info["dtypes"].values()) and info["dtypes"]=={"BF16":info["tensor_count"]}
        for r in rows:
            assert int(r["input_bytes"])==n and r["input_sha256"]==r["restored_sha256"]==info["sha256"]
            assert int(r["order"])==int((int(r["trial"])%2) ^ (method=="zipnn_0.5.4"))
            assert int(r["encode_rss_bytes"])>0 and int(r["decode_rss_bytes"])>0
        hist=next(v for k,v in old.items() if model in k)
        old_method="kolmo -3" if method=="kolmo_L3" else "zipnn [bfloat16]"
        historical_size=next(x["size"] for x in hist["rows"] if x["tool"]==old_method)
        timed=rows[1:]
        e=[int(r["encode_wall_ns"]) for r in timed];d=[int(r["decode_wall_ns"]) for r in timed]
        ep,dp,paired=(statistics.median(series) for series in (e,d,[a+b for a,b in zip(e,d)]))
        peaks=[max(int(r[f"{k}_rss_bytes"]) for r in timed)/2**20 for k in ("encode","decode")]
        ready=["","",""]
        if method.startswith("zipnn"):
            re=[int(r["zip_encode_ready_ns"]) for r in timed];rd=[int(r["zip_decode_ready_ns"]) for r in timed]
            assert all(0<a<b for a,b in zip(re,e)) and all(0<a<b for a,b in zip(rd,d))
            ready=[statistics.median(re)/1e9,statistics.median(rd)/1e9,
                   n*1000/statistics.median([a+b for a,b in zip(re,rd)])]
        values=[model,method,n,packed,rows[0]["packed_sha256"],packed-historical_size,n/packed,ep/1e9,dp/1e9,paired/1e9,
                n*1000/ep,n*1000/dp,n*1000/paired,*peaks,*ready]
        w.writerow([f"{x:.6f}" if isinstance(x,float) else x for x in values])
print("Verified 16 full-file pack/restore rows, two v0.0.2 restores, two pinned-main frame matches, alternating paired order, "
      "peak RSS and stable whole-frame hashes. Medians exclude four warm-up rows.")
