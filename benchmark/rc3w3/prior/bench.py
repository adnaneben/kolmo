"""End-to-end timing of the lane (one process at a time): python3 -I bench.py THREADS OUT.jsonl NAME=BIN... -- FILES...
For each file and build: `BIN -x -T THREADS` then `BIN -d`, wall time and peak RSS from wait4, the decoded file compared
byte for byte. External coders (zstd, xz, brotli, zpaq) when their name is given as a build: NAME=zstd etc."""
import json, os, subprocess, sys, tempfile, time

def run(cmd):
    t = time.perf_counter(); p = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    _, st, ru = os.wait4(p.pid, 0); dt = time.perf_counter() - t
    if st: raise RuntimeError(f'{cmd} failed ({st})')
    return dt, ru.ru_maxrss * 1024

EXT = {
    'zstd': (lambda i, o, T: ['zstd', '-q', '-f', '--ultra', '-22', f'-T{T}', '--long=27', i, '-o', o], lambda i, o, T: ['zstd', '-q', '-f', '-d', '--long=27', i, '-o', o]),
    'xz': (lambda i, o, T: ['sh', '-c', f'xz -9e -T{T} -c "{i}" > "{o}"'], lambda i, o, T: ['sh', '-c', f'xz -d -T{T} -c "{i}" > "{o}"']),
    'brotli': (lambda i, o, T: ['brotli', '-q', '11', '-w', '24', '-f', i, '-o', o], lambda i, o, T: ['brotli', '-d', '-f', i, '-o', o]),
    'zpaq': (lambda i, o, T: ['sh', '-c', f'cd "{os.path.dirname(i)}" && zpaq a "{o}.zpaq" "{os.path.basename(i)}" -m5 -t{T} && mv "{o}.zpaq" "{o}"'], None),
}

def main():
    T = int(sys.argv[1]); out = open(sys.argv[2], 'a'); a = sys.argv[3:]; k = a.index('--')
    builds = [x.split('=', 1) for x in a[:k]]; files = a[k + 1:]
    with tempfile.TemporaryDirectory() as d:
        for f in files:
            for name, b in builds:
                z, r = os.path.join(d, 'z'), os.path.join(d, 'r')
                for p in (z, r):
                    if os.path.exists(p): os.remove(p)
                if b in EXT:
                    enc, dec = EXT[b]
                    te, me = run(enc(f, z, T))
                    if dec: td, md = run(dec(z, r, T))
                    else:   # zpaq: the archive needs its extension; the member is renamed to r
                        os.replace(z, z + '.zpaq')
                        td, md = run(['zpaq', 'x', z + '.zpaq', os.path.basename(f), '-to', r, f'-t{T}'])
                        os.replace(z + '.zpaq', z)
                else:
                    te, me = run([b, '-x', f'-T{T}', f, z]); td, md = run([b, '-d', f'-T{T}', z, r])
                ok = os.path.isfile(r) and open(r, 'rb').read() == open(f, 'rb').read()
                rec = dict(file=os.path.basename(f), build=name, threads=T, raw=os.path.getsize(f), size=os.path.getsize(z),
                           enc=round(te, 4), dec=round(td, 4), enc_rss=me, dec_rss=md, ok=ok)
                out.write(json.dumps(rec) + '\n'); out.flush()
                print(rec['file'], name, T, rec['size'], rec['enc'], rec['dec'], 'ok' if ok else 'MISMATCH', flush=True)

main()
