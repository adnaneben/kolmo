# full-size table: every variant against plain CM (codec 7, model 3), whole file as one stream
import json, sys, glob, collections
rows = collections.OrderedDict()
for p in sorted(glob.glob(sys.argv[1] + '/*.jsonl')):
    for l in open(p):
        if not l.startswith('{'): continue
        j = json.loads(l); v = j['variant'] + ('-T%d' % j['T'] if 'T' in j else '')
        rows.setdefault(j['file'], {'n': j['n']})[v] = j
cols = ['word+cm', 'phrase+cm', 'cmx-null', 'cmx-dict', 'cmx-adapt-T8']
print('| file | bytes | CM (codec 7) | ' + ' | '.join(cols) + ' |'); print('|---|---:|---:|' + '---:|' * len(cols))
for f, r in rows.items():
    b = r.get('cm', {}).get('size')
    if not b: continue
    cells = [f"{100 * (r[c]['size'] - b) / b:+.2f}%" if c in r else '' for c in cols]
    print(f"| {f} | {r['n']:,} | {b:,} | " + ' | '.join(cells) + ' |')
