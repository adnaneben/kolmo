"""Every GR6 / GR6+ / GR6-L3 option against plain Huffman, single thread and 4 threads.

Levels: L1 letters (character contexts), L2 words and numbers (field ops), L3 phrases (lzp: dictionary-free match;
dict: phrase dictionary, all candidates or chosen by the neural-net decision maker trained leave-one-file-out).
"L2 without L1" spells new words with no letter contexts (FLAT). Final coder: CM = neural context mixing (default),
huf = static Huffman over the same 6-bit unit streams. auto = level choice by racing on a sample (map-reduce).
Map-reduce/parallel parts at 4 threads: blocks (at most one per MiB), formula search, race trials, phrase mining.

usage: python3 bench_matrix.py WORKDIR GR6L3 GR6 HUFFMT SELDIR file...
writes WORKDIR/matrix.csv and prints markdown tables
"""
import csv, os, subprocess, sys

W, L3, GR6, HUF, SELDIR = sys.argv[1:6]
FILES = sys.argv[6:]
os.makedirs(W, exist_ok=True)

CONFIGS = [  # name, env, policy, final coder note
    ('L1 letters', {'LEVEL1': '1'}, 'none'),
    ('L2 words (no letter model)', {'FLAT': '1'}, 'none'),
    ('L1+L2 (GR6+)', {}, 'none'),
    ('L1+L3 lzp', {'LEVEL1': '1'}, 'lzp'),
    ('L2+L3 lzp', {'FLAT': '1'}, 'lzp'),
    ('L1+L2+L3 lzp', {}, 'lzp'),
    ('L1+L2+L3 dict all', {}, 'all'),
    ('L1+L2+L3 dict NN-selected', {}, 'sel:{sel}'),
    ('L1+L2+L3 lzp + dict NN-selected', {}, 'lzp+sel:{sel}'),
    ('auto (race)', {}, 'auto:524288'),
    ('L1 letters, Huffman final', {'LEVEL1': '1', 'CODER': 'huf'}, 'none'),
    ('L1+L2, Huffman final', {'CODER': 'huf'}, 'none'),
    ('L1+L2+L3 lzp, Huffman final', {'CODER': 'huf'}, 'lzp'),
    ('auto (race), Huffman final', {'CODER': 'huf'}, 'auto:524288'),
]


def run_l3(path, env, policy, T):
    e = dict(os.environ, **env)
    r = subprocess.run([L3, 'b', path, str(T), policy], capture_output=True, text=True, env=e, timeout=3000)
    line = [l for l in r.stdout.splitlines() if ' -> ' in l and 'threads' in l][0]
    assert line.rstrip().endswith('OK'), line
    size = int(line.split(' -> ')[1].split()[0])
    enc = float(line.split('| enc ')[1].split(' MB/s')[0]); dec = float(line.split(' dec ')[-1].split(' MB/s')[0])
    blocks = int(line.split(' blocks ')[1].split()[0])
    return size, enc, dec, blocks


rows = []
out = open(f'{W}/matrix.csv', 'w', newline=''); wr = csv.writer(out)
wr.writerow(['file', 'config', 'threads', 'blocks', 'bytes', 'ratio', 'enc_MBps', 'dec_MBps'])
for path in FILES:
    name = os.path.basename(path); n = os.path.getsize(path)
    for T in (1, 4):
        o = subprocess.run([HUF, path, str(T)], capture_output=True, text=True).stdout.split()
        rows.append((name, 'plain Huffman (bytes)', T, '-', int(o[1]), float(o[2]), float(o[3]), float(o[4])))
        wr.writerow(rows[-1]); out.flush()
    o = subprocess.run([GR6, 'b', path], capture_output=True, text=True).stdout
    size = int(o.split(' -> ')[1].split()[0]); enc = float(o.split('enc ')[1].split()[0]); dec = float(o.split('dec ')[1].split()[0])
    rows.append((name, 'GR6 (repeat+graph, fixed 6-bit)', 1, 1, size, n / size, enc, dec)); wr.writerow(rows[-1]); out.flush()
    for cname, env, pol in CONFIGS:
        sel = f'{SELDIR}/{name}.sel.mlp'
        if '{sel}' in pol and not os.path.exists(sel):
            continue
        for T in (1, 4):
            size, enc, dec, blocks = run_l3(path, env, pol.format(sel=sel), T)
            rows.append((name, cname, T, blocks, size, n / size, enc, dec)); wr.writerow(rows[-1]); out.flush()
        print(f'{name}: {cname} done', flush=True)
out.close()
