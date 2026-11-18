"""Q1, heterogeneous files: the mixed file coded whole, against its four parts (text, binary, CSV, float64) coded
separately and summed (each part gets its own analysis and codec choice: the gain a per-segment choice could reach).
python3 -I segments.py KOLMO MIXED_FILE MODES...   (the parts are cut as prepare cut them: four quarters at line ends)"""
import os, subprocess, sys, tempfile, time
kolmo, path, modes = sys.argv[1], sys.argv[2], sys.argv[3:]
data = open(path, 'rb').read()
q = len(data) // 4  # the mixed file is text, bin, table, num, each about a quarter (text and table cut at a line end)
# recover the cut points: text ends at the last newline before q; bin is exactly q bytes; table ends at a line end
src = '/home/user/data/c/'
def part(k, n):
    d = open(src + k + '_100m', 'rb').read(n)
    return d[:d.rfind(b'\n') + 1] if k in ('text', 'table') else d
tot = len(data) + 22  # placeholder; recomputed below
Q = {'1m': 1_000_000, '10m': 10_000_000, '100m': 100_000_000}[path.rsplit('_', 1)[1]] // 4
parts = [part(k, Q) for k in ('text', 'bin', 'table', 'num')]
assert b''.join(parts) == data, 'parts do not match'
tmp = tempfile.mkdtemp()
def run(args):
    t = time.time(); subprocess.run(args, check=True); return time.time() - t
for m in modes:
    margs = ['-x'] if m == 'x' else ['-m', m]
    whole = os.path.join(tmp, 'w.kol'); tw = run([kolmo, *margs, '-T1', path, whole]); zw = os.path.getsize(whole)
    zs = 0; ts = 0
    for i, p in enumerate(parts):
        fi = os.path.join(tmp, f'p{i}'); open(fi, 'wb').write(p); fo = fi + '.kol'
        ts += run([kolmo, *margs, '-T1', fi, fo]); zs += os.path.getsize(fo) - 32  # one frame header shared
    print(f'{os.path.basename(path)} {m} whole={zw} ({tw:.1f}s) parts={zs} ({ts:.1f}s) gain={100.0 * (zw - zs) / zw:.2f}%', flush=True)
