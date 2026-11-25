"""Inspect stock-generated frames with the T1-only diagnostic decoder."""
from pathlib import Path
import csv, hashlib, re, subprocess, sys, tempfile
from collections import defaultdict
root = Path(__file__).resolve().parent
paths = {Path(p).name:Path(p) for p in sys.argv[3:]}
seen = set()
writer = csv.writer(sys.stdout)
writer.writerow('source level frame_sha256 match_fill_count fill_bytes input_bytes'.split())
with tempfile.TemporaryDirectory() as tmp:
    src, frame, out = (Path(tmp)/p for p in ('data','frame','out'))
    for r in csv.DictReader((root/'raw.csv').open()):
        key=r['name'], r['level']
        if key in seen: continue
        seen.add(key)
        n=int(r['bytes'])
        data=(bytes(range(64))*512+b'\x00'*(64<<10))*8 if key[0]=='synthetic-run' else paths[key[0]].read_bytes()[:n]
        assert hashlib.sha256(data).hexdigest()==r['input_sha256']
        src.write_bytes(data)
        subprocess.run([sys.argv[1],'-'+key[1],'-T1','-f','-o',str(frame),str(src)],check=True)
        assert hashlib.sha256(frame.read_bytes()).hexdigest()==r['frame_sha256']
        p=subprocess.run([sys.argv[2],'-d','-T1','-f','-o',str(out),str(frame)],check=True,stderr=subprocess.PIPE,text=True)
        count, amount = re.search(r'fills=(\d+) bytes=(\d+)',p.stderr).groups()
        assert out.read_bytes()==data
        writer.writerow([*key,r['frame_sha256'],count,amount,len(data)])
