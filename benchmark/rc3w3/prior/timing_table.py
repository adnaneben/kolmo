"""Markdown tables from bench.py's JSON lines: per file and thread count, size and times of each build, and the time
ratios of BUILD against BASE.  python3 -I timing_table.py timing.jsonl BASE BUILD [OTHER...]"""
import json, sys, collections
recs = [json.loads(l) for l in open(sys.argv[1])]
base, build, others = sys.argv[2], sys.argv[3], sys.argv[4:]
by = collections.defaultdict(dict)
for r in recs: by[(r['threads'], r['file'])][r['build']] = r
sz = ['10k', '100k', '1m', '10m']
for T in sorted({k[0] for k in by}):
    keys = sorted([k for k in by if k[0] == T and base in by[k] and build in by[k]], key=lambda k: (sz.index(k[1].rsplit('_', 1)[1]), k[1]))
    if not keys: continue
    print(f'\n{T} thread{"s" if T > 1 else ""}:\n')
    cols = [c for c in others if any(c in by[k] for k in keys)]
    print('| file | ' + ' | '.join(f'{c} size, enc s, dec s' for c in [base, build] + cols) + f' | {build}/{base} enc | {build}/{base} dec |')
    print('|---|' + '---|' * (2 + len(cols)) + '---:|---:|')
    er, dr = [], []
    for k in keys:
        a, b = by[k][base], by[k][build]
        cell = lambda r: f"{r['size']:,}, {r['enc']:.2f}, {r['dec']:.2f}" + ('' if r['ok'] else ' (not verified)')
        e, d = b['enc'] / a['enc'], b['dec'] / a['dec']; er.append(e); dr.append(d)
        print(f'| {k[1]} | ' + ' | '.join(cell(by[k][c]) if c in by[k] else '—' for c in [base, build] + cols) + f' | {e:.2f} | {d:.2f} |')
    print(f'\nmedian {build}/{base}: encode {sorted(er)[len(er) // 2]:.2f}, decode {sorted(dr)[len(dr) // 2]:.2f}; max decode {max(dr):.2f}')
