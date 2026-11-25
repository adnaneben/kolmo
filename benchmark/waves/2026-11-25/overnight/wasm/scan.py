"""Inventory and verify options on every distinct installed core WebAssembly module."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("reference")
p.add_argument("guard")
p.add_argument("reader")
p.add_argument("out", type=Path)
p.add_argument("roots", type=Path, nargs="+")
args = p.parse_args()
seen = set()
with tempfile.TemporaryDirectory() as tmp, args.out.open("x") as out:
    frame = Path(tmp) / "frame"
    readback = Path(tmp) / "readback"
    for root in args.roots:
        for src in sorted(root.rglob("*.wasm")):
            b = src.read_bytes()
            sha = hashlib.sha256(b).hexdigest()
            if b[:8] != b"\0asm\1\0\0\0" or sha in seen:
                continue
            seen.add(sha)
            for level in [1, 3]:
                for tag, binary, flag in [("main",args.reference,[]),("guard",args.guard,["--wasm-check-shuffle"])]:
                    subprocess.run([binary,f"-{level}","-T1",*flag,"-f","-o",frame,src],check=True)
                    subprocess.run([args.reader,"-d","-T1","-f","-o",readback,frame],check=True)
                    archive = frame.read_bytes()
                    restored = hashlib.sha256(readback.read_bytes()).hexdigest()
                    assert sha == restored
                    nblocks = int.from_bytes(archive[20:24],"little")
                    offset = 32 + 16*nblocks
                    plans = []
                    for i in range(nblocks):
                        index = archive[32+16*i:48+16*i]
                        plans.append([index[12], index[13], archive[offset:offset+4].hex()])
                        offset += int.from_bytes(index[4:8],"little")
                    row = dict(file=str(src), input_bytes=len(b), sha256=sha, level=level,
                               option=tag, frame_bytes=len(archive), frame_sha256=hashlib.sha256(archive).hexdigest(),
                               restored_sha256=restored, plans=plans)
                    out.write(json.dumps(row)+"\n")
                    out.flush()
