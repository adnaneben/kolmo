#!/usr/bin/env python3
"""w4 documents benchmark: size, MB/s and peak RSS of Kolmo levels and their rivals, per file.
usage: run.py OUT.jsonl KOLMO FILE... [--threads N] [--tools a,b] [--rounds R]
Each tool is a whole process; wall time from wait4, fastest of up to R rounds (20 s cap), peak RSS from GNU time.
Every decoded output is compared with the input."""
import os, sys, json, time, hashlib, tempfile, argparse

def tools(kolmo, T):
    t = str(T)
    return {
        'lz4 -1': (['lz4', '-1', '-q', '-f', '{i}', '{o}'], ['lz4', '-d', '-q', '-f', '{o}', '{r}']),
        'zstd -3': (['zstd', '-3', '-q', '-f', f'-T{t}' if T > 1 else '--single-thread', '{i}', '-o', '{o}'], ['zstd', '-d', '-q', '-f', '{o}', '-o', '{r}']),
        'zstd -19 --long=27': (['zstd', '-19', '--long=27', '-q', '-f', f'-T{t}' if T > 1 else '--single-thread', '{i}', '-o', '{o}'], ['zstd', '-d', '--long=27', '-q', '-f', '{o}', '-o', '{r}']),
        'brotli -11': (['brotli', '-q', '11', '-f', '{i}', '-o', '{o}'], ['brotli', '-d', '-f', '{o}', '-o', '{r}']),
        **{f'kolmo {m}': ([kolmo, '-m', m, '-T', t, '{i}', '{o}'], [kolmo, '-d', '-T', t, '{o}', '{r}'])
           for m in ('speed', 'balanced', 'eco', 'hc')},
    }

RSSF = os.path.join(tempfile.gettempdir(), f'w4rss.{os.getpid()}')
def run(cmd):
    # peak RSS from GNU time (a small C parent): a child forked from Python would start from Python's RSS, which
    # Linux keeps in ru_maxrss across exec
    t0 = time.perf_counter(); pid = os.fork()
    if pid == 0:
        fd = os.open(os.devnull, os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2)
        try: os.execvp('/usr/bin/time', ['/usr/bin/time', '-f', '%M', '-o', RSSF] + cmd)
        finally: os._exit(127)
    _, st, ru = os.wait4(pid, 0); dt = time.perf_counter() - t0
    if st: raise RuntimeError(f'{cmd} failed: {st}')
    return dt, int(open(RSSF).read().split()[-1]) * 1024

def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()

ap = argparse.ArgumentParser(); ap.add_argument('out'); ap.add_argument('kolmo'); ap.add_argument('files', nargs='+')
ap.add_argument('--threads', type=int, default=1); ap.add_argument('--tools', default=''); ap.add_argument('--rounds', type=int, default=3)
a = ap.parse_args()
TL = tools(a.kolmo, a.threads); sel = [x for x in a.tools.split(',') if x] or list(TL)
tmp = tempfile.mkdtemp(dir=os.environ.get('W4TMP'))
with open(a.out, 'a') as out:
    for f in a.files:
        n = os.path.getsize(f); ref = sha(f)
        for lab in sel:
            c, d = TL[lab]; o = os.path.join(tmp, 'c'); r = os.path.join(tmp, 'r')
            sub = lambda cmd: [x.format(i=f, o=o, r=r) for x in cmd]
            best_c = best_d = 1e9; crss = drss = 0; t_end = time.time() + 20
            for k in range(a.rounds):
                dt, rss = run(sub(c)); best_c = min(best_c, dt); crss = max(crss, rss)
                dt, rss = run(sub(d)); best_d = min(best_d, dt); drss = max(drss, rss)
                if k == 0 and sha(r) != ref: raise SystemExit(f'MISMATCH {lab} {f}')
                if time.time() > t_end: break
            row = dict(file=os.path.basename(f), tool=lab, threads=a.threads, input=n, compressed=os.path.getsize(o),
                       c_mbs=n / best_c / 1e6, d_mbs=n / best_d / 1e6, c_rss=crss, d_rss=drss)
            out.write(json.dumps(row) + '\n'); out.flush()
            print(f"{row['file'][:24]:24} {lab:20} {row['compressed']:>11} {row['c_mbs']:8.1f} {row['d_mbs']:8.1f} {crss>>20:6}M {drss>>20:5}M", flush=True)
            for p in (o, r):
                if os.path.exists(p): os.remove(p)
