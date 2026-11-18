# table: files x variants, size relative to plain CM (codec 7, model 3)
import json, sys, glob, os
rows = {}
for p in sys.argv[1:]:
    tag = os.path.basename(p).replace('.jsonl', '')
    for l in open(p):
        if not l.startswith('{'): continue
        j = json.loads(l)
        v = j['variant'] if j['variant'] != 'phrase+cm' else tag
        rows.setdefault(j['file'], {})[v] = j['size']
cols = []
for r in rows.values():
    for k in r:
        if k not in cols: cols.append(k)
print('| file | ' + ' | '.join(cols) + ' |')
print('|---|' + '---:|' * len(cols))
for f, r in rows.items():
    b = r.get('cm')
    cells = []
    for c in cols:
        if c not in r: cells.append(''); continue
        cells.append(f"{r[c]:,}" if c == 'cm' or not b else f"{100.0 * (r[c] - b) / b:+.2f}%")
    print(f'| {f} | ' + ' | '.join(cells) + ' |')
