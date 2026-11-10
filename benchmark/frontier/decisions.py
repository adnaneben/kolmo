"""Strict per-setting wins, same-budget gates, and exact-output preservation."""
import argparse
import json
from pathlib import Path
from statistics import median
from panel import MARKET,OPTIONS,write_json

def read(path):return json.loads(path.read_text())
def triple(c,r):
    return c["bytes"]<r["bytes"] and c["encode"]>r["encode"] and c["decode"]>r["decode"]
def margin(c,r):
    return dict(fewer_bytes_pct=100*(1-c["bytes"]/r["bytes"]),
                encode_speedup=c["encode"]/r["encode"],decode_speedup=c["decode"]/r["decode"])
def external(k):return k not in ("kbc","krc","m1_fast") and not k.startswith("kolmo_")
def decide(options,expected):
    eligible={k:v for k,v in options.items() if v.get("valid")}
    rivals={k:v for k,v in eligible.items() if external(k)}
    if not rivals:return {}
    best={}
    for k,op in (("bytes",min),("encode",max),("decode",max)):
        value=op(x[k] for x in rivals.values());best[k]=[c for c,v in rivals.items() if v[k]==value]
    candidates={k:v for k,v in eligible.items() if not external(k)}
    winners={k:[a for a,r in rivals.items() if triple(v,r)] for k,v in candidates.items()}
    return dict(external_settings_measured=len(rivals),external_settings_expected=len(expected),
        missing=sorted(set(expected)-set(rivals)),leaders=best,
        strict_all=[k for k,v in winners.items() if len(v)==len(expected)],
        paired_zstd={k:dict(strict=triple(v,rivals["zstd3"]),**margin(v,rivals["zstd3"])) for k,v in candidates.items()} if "zstd3" in rivals else {},
        count_beaten_by_candidate={k:len(v) for k,v in winners.items()})

def costs(v):
    return dict(bytes=v["size"],encode_ms=v["encode_ms"],decode_ms=v["decode_ms"],
                encode_rss=v["encode_rss"],decode_rss=v["decode_rss"])
def gate(c,r):
    checks={k:c[k]<=r[k] for k in c if k!="bytes"}
    checks["size_saving_at_least_20pct"]=5*c["bytes"]<=4*r["bytes"]
    return dict(checks=checks,all_limits=all(checks.values()),
                three_metrics=c["bytes"]<r["bytes"] and c["encode_ms"]<r["encode_ms"] and c["decode_ms"]<r["decode_ms"],
                size_saving_pct=100*(1-c["bytes"]/r["bytes"]),encode_speedup=r["encode_ms"]/c["encode_ms"],
                decode_speedup=r["decode_ms"]/c["decode_ms"])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);p.add_argument("out",type=Path);a=p.parse_args()
    w=a.work
    expected={k for k in set(MARKET)|set(OPTIONS) if external(k)}
    contents={};groups={}
    for path in (w/"observations").glob("*.json"):
        d=read(path);groups.setdefault(d["input"],{})[d["codec"]]=d
    groups["archive"]={f.stem:read(f) for f in (w/"board").glob("*.json")}
    for f in (w/"m1-panel").glob("*.json"):
        d=read(f);k=d["input"] if d["input"]!="whole-silesia" else "archive"
        groups.setdefault(k,{})["m1_fast"]=d
    contents["local_lzbench"]={k:decide(v,expected) for k,v in groups.items()}
    cli={}
    for folder in ("confirmation","transfer-results"):
        for path in (w/folder).glob("*.json"):
            d=read(path);m=d.get("metrics",{})
            if not m:continue
            c=costs(m["candidate"])
            vs={k:gate(c,costs(v)) for k,v in m.items() if k!="candidate"}
            cli[path.stem]=dict(bytes=d["source_bytes"],source_sha256=d["source_sha256"],costs=c,against=vs,
                invalid=[k for k in d["commands"] if k not in m])
    contents["file_trials"]=cli
    controls={}
    for f in (w/"controls").glob("*.json"):
        d=read(f)
        if f.stem=="compatibility" or "metrics" not in d:continue
        m=d["metrics"];c=costs(m["candidate"]);r=costs(m["baseline"])
        controls[f.stem]={"against_previous":gate(c,r),"frame_identical":
            {row["frame_sha256"] for row in d["trials"] if row["codec"]=="candidate"}==
            {row["frame_sha256"] for row in d["trials"] if row["codec"]=="baseline"}}
    contents["controls"]=controls
    for key in ("compact-dickens","compact-osdb","compact-b-dickens","compact-whole"):
        f=w/(key+".json")
        if not f.exists():continue
        d=read(f)
        if "metrics" not in d:continue
        contents.setdefault("ultra",{})[key]=dict(metrics=d["metrics"],same_frame=len({r["frame_sha256"] for r in d["trials"]})==1)
    write_json(a.out,contents)
if __name__=="__main__":main()
