# xrun.py OUT T FILES : -x compress with kolmo-base and kolmo-g6 (alternating), sizes, sha of frames, one decode check
import sys, os, subprocess, time, json, hashlib
out, T = sys.argv[1], sys.argv[2]; files = sys.argv[3:]
W = '/home/user/data/w'; os.makedirs(W, exist_ok=True)
for fn in files:
    row = {'file': os.path.basename(fn), 'T': T}
    for lab in ['kolmo-base', 'kolmo-g6']:
        o = f'{W}/{lab}.x'
        t = time.perf_counter(); subprocess.run(['/home/user/data/' + lab, '-x', '-T', T, fn, o], check=True); c = time.perf_counter() - t
        h = hashlib.sha256(open(o, 'rb').read()).hexdigest()[:16]
        row[lab] = {'c': c, 'size': os.path.getsize(o), 'sha': h}
        if lab == 'kolmo-g6':
            t = time.perf_counter(); subprocess.run(['/home/user/data/' + lab, '-d', '-T', T, o, W + '/x.out'], check=True); row[lab]['d'] = time.perf_counter() - t
            row['rt'] = open(W + '/x.out', 'rb').read() == open(fn, 'rb').read()
        os.remove(o)
    print(json.dumps(row), flush=True); open(out, 'a').write(json.dumps(row) + '\n')
