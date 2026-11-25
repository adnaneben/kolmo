"""Measure graph ownership on the existing one-shot corpus and archived review frames."""
import argparse
from collections import Counter
import csv
from hashlib import sha256
import json
import os
from pathlib import Path
import random
from statistics import median
import struct
import sys
import tempfile

CATS = ["metadata","leaves","inverse_buffers","fields_scratch","dec64_scratch"]
OPS = ["FIELDS","DECIMAL","CONCAT","INT","FLOAT","IMG8","DEC64"]
FIELDS = ["graph_payload_bytes","block_decoded_bytes","peak_owned","allocated_versions","resizes","view_unique_bytes_peak",
          "view_backing_capacity_peak","fused_cursor_bytes_peak","fused_columns","dictionary_scratch_peak","live_at_exit",
          *["max_"+c for c in CATS], *["at_peak_"+c for c in CATS], *OPS]
HERE=Path(__file__).resolve().parent
PINS_INDEX="acf083f620ee945e6e693f44ef7230e4df85c70d31fb204d7582bd18e8ab9c4d"

def digest(data):
    return sha256(data).hexdigest()

def check(ok,msg):
    if not ok: raise ValueError(msg)

def graph_nodes(frame):
    nb=struct.unpack_from("<I",frame,20)[0]
    offset=32+16*nb
    result=[]
    for i in range(nb):
        raw,cl,_,codec=struct.unpack_from("<IIIB",frame,32+16*i)
        payload=frame[offset+4:offset+cl]
        offset+=cl
        if codec!=12: continue
        check(payload[:2]==b"G\x01","invalid graph marker on known-good frame")
        j=2
        def uv():
            nonlocal j
            value=shift=0
            while True:
                c=payload[j]; j+=1;value|=(c&127)<<shift
                if c<128: return value
                shift+=7
        counts=Counter()
        for _ in range(uv()):
            op=payload[j];j+=1;counts[OPS[op-1]]+=1
            for _ in range(uv()): uv()
            uv(); length=uv();j+=length
        result.append(dict(block=i,graph_payload_bytes=len(payload),block_decoded_bytes=raw,**{op:counts[op] for op in OPS}))
    return result

def write(path,rows):
    with path.open("w",newline="") as f:
        w=csv.DictWriter(f,fieldnames=rows[0]);w.writeheader();w.writerows(rows)

