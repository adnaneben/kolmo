"""Prepare geographically disjoint electricity data and complete container bytes."""
import argparse
from collections import Counter
import csv
from datetime import datetime, timezone
from functools import lru_cache
import json
from pathlib import Path
import sqlite3
import sys
import zipfile

from panel import sha,write_json
from file_trials import measure
HERE=Path(__file__).resolve().parent
EIA="https://www.eia.gov/electricity/gridmonitor/sixMonthFiles/EIA930_BALANCE_2020_Jan_Jun.csv"

@lru_cache(maxsize=8192)
def parse_stamp(s):
    return datetime.strptime(s,"%m/%d/%Y %I:%M:%S %p")

def electricity(work):
    d=work/"transfer";src=d/"eia.csv"
    with src.open(encoding="utf-8-sig",newline="") as stream:
        reader=csv.DictReader(stream,strict=True)
        field="UTC Time at End of Hour"
        assert field in reader.fieldnames
        times=Counter(parse_stamp(row[field]) for row in reader)
    split=sorted(times)[len(times)//4]
    limit=4<<20
    with src.open("rb") as stream:
        header=stream.readline();fields=next(csv.reader([header.decode("utf-8-sig")],strict=True))
        keys=fields.index(field)
        train=d/"train.csv";test=d/"test.csv";counts=[0,0]
        with train.open("wb") as a,test.open("wb") as b:
            a.write(header);b.write(header)
            seen=0
            for line in stream:
                row=next(csv.reader([line.decode("utf-8")],strict=True))
                assert len(row)==len(fields)
                if parse_stamp(row[keys])>=split:
                    b.write(line);counts[1]+=1
                elif a.tell()+len(line)<=limit:
                    a.write(line);counts[0]+=1
                seen+=1
            assert seen==sum(times.values())
    return dict(url=EIA,source_bytes=src.stat().st_size,source_sha256=sha(src),
              cutoff_utc=split.replace(tzinfo=timezone.utc).isoformat(),distinct_hours=len(times),
              train_rows=counts[0],test_rows=counts[1],train_bytes=train.stat().st_size,
              train_sha256=sha(train),target_bytes=test.stat().st_size,target_sha256=sha(test))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);p.add_argument("zli",type=Path)
    a=p.parse_args();w=a.work.resolve();d=w/"transfer"
    e=electricity(w)
    rows={"eia":e}
    with sqlite3.connect("file:"+str(d/"chinook.sqlite")+"?mode=ro",uri=True) as db:
        assert db.execute("PRAGMA integrity_check").fetchall()==[("ok",)]
    poi=json.loads((d/"poi-list.json").read_text())
    chosen=sorted((r for r in poi if r['name'].endswith(".docx") and (1<<20)<=r["size"]<=(8<<20)),key=lambda r:(-r["size"],r["name"]))[0]
    docx=d/chosen["name"]
    with zipfile.ZipFile(docx) as z:
        assert "word/document.xml" in z.namelist() and z.testzip() is None
    assert docx.stat().st_size==chosen["size"]
    for k,file,url in (("docx",docx,chosen["download_url"]),("sqlite",d/"chinook.sqlite",
           "https://raw.githubusercontent.com/lerocha/chinook-database/master/ChinookDatabase/DataSources/Chinook_Sqlite.sqlite")):
        rows[k]=dict(name=file.name,url=url,target_bytes=file.stat().st_size,target_sha256=sha(file))
    rows["docx"]["git_blob_sha1"]=chosen["sha"]
    for name,path in (("flac","cellar-01.flac"),("png","coffee.png"),("y4m","akiyo.y4m")):
        src=w/"holdouts"/path
        rows[name]=dict(name=path,target_bytes=src.stat().st_size,target_sha256=sha(src))
    write_json(HERE/"transfer-inputs.json",rows)
    graph=d/"eia.zlc"
    if not graph.exists():
        t=measure([a.zli,"train","--profile","csv","--threads","4","--max-time-secs","10",
                   d/"train.csv","--output",graph])
        write_json(d/"training.json",dict(**t,graph_sha256=sha(graph),graph_bytes=graph.stat().st_size,
             train_sha256=e["train_sha256"],trainer_sha256=sha(a.zli)))
    print("EIA",e["test_rows"],e["target_bytes"],"bytes; graph",graph.stat().st_size,flush=True)
if __name__=="__main__":
    main()
