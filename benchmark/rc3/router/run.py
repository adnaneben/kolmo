#!/usr/bin/env python3
"""Routing matrix: every corpus file through every Kolmo mode and the general and specialist leaders.
One process per compression and decompression (wall time, best of `--rounds`), every output decoded and compared.
Jobs run `--jobs` at a time, each tool on one thread (-T 1, zstd/xz single threaded), so the times are per core
under a full machine. Appends JSON lines to OUT; rows already in OUT are skipped (resumable).
usage: python3 -I run.py CORPUS OUT [--kolmo BIN] [--jobs 4] [--only kolmo|leaders] [--types a,b] [--modes ...]"""
import argparse, concurrent.futures as cf, hashlib, json, os, shutil, subprocess, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from frameinfo import info  # noqa: E402

ap = argparse.ArgumentParser()
ap.add_argument('corpus'); ap.add_argument('out')
ap.add_argument('--kolmo', default='build/kolmo'); ap.add_argument('--tag', default='kolmo')
ap.add_argument('--jobs', type=int, default=4); ap.add_argument('--rounds', type=int, default=1)
ap.add_argument('--only', default=''); ap.add_argument('--types', default='')
ap.add_argument('--modes', default='speed,balanced,eco,hc,ratio,x'); ap.add_argument('--threads', default='1')
A = ap.parse_args()
KOLMO = os.path.abspath(A.kolmo)
MODES = {'speed': ['-m', 'speed'], 'balanced': ['-m', 'balanced'], 'eco': ['-m', 'eco'], 'hc': ['-m', 'hc'],
         'ratio': ['-m', 'ratio'], 'x': ['-x']}


def wall(cmd, stdin=None, stdout=None):
    t = time.perf_counter()
    r = subprocess.run(cmd, stdin=stdin, stdout=stdout or subprocess.DEVNULL, stderr=subprocess.PIPE)
    return time.perf_counter() - t, r.returncode, r.stderr.decode(errors='replace')[-300:]


def sha(p):
    h = hashlib.sha256()
    with open(p, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''): h.update(b)
    return h.hexdigest()


def pipe_tool(c, d):   # tools that read stdin and write stdout
    def run(src, tmp):
        z = os.path.join(tmp, 'z'); o = os.path.join(tmp, 'o')
        with open(src, 'rb') as i, open(z, 'wb') as f: tc, rc, e = wall(c, i, f)
        if rc: return None, e
        with open(z, 'rb') as i, open(o, 'wb') as f: td, rc, e = wall(d, i, f)
        return (os.path.getsize(z), tc, td, o), e
    return run


def kolmo_tool(mode):
    def run(src, tmp):
        z = os.path.join(tmp, 'z'); o = os.path.join(tmp, 'o')
        tc, rc, e = wall([KOLMO] + MODES[mode] + ['-T', A.threads, src, z])
        if rc: return None, e
        td, rc, e = wall([KOLMO, '-d', '-T', A.threads, z, o])
        if rc: return None, e
        agg, meta = info(z); tot = sum(agg.values()) or 1
        route = ' '.join('%s:%.0f%%' % (k, 100 * v / tot) for k, v in agg.most_common())
        return (os.path.getsize(z), tc, td, o, 'v%d %s' % (meta[0], route)), e
    return run


def flac_tool(src, tmp):
    z = os.path.join(tmp, 'z.flac'); o = os.path.join(tmp, 'o.wav')
    tc, rc, e = wall(['flac', '-8', '-s', '-f', '--no-seektable', '-o', z, src])
    if rc: return None, e
    td, rc, e = wall(['flac', '-d', '-s', '-f', '-o', o, z])
    return (os.path.getsize(z), tc, td, o), e


def ffv1_tool(src, tmp):
    z = os.path.join(tmp, 'z.mkv'); o = os.path.join(tmp, 'o.y4m')
    tc, rc, e = wall(['ffmpeg', '-v', 'error', '-threads', '1', '-i', src, '-c:v', 'ffv1', '-level', '3', '-threads', '1',
                      '-slices', '4', '-g', '1', '-context', '1', '-y', z])
    if rc: return None, e
    td, rc, e = wall(['ffmpeg', '-v', 'error', '-threads', '1', '-i', z, '-f', 'rawvideo', '-y', o])
    if rc: return None, e
    # the Y4M header and frame markers are rewritten by ffmpeg: compare the decoded frames with the source's frames
    r = os.path.join(tmp, 'r.yuv'); wall(['ffmpeg', '-v', 'error', '-i', src, '-f', 'rawvideo', '-y', r])
    return (os.path.getsize(z), tc, td, None if sha(o) == sha(r) else 'mismatch'), e