def run(exe,frame,destination):
    with tempfile.TemporaryFile() as log:
        actions=[(os.POSIX_SPAWN_DUP2,log.fileno(),2),(os.POSIX_SPAWN_OPEN,1,os.devnull,os.O_WRONLY,0)]
        argv=[str(exe),"-dq","-T1","--memlimit","512M","-o",str(destination),"--",str(frame)]
        env={k:v for k,v in os.environ.items() if not k.startswith(("KOLMO_", "CZN_"))}
        pid=os.posix_spawn(argv[0],argv,env,file_actions=actions)
        _,status,usage=os.wait4(pid,0)
        log.seek(0);text=log.read().decode()
        check(os.waitstatus_to_exitcode(status)==0,text)
        parsed=[]
        for line in text.splitlines():
            check(line.startswith("KGTRACE "),line)
            numbers=[int(v) for v in line.split()[1:]]
            check(len(numbers)==len(FIELDS),line)
            parsed.append(dict(zip(FIELDS,numbers)))
        return usage.ru_maxrss*(1 if sys.platform=="darwin" else 1024),parsed

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ["corpus","review","stock","probe","out"]: p.add_argument("--"+k,type=Path,required=True)
    a=p.parse_args(); a.out.mkdir(parents=True,exist_ok=True)
    c=a.corpus;r=a.review/"review40b/work/R30/enc";m=c/"MANIFEST"
    pin_file=c.parent/"v12.sha256";check(digest(pin_file.read_bytes())==PINS_INDEX,"frozen fixture index changed")
    pins={name:h for h,name in (l.split("  ",1) for l in pin_file.read_text().splitlines())}
    check(digest(m.read_bytes())==pins["MANIFEST"],"frozen fixture manifest changed")
    cases=[]
    for line in m.read_text().splitlines():
        frame,orig=line.split();fb=(c/frame).read_bytes()
        if graph_nodes(fb):
            check(digest(fb)==pins[frame] and digest((c/orig).read_bytes())==pins[orig],"frozen fixture changed")
            cases.append(("frozen",frame,c/frame,c/orig))
    review_frames=["wide_test.csv.L3","wide_test.csv.L9","owid_energy_f64.npy.L3","owid_energy_f64.npy.L9",
                   "population.i64.L3","llm_bf16_10mb.safetensors.L3","retina.ppm.L3"]
    for name in review_frames:
        orig=name.rsplit(".",1)[0];path=r/(name+".kolmo")
        check(graph_nodes(path.read_bytes()),"selected review fixture contains no directly visible graph")
        cases.append(("R30",path.name,path,a.review/"corpus"/orig))
    freeze=[dict(source=kind,frame=name,frame_bytes=f.stat().st_size,frame_sha256=digest(f.read_bytes()),
                 original=o.name,original_bytes=o.stat().st_size,original_sha256=digest(o.read_bytes()))
            for kind,name,f,o in cases]
    reference=HERE/"fixture-hashes.json"
    if reference.exists():
        check(json.loads(reference.read_text())==freeze,"frame or original diverged from recorded hash")
    (a.out/"fixture-hashes.json").write_text(json.dumps(freeze,indent=2)+"\n")
    raw=[];blocks=[];rng=random.Random(2026112533)
    with tempfile.TemporaryDirectory() as t:
        output=Path(t)/"out"
        for idx,(kind,name,f,o) in enumerate(cases):
            expected=graph_nodes(f.read_bytes()); first=None
            for rep in range(3):
                for arm,exe in rng.sample([("stock",a.stock),("probe",a.probe)],2):
                    rss,entries=run(exe,f,output)
                    restored=digest(output.read_bytes());output.unlink()
                    check(restored==freeze[idx]["original_sha256"],"wrong decoded output")
                    raw.append(dict(source=kind,frame=name,repeat=rep,arm=arm,process_maxrss_bytes=rss,output_sha256=restored))
                    if arm=="stock":
                        check(not entries,"stock printed diagnostics");continue
                    check(len(entries)==len(expected),f"wrong graph count on {name}")
                    for e,ref in zip(entries,expected):
                        check(all(e[k]==v for k,v in ref.items() if k!="block"),"instrumented call and independent graph parse disagree")
                        check(e["live_at_exit"]==0 and sum(e["at_peak_"+k] for k in CATS)==e["peak_owned"]<=128*2**20,
                              "inconsistent peak accounting")
                        if e["view_unique_bytes_peak"]:
                            check(e["view_unique_bytes_peak"]<=e["view_backing_capacity_peak"]<=e["peak_owned"],"inconsistent alias peak")
                    if first is None:
                        for e,ref in zip(entries,expected):
                            blocks.append(dict(source=kind,frame=name,block=ref["block"],**e))
                        first=entries
                    else: check(first==entries,"allocation events changed on repeat")
            print(kind,name,len(expected),"graphs verified",flush=True)
    summaries=[]
    for info in freeze:
        kind,name=info["source"],info["frame"]
        filt=lambda rows:[row for row in rows if row["source"]==kind and row["frame"]==name]
        rs,bs=filt(raw),filt(blocks)
        summary=dict(source=kind,frame=name,original_bytes=info["original_bytes"],frame_bytes=info["frame_bytes"],
                     graph_blocks=len(bs),peak_owned=max(i["peak_owned"] for i in bs))
        for label in ["max_fields_scratch","max_dec64_scratch","dictionary_scratch_peak","view_unique_bytes_peak","view_backing_capacity_peak","fused_cursor_bytes_peak"]:
            summary[label]=max(i[label] for i in bs)
        for arm in ("stock","probe"):
            rr=[e["process_maxrss_bytes"] for e in rs if e["arm"]==arm]
            summary[arm+"_rss_median"]=int(median(rr));summary[arm+"_rss_max"]=max(rr)
        summaries.append(summary)
    write(a.out/"block-allocations.csv",blocks);write(a.out/"process-rss.csv",raw);write(a.out/"summary.csv",summaries)
    report=dict(pinned_main="a1d39559686a9ec17c8614b9154a64d7b5b98672",native_build="Apple M5, macOS 27.2, Apple clang 21",
                stock_sha256=digest(a.stock.read_bytes()),probe_sha256=digest(a.probe.read_bytes()),cases=len(freeze),blocks=len(blocks),
                measured_restores=len(raw),frozen_cases=sum(e["source"]=="frozen" for e in freeze),
                archived_cases=sum(e["source"]=="R30" for e in freeze),declared_codec_cap_bytes=512*2**20,
                instrumentation_owned_ceiling_bytes=128*2**20,raw_frame_peak_bytes=max(i["peak_owned"] for i in blocks))
    (a.out/"verified.json").write_text(json.dumps(report,indent=2)+"\n");print(report)
if __name__=="__main__": main()
