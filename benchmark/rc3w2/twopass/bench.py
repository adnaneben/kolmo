"""End-to-end runs of the twopass lane: one whole process at a time, wall time and peak resident memory from wait4,
encode and decode separately, decoded output compared byte for byte.
python3 -I bench.py OUT.csv BIN_LABEL=PATH ... -- MODE:THREADS ... -- FILES...
Rows already in OUT.csv (same binary, mode, threads, file) are skipped, so the run can be resumed."""
import csv, os, resource, subprocess, sys, tempfile, time
args = sys.argv[1:]; out = args.pop(0)
a = args.index('--'); bins = dict(x.split('=', 1) for x in args[:a]); rest = args[a + 1:]
b = rest.index('--'); runs = rest[:b]; files = rest[b + 1:]
done = set()
if os.path.exists(out):
    for r in csv.DictReader(open(out)): done.add((r['bin'], r['mode'], r['threads'], r['file']))
new = not os.path.exists(out)
w = csv.writer(open(out, 'a', newline=''))
if new: w.writerow(['bin', 'mode', 'threads', 'file', 'n', 'size', 'enc_s', 'dec_s', 'enc_mib', 'dec_mib', 'version'])
tmp = tempfile.mkdtemp(dir=os.environ.get('TMPDIR'))
def run(cmd):
    t = time.time(); p = subprocess.Popen(cmd); _, st, ru = os.wait4(p.pid, 0); el = time.time() - t
    if st: raise SystemExit(f'failed: {cmd}')
    return el, ru.ru_maxrss / 1024
for f in files:
    for rm in runs:
        m, th = rm.split(':')
        for lab, exe in bins.items():
            if (lab, m, th, f) in done: continue
            margs = ['-x'] if m == 'x' else ['-m', m]
            z = os.path.join(tmp, 'z.kol'); d = os.path.join(tmp, 'd.out')
            te, me = run([exe, *margs, '-T', th, f, z])
            td, md = run([exe, '-d', '-T', th, z, d])
            ok = open(d, 'rb').read() == open(f, 'rb').read()
            if not ok: raise SystemExit(f'ROUND TRIP FAILED {lab} {m} {f}')
            ver = open(z, 'rb').read(5)[4]
            w.writerow([lab, m, th, os.path.basename(f), os.path.getsize(f), os.path.getsize(z), f'{te:.3f}', f'{td:.3f}', f'{me:.0f}', f'{md:.0f}', ver])
            sys.stdout.flush(); os.remove(z); os.remove(d)
