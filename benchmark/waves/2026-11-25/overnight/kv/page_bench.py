"""Inspect a fresh public slot export from llama.cpp 39febd2ad and benchmark exact KV page bytes."""
import argparse
import hashlib
import json
import random
import struct
import subprocess
import tempfile
import time
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument("state",type=Path)
p.add_argument("kolmo")
p.add_argument("zstd")
p.add_argument("out",type=Path)
args=p.parse_args()
data=args.state.read_bytes()
offset=0
def get(fmt):
    global offset
    values=struct.unpack_from("<"+fmt,data,offset)
    offset+=struct.calcsize("<"+fmt)
    return values
def take(n):
    global offset
    part=data[offset:offset+n]
    assert len(part)==n
    offset+=n
    return part
def sha(x):return hashlib.sha256(x).hexdigest()
magic,version,ntok=get("III")
ids=list(get("i"*ntok))
streams,cells=get("II")
assert streams == 1 and ids[:3] == [-1, 1, cells] and ntok == cells + 4
assert len(ids[3:-1]) == cells and ids[-1] == 0
for i in range(cells):
    pos,nseq,seq=get("iii")
    assert pos==i and nseq==1 and seq==0
vtrans,nlayer,krot,vrot=get("IIII")
assert vtrans in (0,1) and nlayer>0
tensors=[]
for kind in ["K","V"]:
    for layer in range(nlayer):
        if kind=="K" or not vtrans:
            typ,row=get("iQ")
            dim=row//2
            assert typ==1 and row==2*dim
            tensor=take(cells*row)
        else:
            typ,elsize,dim=get("iII")
            assert typ==1 and elsize==2
            col_bytes=take(cells*dim*2)
            tensor=b"".join(col_bytes[2*(j*cells+i):2*(j*cells+i+1)] for i in range(cells) for j in range(dim))
        tensors.append((kind,layer,dim,tensor))
assert offset==len(data)
work=args.out
meta=dict(state_bytes=len(data),state_sha256=sha(data),magic=magic,version=version,
          tokens=ids[3:-1],cells=cells,layers=nlayer,v_transposed_on_disk=vtrans,
          krot=krot,vrot=vrot,tensors=[dict(kind=k,layer=l,shape=[cells,d],dtype="f16-le",sha256=sha(t)) for k,l,d,t in tensors])
(work/"shape.json").write_text(json.dumps(meta,indent=2)+"\n")
rng=random.Random(27)
with tempfile.TemporaryDirectory() as tmp,(work/"observations.jsonl").open("x") as results:
    tmp=Path(tmp)
    src=tmp/"page";frame=tmp/"frame";back=tmp/"back"
    for kind,layer,dim,tensor in tensors:
        if layer not in (0,nlayer//2,nlayer-1):continue
        for page_tokens in (8,16,cells):
            page=tensor[:page_tokens*dim*2]
            src.write_bytes(page)
            for rep in range(4):
                arms=[("kolmo-L1",1),("kolmo-L3",3),("zstd-1",1),("zstd-3",3)]
                rng.shuffle(arms)
                for arm,level in arms:
                    if arm.startswith("kolmo"):
                        enc=[args.kolmo,f"-{level}","-T1","-f","-o",frame,src]
                        dec=[args.kolmo,"-d","-T1","-f","-o",back,frame]
                    else:
                        enc=[args.zstd,"--single-thread","-q","-f",f"-{level}",src,"-o",frame]
                        dec=[args.zstd,"--single-thread","-q","-d","-f",frame,"-o",back]
                    start=time.perf_counter_ns()
                    subprocess.run(enc,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    middle=time.perf_counter_ns()
                    subprocess.run(dec,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
                    end=time.perf_counter_ns()
                    assert back.read_bytes()==page
                    results.write(json.dumps(dict(kind=kind,layer=layer,dim=dim,tokens=page_tokens,codec=arm,
                        rep=rep,raw_bytes=len(page),frame_bytes=frame.stat().st_size,page_sha256=sha(page),
                        frame_sha256=sha(frame.read_bytes()),readback_sha256=sha(back.read_bytes()),
                        encode_s=(middle-start)/1e9,decode_s=(end-middle)/1e9))+"\n")
                    results.flush()
