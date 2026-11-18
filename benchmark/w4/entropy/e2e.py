#!/usr/bin/env python3
"""e2e.py: end-to-end A/B of two kolmo builds and the rivals, one thread count per run (never mixed).

usage: e2e.py OUT.jsonl THREADS ROUNDS MODES FILE...   (env: KOLMO_A, KOLMO_B = the two builds; BSC, BZIP3, ZSTD)
For every file and mode: compress once with each build, decode ROUNDS times alternating the order (best wall time
kept), check the output, record sizes, wall times and peak RSS (wait4). Rivals for `ratio`: bsc -m0 -e2 (1 thread:
-t -T) and bzip3 -b 16 (-j THREADS); for the other modes zstd at the mode's usual rival level. One JSON row per
(file, mode, tool)."""
import json, os, sys, time, hashlib

def run(cmd):
    t = time.perf_counter(); pid = os.fork()
    if pid == 0:
        fd = os.open(os.devnull, os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2); os.execvp(cmd[0], cmd)
    _, st, ru = os.wait4(pid, 0)
    return time.perf_counter() - t, ru.ru_maxrss, os.waitstatus_to_exitcode(st)

def digest(p):
    return hashlib.sha256(open(p, 'rb').read()).hexdigest()

out, T, R, modes, files = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), sys.argv[4].split(','), sys.argv[5:]
A, B = os.environ['KOLMO_A'], os.environ['KOLMO_B']
BSC, BZ3, ZSTD = os.environ.get('BSC'), os.environ.get('BZIP3', 'bzip3'), os.environ.get('ZSTD', 'zstd')
W = os.environ.get('E2E_TMP', '/tmp/e2e'); os.makedirs(W, exist_ok=True)
ZLEV = {'speed': '-1', 'balanced': '-3', 'eco': '-12', 'hc': '-19', 'max': '-19'}
with open(out, 'a') as fo:
    for f in files:
        n = os.path.getsize(f); h = digest(f); name = os.path.basename(f)
        for m in modes:
            tools = [('A', [A, '-m', m, '-T', str(T), f, W + '/a.k'], [A, '-d', '-T', str(T), W + '/a.k', W + '/a.d'], W + '/a.k', W + '/a.d'),
                     ('B', [B, '-m', m, '-T', str(T), f, W + '/b.k'], [B, '-d', '-T', str(T), W + '/b.k', W + '/b.d'], W + '/b.k', W + '/b.d')]
            if m == 'ratio' and BSC:
                o = ['-t', '-T'] if T == 1 else []
                tools.append(('bsc -m0 -e2', [BSC, 'e', f, W + '/c.bsc', '-m0', '-e2'] + o, [BSC, 'd', W + '/c.bsc', W + '/c.d'] + o, W + '/c.bsc', W + '/c.d'))
            if m == 'ratio':
                tools.append(('bzip3 -b 16', [BZ3, '-e', '-b', '16', '-j', str(T), '-f', '-c', f], None, None, None))
            if m in ZLEV:
                tools.append(('zstd ' + ZLEV[m], [ZSTD, '-q', '-f', ZLEV[m], '-T%d' % T, f, '-o', W + '/z.zst'], [ZSTD, '-q', '-f', '-d', '-T%d' % T, W + '/z.zst', '-o', W + '/z.d'], W + '/z.zst', W + '/z.d'))
            done = []
            for name_t, c, d, zf, df in tools:
                if c[0] == BZ3:   # bzip3 writes to stdout: compress and decompress through files
                    c = ['sh', '-c', '%s -e -b 16 -j %d -f -c %s > %s/y.bz3' % (BZ3, T, f, W)]
                    d = ['sh', '-c', '%s -d -j %d -f -c %s/y.bz3 > %s/y.d' % (BZ3, T, W, W)]; zf, df = W + '/y.bz3', W + '/y.d'
                tc, cmem, rc = run(c)
                if rc != 0: print('compress failed', name_t, f, m, file=sys.stderr); continue
                done.append(dict(t=name_t, d=d, zf=zf, df=df, tc=tc, cmem=cmem, best=1e9, dmem=0, ok=True))
            for r in range(R):                       # decoders alternate, the order flips every round
                for e in (done if r % 2 == 0 else done[::-1]):
                    td, mem, rd = run(e['d'])
                    if rd != 0: e['ok'] = False; continue
                    e['best'] = min(e['best'], td); e['dmem'] = max(e['dmem'], mem)
            for e in done:
                ok = e['ok'] and digest(e['df']) == h
                row = dict(file=name, mode=m, tool=e['t'], threads=T, raw=n, size=os.path.getsize(e['zf']), c_s=round(e['tc'], 4),
                           d_s=round(e['best'], 4), c_rss_kb=e['cmem'], d_rss_kb=e['dmem'], ok=ok)
                fo.write(json.dumps(row) + '\n'); fo.flush()
                print(json.dumps(row))
