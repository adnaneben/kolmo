"""Compare paired decode passes over one valid, densely blocked CZNS member."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import tempfile
import time

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda:source.read(1<<20), b''):
            h.update(chunk)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--before', type=Path, required=True)
    p.add_argument('--after', type=Path, required=True)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--frame', type=Path, required=True)
    p.add_argument('--repeats', type=int, default=7)
    p.add_argument('--limits', nargs='+', default=['2M','max'])
    a=p.parse_args()
    sha=digest(a.source)
    n=a.source.stat().st_size
    frame=digest(a.frame)
    rng=random.Random(662007)
    bins=[(name,path.resolve(),digest(path)) for name,path in [('before',a.before),('reuse',a.after)]]
    with tempfile.TemporaryDirectory() as tmp:
        output=Path(tmp)/'restored'
        for limit in a.limits:
            for repeat in range(-1,a.repeats):
                for mode, executable, build in rng.sample(bins,len(bins)):
                    output.unlink(missing_ok=True)
                    argv=[str(executable),'-d','-M',limit,'-T','1','-q','-f',str(a.frame),'-o',str(output)]
                    start=time.perf_counter()
                    pid=os.posix_spawn(str(executable),argv,os.environ)
                    _,status,usage=os.wait4(pid,0)
                    duration=time.perf_counter()-start
                    if status or digest(output)!=sha:
                        raise RuntimeError(f'failed on {mode}/{limit}')
                    if repeat>=0:
                        print(json.dumps(dict(mode=mode, limit=limit, repeat=repeat, raw_bytes=n,
                                              frame_bytes=a.frame.stat().st_size, raw_sha256=sha,
                                              frame_sha256=frame, binary_sha256=build,wall_s=duration,
                                              cpu_s=usage.ru_utime+usage.ru_stime,
                                              rss_bytes=usage.ru_maxrss*(1024 if os.uname().sysname=='Linux' else 1))),flush=True)
if __name__=='__main__':
    main()
