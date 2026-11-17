"""Runs every gpu2 measurement and Kolmo's own end-to-end times on the same inputs, one at a time.

python3 -I benchmark/rc3w2/gpu2/run.py BENCH DATA OUT.jsonl [only...]

BENCH is build/gpu2/gpu2bench, DATA the directory mkdata.py filled. Waits while the benchmark lock (GPU2_LOCK) exists
before each job; a job that the lock interrupted (exit 3) is retried. Results are appended, one JSON object per line;
finished jobs (by name) are skipped on a rerun.
"""
import json, os, subprocess, sys, time

bench, data, out = sys.argv[1], sys.argv[2], sys.argv[3]
only = set(sys.argv[4:])
lock = os.environ.get('GPU2_LOCK', '')
kolmo = os.path.join(os.path.dirname(os.path.abspath(bench)), '..', 'kolmo')


def D(name):
    return os.path.join(data, name)


JOBS = [
    ('table_owid8', [bench, 'table', D('owid8.csv'), 'csv']),
    ('table_owid8_64', [bench, 'table', D('owid8.csv'), 'csv', '64']),
    ('table_energy16', [bench, 'table', D('energy16.csv'), 'csv']),
    ('table_jsonl', [bench, 'table', D('owid.jsonl'), 'jsonl']),
    ('table_1mb', [bench, 'table', D('owid_1mb.csv'), 'csv']),
    ('table_4mb', [bench, 'table', D('owid_4mb.csv'), 'csv']),
    ('table_16mb', [bench, 'table', D('owid_16mb.csv'), 'csv']),
    ('formula_energy', [bench, 'formula', '/Users/adnanbenali/Downloads/kolmo-bench/data/energy_test.csv']),
    ('formula_owid', [bench, 'formula', '/Users/adnanbenali/Downloads/kolmo-bench/data/owid-co2-data.csv']),
    ('formula_k8', [bench, 'formula', D('energy_k8.csv')]),
    ('formula_k16', [bench, 'formula', D('energy_k16.csv')]),
    ('formula_k32', [bench, 'formula', D('energy_k32.csv')]),
    ('formula_k64', [bench, 'formula', D('energy_k64.csv')]),
    ('krw_mix1', [bench, 'krw', D('mix1.bin')]),
    ('krw_mix16', [bench, 'krw', D('mix16.bin')]),
    ('krw_mix64', [bench, 'krw', D('mix64.bin')]),
    ('krw_mix256', [bench, 'krw', D('mix256.bin')]),
    ('krw_mix256_l1024', [bench, 'krw', D('mix256.bin'), '1024', '1024']),
    ('krw_mix256_s256', [bench, 'krw', D('mix256.bin'), '256', '256']),
    ('krw_bf16', [bench, 'krw', D('bf16x5.bin')]),
    ('hist_mix1', [bench, 'hist', D('mix1.bin')]),
    ('hist_mix16', [bench, 'hist', D('mix16.bin')]),
    ('hist_mix64', [bench, 'hist', D('mix64.bin')]),
    ('hist_mix256', [bench, 'hist', D('mix256.bin')]),
    ('hist_mix512', [bench, 'hist', D('mix512.bin')]),
    ('video_cif', [bench, 'video', '/private/tmp/claude-501/-Users-adnanbenali-Projects-kolmo/150de77a-2a2f-5cf3-b144-4e505fb7f30d/scratchpad/video/mobile_cif.y4m']),
    ('video_1080', [bench, 'video', D('v1080.y4m')]),
    ('video_4k', [bench, 'video', D('v4k.y4m')]),
    ('batch_repo', [bench, 'batch', D('files.txt')]),
    ('sa_16', [bench, 'sa', D('text256.bin'), '16']),
    ('sa_64', [bench, 'sa', D('text256.bin'), '64']),
    ('sa_128', [bench, 'sa', D('text256.bin'), '128']),
    ('sa_256', [bench, 'sa', D('text256.bin'), '256']),
]
# Kolmo end to end on the same inputs (context for the stage times): (name, file, args)
KOLMO = [
    ('owid8.csv', D('owid8.csv'), ['-m', 'speed']), ('owid8.csv', D('owid8.csv'), ['-m', 'balanced']),
    ('owid8.csv', D('owid8.csv'), ['-m', 'ratio']),
    ('owid.jsonl', D('owid.jsonl'), ['-m', 'speed']), ('owid.jsonl', D('owid.jsonl'), ['-m', 'balanced']),
    ('energy16.csv', D('energy16.csv'), ['-m', 'balanced']),
    ('energy_test.csv', '/Users/adnanbenali/Downloads/kolmo-bench/data/energy_test.csv', ['-x']),
    ('mix256.bin', D('mix256.bin'), ['-m', 'balanced']), ('bf16x5.bin', D('bf16x5.bin'), ['-m', 'balanced']),
    ('v4k.y4m', D('v4k.y4m'), ['-m', 'balanced']), ('text256.bin', D('text256.bin'), ['-m', 'ratio']),
]


def wait_lock():
    while lock and os.path.exists(lock):
        time.sleep(30)


def done_names():
    s = set()
    if os.path.exists(out):
        for line in open(out):
            try:
                s.add(json.loads(line).get('job'))
            except ValueError:
                pass
    return s


def emit(obj):
    with open(out, 'a') as f:
        f.write(json.dumps(obj) + '\n')


have = done_names()
for name, cmd in JOBS:
    if (only and name not in only) or name in have:
        continue
    while True:
        wait_lock()
        p = subprocess.run(cmd, capture_output=True, text=True)
        if p.returncode == 3:
            continue
        break
    sys.stderr.write('%s: rc %d\n%s' % (name, p.returncode, p.stderr))
    lines = [json.loads(l) for l in p.stdout.splitlines() if l.startswith('{')]
    for l in lines:
        l['job'] = name
        emit(l)
    if p.returncode:
        emit({'job': name, 'error': p.returncode, 'stderr': p.stderr[-2000:]})

work = os.path.join(os.path.dirname(os.path.abspath(out)), 'kolmo_work')
os.makedirs(work, exist_ok=True)
for fname, path, args in KOLMO:
    name = 'kolmo_%s_%s' % (fname, '_'.join(a.strip('-') for a in args))
    if (only and 'kolmo' not in only and name not in only) or name in have:
        continue
    c, dd = os.path.join(work, 'c.k'), os.path.join(work, 'd.out')
    rounds = 1 if '-x' in args else 3
    wt, dt = [], []
    for r in range(rounds):
        wait_lock()
        t0 = time.time(); subprocess.run([kolmo] + args + ['-T', '10', path, c], check=True); wt.append(time.time() - t0)
        t0 = time.time(); subprocess.run([kolmo, '-d', c, dd], check=True); dt.append(time.time() - t0)
    same = open(path, 'rb').read() == open(dd, 'rb').read()
    n, z = os.path.getsize(path), os.path.getsize(c)
    wt.sort(); dt.sort()
    emit({'job': name, 'test': 'kolmo', 'file': fname, 'args': ' '.join(args) + ' -T 10', 'n': n, 'coded': z,
          'ratio': round(n / z, 3), 'enc_s': round(wt[len(wt) // 2], 3), 'dec_s': round(dt[len(dt) // 2], 3),
          'enc_mbs': round(n / wt[len(wt) // 2] / 1e6, 1), 'roundtrip': same})
    sys.stderr.write('%s done\n' % name)
