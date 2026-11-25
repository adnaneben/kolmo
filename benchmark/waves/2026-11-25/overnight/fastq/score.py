"""Validate all frame, offset and input receipts and calculate paired phase medians."""
from collections import defaultdict
import csv,hashlib,json,statistics,struct,sys
from pathlib import Path
here=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).resolve().parent
meta=json.loads((here/"inputs.json").read_text())
assert hashlib.sha256((here/"experiment.patch").read_bytes()).hexdigest()==meta["patch_sha256"]
data={}
for filename in ["raw.csv","automatic.csv"]:
    arms=defaultdict(list)
    for r in csv.DictReader((here/filename).open()):
        s=meta["inputs"][r["accession"]]["slices"][r["slice"]]
        assert r["sha256"]==r["restored_sha256"]==s["sha256"]
        if int(r["round"])==0:assert r["tagged_reader_restore"]==r["sha256"]
        cuts=list(map(int,r["outer_offsets"].split("|")))
        assert cuts[0]==0 and cuts[-1]==s["input_bytes"] and all(b>a for a,b in zip(cuts,cuts[1:]))
        assert int(r["blocks"])==len(cuts)-1==len(r["codecs"].split("|"))
        assert hashlib.sha256(b"".join(struct.pack("<Q",x) for x in cuts)).hexdigest()==r["outer_offsets_sha256"]
        if r["arm"]=="aligned" and r["level"]=="U":assert int(r["cuts_on_record"])==len(cuts)-2
        assert int(r["encode_ns"])>0 and int(r["stock_decode_ns"])>0
        arms[r["accession"],r["slice"],r["level"],r["arm"]].append(r)
    assert sum(map(len,arms.values()))==({"raw.csv":32,"automatic.csv":24}[filename])
    for (run,subset,level,arm),rows in arms.items():
        assert len({r["stored_sha256"] for r in rows})==1
        assert len(rows)==(1 if subset=="single" else 3)
        other=arms[run,subset,level,"fixed"]
        if subset=="single" or level=="3" or filename=="automatic.csv":assert rows[0]["stored_sha256"]==other[0]["stored_sha256"]
        e=[int(r["encode_ns"]) for r in rows];d=[int(r["stock_decode_ns"]) for r in rows];n=int(rows[0]["raw_bytes"])
        e,d,ed=[statistics.median(x) for x in (e,d,[a+b for a,b in zip(e,d)])]
        data[filename,run,subset,level,arm]=[n,int(rows[0]["stored_bytes"]),rows[0]["stored_sha256"],rows[0]["blocks"],rows[0]["codecs"],
                                            round(e/1e6,4),round(d/1e6,4),round(n*1000/e,3),round(n*1000/d,3),round(n*1000/ed,3)]
with (here/"summary.csv").open("w",newline="") as of:
    w=csv.writer(of);w.writerow(["settings","run","slice","level","arm","input_bytes","archive_bytes","archive_sha256","blocks","codecs",
                                 "encode_ms","decode_ms","encode_MBps","decode_MBps","combined_MBps"])
    for key,values in sorted(data.items()):w.writerow(["explicit-1-MiB" if key[0]=="raw.csv" else "automatic",*key[1:],*values])
controls=[json.loads(line) for line in (here/"regression.jsonl").read_text().splitlines()]
assert len(controls)==8 and {row["case"] for row in controls}=={"variable-length","other-level","mixed-tail","mismatched-header","mismatched-field-length","unterminated","oversized-record","ordinary-text"}
for row in controls:
    assert row["restore"]=="exact"
    assert (row["stock"]!=row["opt_in"])==(row["case"]=="variable-length")
print("Verified eight regression controls across three encoder arms.")
print("Validated 56 final stock restores, 24 independent v0.0.2 restores, expected block alignment and stable frame hashes.")
