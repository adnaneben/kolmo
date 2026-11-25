#!/usr/bin/env python3
"""Regression checks for the optional Ultra record cuts. Use the experiment binary and an independent stock reader."""
import hashlib
import json
import os
import random
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

encoder, stock=map(lambda s: Path(s).resolve(), sys.argv[1:3])
limit=1 << 20
def sha(b):
    return hashlib.sha256(b).hexdigest()

def run(data, root, case, level, change, boundaries=None):
    source=root/"source"; archive=root/"archive"; restored=root/"restored"
    source.write_bytes(data)
    env=dict(os.environ);env.pop("KOLMO_BENCH_READ_CUTS",None)
    results=[]
    for tool,settings in [(stock,env),(encoder,env),(encoder,{**env,"KOLMO_BENCH_READ_CUTS":"1"})]:
        subprocess.run([tool,"-q","-T1",level,"--block-size","1","-o",archive,source],env=settings,check=True)
        frame=archive.read_bytes()
        subprocess.run([stock,"-q","-T1","-d","-o",restored,archive],env=env,check=True)
        assert sha(restored.read_bytes())==sha(data)
        count=struct.unpack_from("<I",frame,20)[0]
        cuts=[0]
        for i in range(count):
            cuts.append(cuts[-1]+struct.unpack_from("<I",frame,32+i*16)[0])
        assert cuts[-1]==len(data)
        results.append((sha(frame),cuts,len(frame)))
        archive.unlink();restored.unlink()
    assert results[0]==results[1]
    assert (results[1][0]!=results[2][0])==change, (case,results)
    if change: assert all(c in boundaries for c in results[2][1])
    print(json.dumps(dict(case=case,level=level,raw=len(data),input_sha256=sha(data),
                          stock=results[0],opt_in=results[2],restore="exact")))

with tempfile.TemporaryDirectory() as td:
    root=Path(td);data=bytearray(); boundaries={0}; index=0; rand=random.Random(825)
    while len(data)<limit+(128<<10):
        width=[83,129,247][index%3]
        name=("@synthetic-%d"%index).encode()
        plus=(b"+"+name[1:]) if index%2 else b"+"
        # Opaque punctuation is sufficient to test four-line record framing.
        values = bytes(rand.choices(b"?!~#%&*.)(", k=width))
        aux = bytes(rand.choices(range(33, 93), k=width))
        data+=b"\n".join([name,values,plus,aux])+b"\n"
        boundaries.add(len(data)); index+=1
    block=bytes(data)
    run(block,root,"variable-length","-U",True,boundaries)
    run(block,root,"other-level","-3",False)
    run(block+b"plain trailing text\n",root,"mixed-tail","-U",False)
    run(block+b"@test\n???\n+wrong\n!!!\n",root,"mismatched-header","-U",False)
    run(block+b"@test\n???\n+\n!!\n",root,"mismatched-field-length","-U",False)
    run(block+b"@test\n???\n+\n!!!",root,"unterminated","-U",False)
    wide=limit//4+1
    run(block+b"@long\n"+b"?"*wide+b"\n+\n"+b"!"*wide+b"\n",root,"oversized-record","-U",False)
    run((b"plain prose material 0123456789\n"*(limit//20)),root,"ordinary-text","-U",False)