def img_tool(codec):   # imagecodecs in a subprocess (python3 -I), PPM/PGM input, compare pixels
    code = r'''
import sys, time, io, numpy as np, imagecodecs as ic
from PIL import Image
a = np.asarray(Image.open(sys.argv[1])); c = sys.argv[2]
enc = {'jxl': lambda x: ic.jpegxl_encode(x, lossless=True, effort=9), 'jls': lambda x: ic.jpegls_encode(x, level=0),
       'webp': lambda x: ic.webp_encode(x, lossless=True, level=100)}[c]
dec = {'jxl': ic.jpegxl_decode, 'jls': ic.jpegls_decode, 'webp': ic.webp_decode}[c]
t = time.perf_counter(); z = enc(a); tc = time.perf_counter() - t
t = time.perf_counter(); b = dec(z); td = time.perf_counter() - t
print(len(z), tc, td, int(np.array_equal(a, b)))
'''
    def run(src, tmp):
        if not src.endswith(('.ppm', '.pgm')): return None, 'not an image'
        r = subprocess.run([sys.executable, '-I', '-c', code, src, codec], capture_output=True, text=True)
        if r.returncode: return None, r.stderr[-300:]
        n, tc, td, ok = r.stdout.split()
        return (int(n), float(tc), float(td), None if ok == '1' else 'mismatch'), ''
    return run


LEADERS = {
    'zstd-3': pipe_tool(['zstd', '-3', '-T1', '-c'], ['zstd', '-d', '-c']),
    'zstd-19': pipe_tool(['zstd', '-19', '-T1', '-c'], ['zstd', '-d', '-c']),
    'zstd-22': pipe_tool(['zstd', '--ultra', '-22', '--long=27', '-T1', '-c'], ['zstd', '-d', '--long=27', '-c']),
    'xz-9e': pipe_tool(['xz', '-9e', '-T1', '-c'], ['xz', '-d', '-c']),
    'brotli-11': pipe_tool(['brotli', '-q', '11', '-w', '24', '-c'], ['brotli', '-d', '-c']),
    'lz4-1': pipe_tool(['lz4', '-1', '-c'], ['lz4', '-d', '-c']),
    'bzip2-9': pipe_tool(['bzip2', '-9', '-c'], ['bzip2', '-d', '-c']),
}
SPECIAL = {'audio': {'flac-8': flac_tool}, 'video': {'ffv1': ffv1_tool},
           'img8': {'jxl-e9': img_tool('jxl'), 'jpegls': img_tool('jls'), 'webp-ll': img_tool('webp')}}

done = set()
if os.path.exists(A.out):
    for l in open(A.out):
        r = json.loads(l); done.add((r['file'], r['tool']))
jobs = []
types = [t for t in sorted(os.listdir(A.corpus)) if not A.types or t in A.types.split(',')]
for t in types:
    for f in sorted(os.listdir(os.path.join(A.corpus, t))):
        p = os.path.join(A.corpus, t, f)
        tools = {}
        if A.only != 'leaders':
            for m in A.modes.split(','): tools['%s-%s' % (A.tag, m)] = kolmo_tool(m)
        if A.only != 'kolmo':
            tools.update(LEADERS); tools.update(SPECIAL.get(t, {}))
        for name, fn in tools.items():
            if (p, name) not in done: jobs.append((t, p, name, fn))
# slowest first, so the pool stays full
jobs.sort(key=lambda j: -(os.path.getsize(j[1]) * (50 if j[2].endswith('-x') else 1)))
H = {}


def one(job):
    t, p, name, fn = job
    tmp = tempfile.mkdtemp(prefix='rt', dir=os.environ.get('RTMP', '/tmp'))
    try:
        best = None
        for _ in range(A.rounds):
            r, err = fn(p, tmp)
            if r is None: return dict(type=t, file=p, tool=name, error=err)
            if best is None or r[1] < best[1]: best = (r[0], r[1], min(r[2], best[2]) if best else r[2]) + tuple(r[3:])
            else: best = best[:2] + (min(best[2], r[2]),) + best[3:]
        size, tc, td = best[:3]
        out = best[3] if len(best) > 3 else None
        if isinstance(out, str) and os.path.exists(out):
            ok = sha(out) == H.setdefault(p, sha(p))
        else:
            ok = out is None
        row = dict(type=t, file=p, n=os.path.getsize(p), tool=name, size=size, c=round(tc, 4), d=round(td, 4), ok=ok)
        if len(best) > 4: row['route'] = best[4]
        return row
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


with cf.ThreadPoolExecutor(A.jobs) as ex, open(A.out, 'a') as fo:
    for row in ex.map(one, jobs):
        fo.write(json.dumps(row) + '\n'); fo.flush()
        print(row.get('tool'), os.path.basename(row['file']), row.get('size', row.get('error')), row.get('route', ''), flush=True)
