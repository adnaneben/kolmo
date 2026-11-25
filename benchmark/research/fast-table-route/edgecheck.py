"""Verify transitions out of the two sampled windows with the stock reader."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
import tempfile

base,cand,source = map(Path,sys.argv[1:])
with tempfile.TemporaryDirectory() as temp:
    work = Path(temp)
    orig = source.read_bytes()[:2<<20]
    variants = {"threshold-1": orig[:(512<<10)-1], "threshold": orig[:512<<10],
        "ragged tail": orig+((b'1,ab,"split\nline",5\r\n'*1000)+b'no final newline'),
        "binary tail": orig+(bytes(range(256))*37),
        "changed delimiter": orig+orig[-(64<<10):].replace(b",",b";"),
        "long field": orig+b'\n'+b'a'*(65<<10)+b',end\n',
        "empty": b"", "no newlines": b','*(512<<10)}
    for name,data in variants.items():
        inp, frame, out = [work / k for k in ("in","frame","out")]
        inp.write_bytes(data)
        for threads in (1,4):
            for level in (1,2,3):
                run = lambda cmd: subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                hashes = []
                for binary in (base,cand):
                    run([binary,f'-{level}','-T',str(threads),'-f','-o',frame,inp])
                    hashes.append(hashlib.sha256(frame.read_bytes()).hexdigest())
                    run([base,'-d','-T',str(threads),'-f','-o',out,frame])
                    assert out.read_bytes() == data
                if level==3 or len(data)<(512<<10):
                    assert hashes[0] == hashes[1]
                print(json.dumps(dict(case=name,threads=threads,level=level,n=len(data),
                    input_sha256=hashlib.sha256(data).hexdigest(),base_sha256=hashes[0],
                    candidate_sha256=hashes[1],exact=True)))
