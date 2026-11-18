# Builds train/<type>/train.bin (training text, at most 4 MB, sources listed) and test/<type>/<name>_<size>
# (first N bytes cut at a line end). Train and test come from different public sources (listed in SOURCES.txt).
import os, sys, glob
S = sys.argv[1]; R = S + '/dl/raw'; X = R + '/x'
out = S + '/corpus'
def rd(p, lim=None):
    with open(p, 'rb') as f: return f.read(lim) if lim else f.read()
def cut(b, n):
    if len(b) <= n: return None
    j = b.rfind(b'\n', 0, n)
    return b[:j + 1] if j > n // 2 else b[:n]
TRAIN = {
 'markup': [X + '/xml'] + sorted(glob.glob(R + '/htmltrain/*.html')),
 'json': sorted(glob.glob(R + '/jsontrain/*.json')),
 'csv': sorted(glob.glob(R + '/csvtrain/*.csv')),
 'log': sorted(glob.glob(R + '/loghub2k/*.log')),
 'c': sorted(glob.glob('/home/user/kolmo/vendor/zstd/lib/**/*.c', recursive=True))[:40] + sorted(glob.glob('/home/user/kolmo/vendor/xz/**/*.c', recursive=True))[:40] + sorted(glob.glob('/home/user/kolmo/vendor/brotli/**/*.c', recursive=True)) + sorted(glob.glob('/home/user/kolmo/vendor/lz4/**/*.c', recursive=True)),
 'py': sorted(glob.glob('/usr/lib/python3/dist-packages/**/*.py', recursive=True)),
 'js': sorted(glob.glob(R + '/jstrain/*.js')),
 'text': [X + '/dickens'],
 'sql': [X + '/sakila-db/sakila-db/sakila-data.sql', X + '/sakila-db/sakila-db/sakila-schema.sql'],
}
TEST = {
 'markup': {'cldr': sorted(glob.glob(R + '/xml/*.xml')), 'whatwg': [R + '/html/whatwg.html']},
 'json': {'gharchive': [X + '/gh.json'], 'twitter': [R + '/json/twitter.json']},
 'csv': {'power': [X + '/power/household_power_consumption.txt']},
 'log': {'zookeeper': [X + '/Zookeeper/Zookeeper.log'], 'apache': [X + '/Apache/Apache.log'], 'nginx': [R + '/nginx/nginx_logs']},
 'c': {'sqlite': [X + '/sqlite/sqlite-amalgamation-3460000/sqlite3.c', X + '/sqlite/sqlite-amalgamation-3460000/shell.c', X + '/sqlite/sqlite-amalgamation-3460000/sqlite3.h']},
 'py': {'stdlib': sorted(glob.glob('/usr/lib/python3.12/*.py')) + sorted(glob.glob('/usr/lib/python3.12/[a-z]*/**/*.py', recursive=True))},
 'js': {'plotly': [R + '/js/plotly.js'], 'three': [R + '/js/three.module.js']},
 'text': {'cantlarge': [X + '/large/bible.txt', X + '/large/world192.txt', X + '/cantrbry/alice29.txt', X + '/cantrbry/lcet10.txt', X + '/cantrbry/plrabn12.txt', X + '/cantrbry/asyoulik.txt', X + '/webster']},
 'sql': {'employees': [R + '/sql/load_employees.dump', R + '/sql/load_salaries1.dump']},
}
SIZES = [('10k', 10000), ('100k', 100000), ('1m', 1000000), ('10m', 10000000)]
src = open(out + '/SOURCES.txt', 'w') if os.path.isdir(out) else None
os.makedirs(out, exist_ok=True); src = open(out + '/SOURCES.txt', 'w')
for t, fs in TRAIN.items():
    os.makedirs(f'{out}/train/{t}', exist_ok=True)
    b = bytearray(); used = []
    # round-robin over files so the 4 MB cap keeps variety
    per = max((4 << 20) // max(len(fs), 1), 64 << 10)
    for p in fs:
        d = rd(p)
        if not d: continue
        b += d[:per] if len(fs) > 1 else d[:4 << 20]; used.append(os.path.basename(p))
        if len(b) >= 4 << 20: break
    b = b[:4 << 20]
    open(f'{out}/train/{t}/train.bin', 'wb').write(b)
    src.write(f'train {t}: {len(b)} bytes from {len(used)} files: {" ".join(used[:30])}{" ..." if len(used) > 30 else ""}\n')
for t, sets in TEST.items():
    os.makedirs(f'{out}/test/{t}', exist_ok=True)
    for name, fs in sets.items():
        b = bytearray()
        for p in fs:
            b += rd(p, 12 << 20)
            if len(b) >= 11_000_000: break
        for sn, sz in SIZES:
            c = cut(bytes(b), sz)
            if c is None: continue
            open(f'{out}/test/{t}/{name}_{sn}', 'wb').write(c)
        src.write(f'test {t}/{name}: {len(b)} bytes from {len(fs)} files: {" ".join(os.path.basename(p) for p in fs[:12])}\n')
