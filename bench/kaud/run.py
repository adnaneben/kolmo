#!/usr/bin/env python3
"""Whole-process benchmark for build/kaud and its rivals (see src/experimental/KAUD.md).

For each tool and data set: every file is encoded, then decoded, by separate processes run one after
another; wall time and CPU time (user+sys of the child, from wait4) are summed over the set; the set
is run R times and the fastest total is kept. Every decoded file is checked against the original
(sample bytes for all tools; whole file for tools that promise a byte-exact WAV).

usage: run.py OUTDIR SET=DIR [SET=DIR ...] [--tools a,b,c] [--runs 3] [--json out.json]
"""
import hashlib, json, os, resource, shutil, struct, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
KAUD = os.path.join(ROOT, 'build', 'kaud')
KOLMO = os.path.join(ROOT, 'build', 'kolmo')


def kaud(args):
    return dict(ext='.kau', exact=True,
                enc=lambda i, o: [KAUD] + args + [i, o],
                dec=lambda i, o: [KAUD, '-d'] + _thr(args) + [i, o])


def _thr(args):
    return ['-T', args[args.index('-T') + 1]] if '-T' in args else []


def kolmo(args):
    return dict(ext='.kol', exact=True,
                enc=lambda i, o: [KOLMO] + args + [i, o],
                dec=lambda i, o: [KOLMO, '-d'] + _thr(args) + [i, o])


TOOLS = {
    'flac-5': dict(ext='.flac', exact=False, enc=lambda i, o: ['flac', '-s', '-f', '-5', '-o', o, i],
                   dec=lambda i, o: ['flac', '-s', '-f', '-d', '-o', o, i]),
    'flac-8': dict(ext='.flac', exact=False, enc=lambda i, o: ['flac', '-s', '-f', '-8', '-o', o, i],
                   dec=lambda i, o: ['flac', '-s', '-f', '-d', '-o', o, i]),
    'flac-8-nometa': dict(ext='.flac', exact=False,
                          enc=lambda i, o: ['flac', '-s', '-f', '-8', '--no-padding', '--no-seektable', '-o', o, i],
                          dec=lambda i, o: ['flac', '-s', '-f', '-d', '-o', o, i]),
    'wavpack-hh': dict(ext='.wv', exact=True, enc=lambda i, o: ['wavpack', '-q', '-y', '-hh', i, '-o', o],
                       dec=lambda i, o: ['wvunpack', '-q', '-y', i, '-o', o]),
    'wavpack-hhx6': dict(ext='.wv', exact=True, enc=lambda i, o: ['wavpack', '-q', '-y', '-hhx6', i, '-o', o],
                         dec=lambda i, o: ['wvunpack', '-q', '-y', i, '-o', o]),
    'kolmo-balanced-typedhuf-T1': kolmo(['-m', 'balanced', '--typed-huf', '-T', '1']),
    'kolmo-ratio-T1': kolmo(['-m', 'ratio', '-T', '1']),
    'kolmo-x-l2-T1': kolmo(['-x', '-l', '2', '-T', '1']),
    'kolmo-ratio-T4': kolmo(['-m', 'ratio', '-T', '4']),
    'kaud-0-T1': kaud(['-0', '-T', '1']),
    'kaud-0-T4': kaud(['-0', '-T', '4']),
    'kaud-1-T1': kaud(['-1', '-T', '1']),
    'kaud-1-T4': kaud(['-1', '-T', '4']),
}


def pcm_bytes(path):
    b = open(path, 'rb').read()
    p = 12
    while p + 8 <= len(b):
        cid, sz = struct.unpack('<4sI', b[p:p + 8])
        if cid == b'data':
            return b[p + 8:p + 8 + sz]
        p += 8 + sz + (sz & 1)
    raise ValueError('no data chunk in ' + path)


def run(cmd):
    t0 = time.perf_counter()
    p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    _, status, ru = os.wait4(p.pid, 0)
    wall = time.perf_counter() - t0
    err = p.stderr.read()
    p.stderr.close()
    if os.waitstatus_to_exitcode(status) != 0:
        raise RuntimeError('%s failed: %s' % (' '.join(cmd), err.decode(errors='replace')))
    return wall, ru.ru_utime + ru.ru_stime


def bench(tool, files, outdir, runs):
    t = TOOLS[tool]
    d = os.path.join(outdir, tool)
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    best = dict(enc_wall=1e30, enc_cpu=1e30, dec_wall=1e30, dec_cpu=1e30)
    sizes = {}
    for r in range(runs):
        tot = dict(enc_wall=0, enc_cpu=0, dec_wall=0, dec_cpu=0)
        for f in files:
            base = os.path.basename(f)
            o = os.path.join(d, base + t['ext'])
            w = os.path.join(d, base + '.dec.wav')
            for q in (o, w):
                if os.path.exists(q):
                    os.remove(q)
            a, b = run(t['enc'](f, o))
            tot['enc_wall'] += a; tot['enc_cpu'] += b
            a, b = run(t['dec'](o, w))
            tot['dec_wall'] += a; tot['dec_cpu'] += b
            sizes[base] = os.path.getsize(o)
            if r == 0:
                if t['exact']:
                    ok = open(f, 'rb').read() == open(w, 'rb').read()
                else:
                    ok = pcm_bytes(f) == pcm_bytes(w)
                if not ok:
                    raise RuntimeError('%s: %s did not round trip' % (tool, base))
            os.remove(w)
        for k in best:
            best[k] = min(best[k], tot[k])
    raw = sum(os.path.getsize(f) for f in files)
    res = dict(tool=tool, files=len(files), raw=raw, size=sum(sizes.values()), per_file=sizes, **best)
    res['enc_MBps_wall'] = raw / best['enc_wall'] / 1e6
    res['dec_MBps_wall'] = raw / best['dec_wall'] / 1e6
    res['enc_MBps_cpu'] = raw / best['enc_cpu'] / 1e6
    res['dec_MBps_cpu'] = raw / best['dec_cpu'] / 1e6
    return res


def main():
    a = sys.argv[1:]
    outdir = a.pop(0)
    sets, tools, runs, jout = {}, list(TOOLS), 3, None
    while a:
        x = a.pop(0)
        if x == '--tools':
            tools = a.pop(0).split(',')
        elif x == '--runs':
            runs = int(a.pop(0))
        elif x == '--json':
            jout = a.pop(0)
        else:
            k, v = x.split('=', 1)
            sets[k] = sorted(os.path.join(v, f) for f in os.listdir(v) if f.endswith('.wav'))
    allres = json.load(open(jout)) if jout and os.path.exists(jout) else {}
    for s, files in sets.items():
        for tool in tools:
            r = bench(tool, files, os.path.join(outdir, s), runs)
            allres.setdefault(s, {})[tool] = r
            print('%-6s %-28s size %11d  ratio %.4f  enc %7.2f MB/s (cpu %7.2f)  dec %7.2f MB/s (cpu %7.2f)' % (
                s, tool, r['size'], r['size'] / r['raw'], r['enc_MBps_wall'], r['enc_MBps_cpu'],
                r['dec_MBps_wall'], r['dec_MBps_cpu']), flush=True)
            if jout:
                json.dump(allres, open(jout, 'w'), indent=1, sort_keys=True)


if __name__ == '__main__':
    main()
