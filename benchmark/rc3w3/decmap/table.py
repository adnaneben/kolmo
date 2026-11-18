# table.py RESULTS_DIR : per-file markdown table and per-mode geometric means from the A/B jsonl files
# (later files override earlier rows of the same frame and thread count: the re-measured outliers).
import json, sys, math, collections, os
D = sys.argv[1]; rows = {}
for fn in ['ab_T1.jsonl', 'ab_T4.jsonl', 'ab_x.jsonl', 'ab_re.jsonl', 'ab_re2.jsonl']:
    p = os.path.join(D, fn)
    if os.path.exists(p):
        for l in open(p): r = json.loads(l); rows[(r['frame'], r['T'])] = r
frames = sorted({f for f, _ in rows}, key=lambda f: (f.split('/')[0], f))
print('| frame (mode/file) | raw bytes | frame bytes | T1 rc3 MB/s | T1 branch MB/s | T1 | T4 rc3 MB/s | T4 branch MB/s | T4 |')
print('|---|---:|---:|---:|---:|---:|---:|---:|---:|')
g = collections.defaultdict(list)
for f in frames:
    c = [rows.get((f, t)) for t in (1, 4)]; r0 = c[0] or c[1]
    cells = []
    for t, r in zip((1, 4), c):
        if r: cells += [f"{r['A_MBs']:.1f}", f"{r['B_MBs']:.1f}", f"{r['gain']:+.1%}"]; g[(f.split('/')[0], t)].append(math.log(r['B_MBs'] / r['A_MBs']))
        else: cells += ['—'] * 3
    print(f"| {f.replace('.k', '')} | {r0['size']} | {r0['csize']} | " + ' | '.join(cells) + ' |')
print('\n| mode | files | T1 geomean | T4 geomean |\n|---|---:|---:|---:|')
for m in sorted({m for m, _ in g}):
    a, b = g.get((m, 1), []), g.get((m, 4), [])
    fm = lambda v: f"{math.exp(sum(v) / len(v)) - 1:+.1%}" if v else '—'
    print(f"| {m} | {max(len(a), len(b))} | {fm(a)} | {fm(b)} |")
