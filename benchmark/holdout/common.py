"""Process-level measurements on macOS and Linux. Outputs are verified outside timing."""
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/'benchmark/holdout'
WORK = ROOT/'build/holdout'
BIN = WORK/'frozen'
def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1<<20), b''):
            h.update(chunk)
    return h.hexdigest()

def save(path, value):
    tmp=path.with_suffix(path.suffix+'.tmp')
    tmp.write_text(json.dumps(value, indent=2)+'\n')
    tmp.replace(path)

def measure(args):
    if sys.platform != 'darwin' and not sys.platform.startswith('linux'):
        raise RuntimeError('Process RSS measurements require macOS or Linux')
    rss_unit = 1 if sys.platform == 'darwin' else 1024
    with tempfile.TemporaryFile() as log:
        start=time.perf_counter_ns()
        with subprocess.Popen(args, stdout=log, stderr=log) as p:
            _, status, usage=os.wait4(p.pid,0)
            elapsed=(time.perf_counter_ns()-start)/1e9
            p.returncode=os.waitstatus_to_exitcode(status)
            if p.returncode:
                log.seek(0)
                raise RuntimeError(f'{args}: {log.read()[-4000:]}')
    return {'seconds':elapsed,'peak_rss_bytes':int(usage.ru_maxrss) * rss_unit}

def kolmo_enc(exe,mode,src,dst):
    return [BIN/exe,'-m',mode,'-T','1',src,dst]
def kolmo_dec(exe,src,dst):
    return [BIN/exe,'-d','-T','1',src,dst]

def verify_freeze():
    pins=json.loads((OUT/'freeze.json').read_text())['executables']
    for name,sha in pins.items():
        assert digest(BIN/name)==sha,name
