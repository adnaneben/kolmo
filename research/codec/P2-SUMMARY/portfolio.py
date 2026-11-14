#!/usr/bin/env python3
"""Realizable type-first speed policy, evaluated from committed records (no oracle, no extra trials).
Rule (frozen tonight, development): declared integer array (NumPy/WAV) -> tf_planes_huf_pg16; CSV passing the P2-E6
typing contract -> mcsv_planes_huf; anything else -> the same tools' opaque fallback (whole-input zstd at level 3).
Because typing, guards and fallbacks run inside each measured pipeline, the rule's cost is the measured cost of the
pipeline it names. Reports, per data set: inputs where the rule is inside zstd-3's cost on every pair, smaller, both.
Data sets are reported separately, never pooled: in-memory development (P2-E2, P2-E6) and N1 CLI (P2-E3, P2-E7)."""
import io, json, lzma, sys, tarfile, importlib.util, pathlib
R = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('pp', R / 'competitor-policy-v2' / 'policy.py'); pp = importlib.util.module_from_spec(spec)
sys.modules['pp'] = pp; spec.loader.exec_module(pp); M = pp.CostModel()

def local(tarpath, kind, rule_for):
    t = tarfile.open(fileobj=io.BytesIO(lzma.decompress(tarpath.read_bytes())))
    out = []
    for m in t.getmembers():
        if not m.isfile() or not m.name.endswith('.jsonl'): continue
        rows = [json.loads(l) for l in t.extractfile(m).read().decode().splitlines() if l.strip()]
        size = {r['candidate']: r['stored'] for r in rows if r['kind'] == kind + '_size'}
        runs = {}
        for r in rows:
            if r['kind'] == kind + '_time': runs.setdefault(r['candidate'], {})[r['pair_id']] = pp.run_cost(r, size[r['candidate']], True, M)
        name = pathlib.Path(m.name).name[:-6]; c = rule_for(name)
        if c not in size: continue
        z = runs['zstd3']; fit = all(runs[c][p] <= z[p] for p in z)
        out.append((name, c, fit, size[c] < size['zstd3'], 100 * (size[c] / size['zstd3'] - 1)))
    return out

def cli(recpath, rule_for):
    recs = json.loads(lzma.decompress(recpath.read_bytes()))['records']; by = {}
    for r in recs: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    out = []
    for inp, P in by.items():
        c = rule_for(inp)
        if c not in P or P[c]['status'] != 'MEASURED_DEVELOPMENT_CONTROL': continue
        z = P['zstd_3']; r = P[c]
        fit = all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs']))
        out.append((inp, c, fit, r['stored_bytes'] < z['stored_bytes'], 100 * (r['stored_bytes'] / z['stored_bytes'] - 1)))
    return out

def show(title, rows):
    f = sum(x[2] for x in rows); s = sum(x[3] for x in rows); b = sum(x[2] and x[3] for x in rows)
    print(f"\n{title}: {len(rows)} inputs; inside zstd-3 cost on every pair {f}; smaller {s}; both {b}; worst bytes {max(x[4] for x in rows):+.1f}%")
    for x in sorted(rows): print(f"  {x[0][:34]:34} {x[1]:20} {'fit ' if x[2] else 'over'} {x[4]:+7.1f}%")

arr = lambda n: 'tf_planes_huf_pg16'
csv = lambda n: 'mcsv_planes_huf'
cli_rule = lambda n: 'mcsv_planes_huf' if n.endswith('__table') else 'tf_planes_huf_pg16'
show('P2-E2 in-memory development arrays (20)', local(R / 'P2-E2/results/records.tar.xz', 'p2e2', arr))
show('P2-E6 in-memory development CSVs (10, revision 1 records)', local(R / 'P2-E6/results/records.tar.xz', 'p2e6', csv))
for run in ('first', 'confirmation'):
    show(f'P2-E7 N1 CLI boundary, {run} run (8 NPY + 2 CSV)', cli(R / f'P2-E7/results/{run}/records.json.xz', cli_rule))
