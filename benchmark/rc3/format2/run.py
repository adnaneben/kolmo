# usage: run.py BIN OUT.csv MODES FILES... ; runs each (file,mode) with -T1, 4 at a time; verifies round trip
import sys, os, subprocess, time, tempfile, concurrent.futures as cf, hashlib
binp, out, modes = sys.argv[1], sys.argv[2], sys.argv[3].split(',')
files = sys.argv[4:]
tmp = tempfile.mkdtemp(dir=os.path.dirname(out))
def one(fm):
    f, m = fm; base = os.path.join(tmp, os.path.basename(f) + m.replace(' ', '_'))
    args = [binp] + m.split() + ['-T1', f, base + '.k']
    t = time.time(); r = subprocess.run(args, capture_output=True); te = time.time() - t
    if r.returncode: return (f, m, -1, te, 0, 'encfail ' + r.stderr.decode()[:100])
    t = time.time(); r2 = subprocess.run([binp, '-d', '-T1', base + '.k', base + '.d'], capture_output=True); td = time.time() - t
    ok = r2.returncode == 0 and open(f, 'rb').read() == open(base + '.d', 'rb').read()
    sz = os.path.getsize(base + '.k'); os.remove(base + '.d'); os.remove(base + '.k')
    return (f, m, sz, te, td, 'ok' if ok else 'BAD')
jobs = [(f, m) for f in files for m in modes]
jobs.sort(key=lambda j: -os.path.getsize(j[0]))
with open(out, 'a') as o, cf.ThreadPoolExecutor(int(os.environ.get('P', '4'))) as ex:
    for r in ex.map(one, jobs):
        o.write('%s,%s,%d,%.3f,%.3f,%s\n' % (os.path.basename(r[0]), r[1], r[2], r[3], r[4], r[5])); o.flush()
