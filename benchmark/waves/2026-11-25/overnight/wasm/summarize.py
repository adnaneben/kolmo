"""Regenerate the two benchmark summaries, checking all repetitions and readbacks."""
import csv
import json
from pathlib import Path
from statistics import median

root = Path(__file__).resolve().parent
def records(name):
    return list(map(json.loads,(root / name).read_text().splitlines()))

columns = ["suite","file","level","input_bytes","main_bytes","variant_bytes","bytes_pct",
           "main_e_MBps","variant_e_MBps","main_d_MBps","variant_d_MBps","main_ed_MBps","variant_ed_MBps",
           "paired_ed_speed_pct","aa2_ed_speed_pct","paired_cpu_time_pct",
           "variant_group_ms","variant_restore_ms","main_peak_RSS","variant_peak_RSS","input_sha256"]

def measurement(row, kind="wall", stage="both"):
    names = {"e":["group","encode"],"d":["decode","restore"],"both":list(row["times"])}[stage]
    return sum(row["times"][name][kind] for name in names if name in row["times"])

def speed(row, stage):
    return row["input_bytes"]/1e6/measurement(row, stage=stage)

def middle(xs):
    return round(median(xs),4)

with (root/"results.csv").open("w",newline="") as output:
    writer=csv.writer(output)
    writer.writerow(columns)
    for suite, path in [("group","observations.jsonl"),("analysis-none","analysis-none.jsonl"),("guard","guard.jsonl")]:
        rows=records(path)
        assert len(rows)==(504 if suite=="guard" else 288)
        cases = sorted({(r["file"],r["level"]) for r in rows})
        assert len(cases)==(28 if suite=="guard" else 16)
        for name,level in cases:
            arms={}
            for arm in ("A","A2","B"):
                data=sorted([r for r in rows if (r["file"],r["level"],r["arm"])==(name,level,arm)],key=lambda r:r["rep"])
                assert [r["rep"] for r in data]==list(range(6))
                assert len({r["frame_sha256"] for r in data})==1
                assert all(r["decoded_sha256"]==r["sha256"] for r in data)
                arms[arm]=data[1:]
            a,a2,b=arms["A"],arms["A2"],arms["B"]
            assert a[0]["frame_sha256"]==a2[0]["frame_sha256"]
            ab,bb=a[0]["frame_bytes"],b[0]["frame_bytes"]
            rates=[middle(speed(r,stage) for r in x) for stage in ("e","d","both") for x in (a,b)]
            paired=middle(100*((measurement(x)+measurement(y))/(2*measurement(z))-1) for x,y,z in zip(a,a2,b))
            control=middle(100*(measurement(x)/measurement(y)-1) for x,y in zip(a,a2))
            cpu=middle(100*(2*measurement(z,"cpu")/(measurement(x,"cpu")+measurement(y,"cpu"))-1) for x,y,z in zip(a,a2,b))
            prep=[middle(1000*r["times"].get(k,{}).get("wall",0) for r in b) for k in ("group","restore")]
            rss=[middle(max(t["rss"] for t in r["times"].values()) for r in x) for x in (a,b)]
            writer.writerow([suite,name,level,a[0]["input_bytes"],ab,bb,round(100*(bb/ab-1),4),
                             *rates,paired,control,cpu,*prep,*rss,a[0]["sha256"]])
