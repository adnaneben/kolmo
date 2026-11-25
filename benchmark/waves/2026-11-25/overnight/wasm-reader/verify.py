"""Verify all valid pinned one-shot v12 fixtures through independent JavaScript engines."""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
def sha(b):
    return hashlib.sha256(b).hexdigest()
def require(pass_, message):
    if not pass_:
        raise ValueError(message)

def memlimits(wasm):
    """Get the actual memory section's declaration directly from the module."""
    def uleb(pos):
        v=0; shift=0
        while True:
            c=wasm[pos]; pos+=1
            v|=(c&127)<<shift
            if c<128:
                return v,pos
            shift+=7
    require(wasm[:8] == b"\x00asm\x01\x00\x00\x00", "unexpected module header")
    pos=8
    while pos < len(wasm):
        tag=wasm[pos]; size,start=uleb(pos+1); pos=start+size
        if tag!=5: continue
        count,i=uleb(start); flags,i=uleb(i)
        require(count==1 and flags==1,"expected one fixed-maximum, unshared 32-bit memory")
        minimum,i=uleb(i); maximum,i=uleb(i)
        require(i==pos and maximum in (4096,8192,12288,16384),"unexpected memory bounds")
        return minimum, maximum
    raise ValueError("no memory section")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ("corpus","node","jsc","out"):
        p.add_argument("--"+n,type=Path,required=True)
    p.add_argument("--module",type=Path,default=HERE/"reader.wasm")
    p.add_argument("--record-limits",action="store_true",help="retain all exact successes and genuine resource/portability failures")
    a=p.parse_args(); a.out.mkdir(parents=True,exist_ok=True)
    corpus=a.corpus.resolve()
    pin_file=corpus.parent/"v12.sha256"
    require(sha(pin_file.read_bytes())=="acf083f620ee945e6e693f44ef7230e4df85c70d31fb204d7582bd18e8ab9c4d", "unexpected fixture SHA-256 index")
    pins={k:h for h,k in (line.split("  ",1) for line in pin_file.read_text().splitlines())}
    manifest=corpus/"MANIFEST"
    require(pins["MANIFEST"]==sha(manifest.read_bytes()),"fixture list pin mismatch")
    rows=[]
    for line in manifest.read_text().splitlines():
        frame,original=line.split()
        b=(corpus/frame).read_bytes(); raw=(corpus/original).read_bytes()
        require(b.startswith(b"CZN1\x0c") and sha(b)==pins[frame] and sha(raw)==pins[original], "frame or original pin mismatch")
        rows.append(dict(frame_path=frame,frame_bytes=len(b),frame_sha256=sha(b),original_path=original,
                         original_bytes=len(raw),original_sha256=sha(raw)))
    require(len(rows)==357 and len(set(r["frame_path"] for r in rows))==357,"wrong pinned case count")
    minimum, maximum=memlimits(a.module.read_bytes())
    receipts=[]; engines={}
    with tempfile.TemporaryDirectory() as tmp:
        t=Path(tmp); cases=t/"cases.json";cases.write_text(json.dumps(rows))
        for label,exe in (("Node-V8",a.node),("JavaScriptCore",a.jsc)):
            dest=t/label;dest.mkdir()
            args=[exe] + (["--expose-gc"] if label=="Node-V8" else []) + [HERE/"run.js"] + ([] if label=="Node-V8" else ["--"]) + [a.module,cases,corpus,dest]
            r=subprocess.run([str(x) for x in args],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            require(r.returncode==0,label+" failed: "+r.stderr+" "+r.stdout[-500:])
            log=[json.loads(line) for line in r.stdout.splitlines()]
            require(len(log)==len(rows)+1 and not r.stderr.strip(),label+" incomplete or unexpected diagnostics: "+r.stderr)
            details=log.pop()
            for i,(ent,case) in enumerate(zip(log,rows)):
                require(ent["case"]==i, "fixture order changed")
                saved=dest/(str(i)+".bin")
                actual=""
                if ent["decoded"]==case["original_bytes"]:
                    decoded=saved.read_bytes(); actual=sha(decoded)
                    require(len(decoded)==case["original_bytes"] and actual==case["original_sha256"],
                            label+" restore mismatch on "+case["frame_path"])
                else:
                    require(a.record_limits and ent["decoded"]<0 and not saved.exists(),
                            label+" restore failed on "+case["frame_path"]+" result "+str(ent["decoded"]))
                require(minimum*65536==ent["initial_bytes"]<=ent["before_decode_bytes"]<=ent["after_decode_bytes"]<=maximum*65536,
                        "invalid linear memory counter")
                receipts.append(dict(engine=label,**case,output_sha256=actual,decode_result=ent["decoded"],**{k:v for k,v in ent.items() if "bytes" in k}))
            engines[label]=dict(executable_sha256=sha(exe.read_bytes()),**details,verified=sum(r["original_sha256"]==r["output_sha256"] for r in receipts if r["engine"]==label),
                                high_water_linear_bytes=max(x["after_decode_bytes"] for x in log))
    for n in engines:
        require(not any(k.startswith(("fd_","path_")) for k in engines[n]["calls"]), "host I/O reached")
    with (a.out/"restores.csv").open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=receipts[0]);w.writeheader();w.writerows(receipts)
    report=dict(module_sha256=sha(a.module.read_bytes()),module_bytes=a.module.stat().st_size,
                pinned_manifest_sha256=pins["MANIFEST"],v12_sha256_file_sha256=sha((corpus.parent/"v12.sha256").read_bytes()),
                memory_minimum_bytes=minimum*65536,memory_maximum_bytes=maximum*65536,
                decoded_bytes_per_engine=sum(r["original_bytes"] for r in rows),engines=engines)
    (a.out/"verified.json").write_text(json.dumps(report,indent=2)+"\n")
    print("Tested",len(rows),"pinned frames per engine; verified counts", {k:v["verified"] for k,v in engines.items()})
    print("One unshared Wasm memory: min",minimum,"max",maximum,"pages; high-water bytes:",
          {k:v["high_water_linear_bytes"] for k,v in engines.items()})
if __name__=="__main__":
    main()
