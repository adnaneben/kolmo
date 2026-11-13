#!/usr/bin/env python3
"""P2-E17: smallest pipeline inside OpenZL typed-9's archive cost on every pair, all candidates vs production-only."""
import collections, json, lzma, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
PROD = {'kolmo_fast', 'kolmo_hc', 'kolmo_max', 'zstd_3'}
def load(run):
    f = root / run / 'records.json.xz'
    return json.loads(lzma.decompress(f.read_bytes()) if f.exists() else (root / run / 'records.json').read_text())['records']
out = {}
for run in ('first', 'confirmation'):
    by = {}
    for r in load(run):
        if r.get('stored_bytes') is not None: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    res = {}
    for scope, allowed in (('all', None), ('production_only', PROD)):
        fam = collections.defaultdict(collections.Counter); tot = collections.Counter(); ob = wb = 0
        for i, P in by.items():
            o = [P[k] for k in P if k.startswith('openzl_')][0]
            feas = sorted((r['stored_bytes'], p) for p, r in P.items() if not p.startswith('openzl_') and (allowed is None or p in allowed)
                          and all(a['archive_cost_usd'] <= b['archive_cost_usd'] for a, b in zip(r['runs'], o['runs'])))
            w = feas[0] if feas and feas[0][0] < o['stored_bytes'] else (o['stored_bytes'], 'openzl_9')
            fam[o['family']][w[1]] += 1; tot[w[1]] += 1; ob += o['stored_bytes']; wb += w[0]
        res[scope] = dict(total=dict(tot), bytes_vs_openzl9=round(wb / ob - 1, 4), families={f: dict(c) for f, c in sorted(fam.items())})
    out[run] = res
print(json.dumps(out, indent=1))
