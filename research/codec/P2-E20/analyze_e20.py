#!/usr/bin/env python3
"""P2-E20 frozen analysis (written before any fresh data was read; see PROTOCOL.md for the decision rules).
Reads results/{first,confirmation}/records.json(.xz). 'fits' = cost <= rival cost on EVERY pair (N1 cost fields)."""
import collections, json, lzma, math, pathlib, sys
root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else pathlib.Path(__file__).parent / 'results')
FRAMES = {'tp_phuf_pg16', 'tp_phuf', 'tb_lazy', 'tb_best', 'tp_pent', 'tp_arch19'}
PROD = {'kolmo_fast', 'kolmo_hc', 'kolmo_max'}
CONT = {'mcsv_planes_huf', 'mcsv_prod19'}
def load(run):
    for f in (root / run / 'records.json.xz', root / run / 'records.json'):
        if f.exists(): return json.loads(lzma.decompress(f.read_bytes()) if f.suffix == '.xz' else f.read_text())['records']
    raise SystemExit('missing ' + run)
def fits(r, o, key): return r.get('runs') and all(a[key] <= b[key] for a, b in zip(r['runs'], o['runs']))
def cp_lower(k, n, alpha=0.05):          # one-sided Clopper-Pearson lower bound by bisection on the binomial tail
    if k == 0: return 0.0
    lo, hi = 0.0, 1.0
    for _ in range(60):
        p = (lo + hi) / 2; tail = sum(math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(k, n + 1))
        lo, hi = (p, hi) if tail < alpha else (lo, p)
    return lo
def smallest(P, allowed, rival, key):
    feas = sorted((r['stored_bytes'], p) for p, r in P.items() if p in allowed and r.get('stored_bytes') is not None and fits(r, rival, key))
    return feas
out = {}
for run in ('first', 'confirmation'):
    by = {}
    for r in load(run): by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    res = dict(missing=[], H1={}, H2={}, H3={})
    wav = [i for i, P in by.items() if P['zstd_3']['view'] == 'wav']; csvs = [i for i, P in by.items() if P['zstd_3']['view'] == 'csv']
    for i, P in by.items():
        for p, r in P.items():
            if r.get('stored_bytes') is None: res['missing'].append([i, p])
    # H1: tp_phuf_pg16 vs zstd-3 on ESC-50 WAV
    k = sum(1 for i in wav if by[i]['tp_phuf_pg16'].get('stored_bytes') is not None and fits(by[i]['tp_phuf_pg16'], by[i]['zstd_3'], 'speed_cost_s')
            and by[i]['tp_phuf_pg16']['stored_bytes'] < by[i]['zstd_3']['stored_bytes'])
    fam = collections.Counter(); famn = collections.Counter()
    for i in wav:
        P = by[i]; famn[P['zstd_3']['family']] += 1
        fam[P['zstd_3']['family']] += bool(P['tp_phuf_pg16'].get('stored_bytes') is not None and fits(P['tp_phuf_pg16'], P['zstd_3'], 'speed_cost_s') and P['tp_phuf_pg16']['stored_bytes'] < P['zstd_3']['stored_bytes'])
    other = {p: sum(1 for i in wav if by[i][p].get('stored_bytes') is not None and fits(by[i][p], by[i]['zstd_3'], 'speed_cost_s') and by[i][p]['stored_bytes'] < by[i]['zstd_3']['stored_bytes'])
             for p in sorted(FRAMES | PROD)}
    res['H1'] = dict(n=len(wav), k=k, cp95_lower=round(cp_lower(k, len(wav)), 4), pass_=cp_lower(k, len(wav)) > 0.5,
                     per_family={f: f'{fam[f]}/{famn[f]}' for f in sorted(famn)}, all_pipelines_fit_and_smaller=other)
    # H2: archive, per source; Kolmo = production modes + production-format frames (containers excluded here)
    for src, ins in (('ESC50', wav), ('F38', csvs)):
        c = collections.Counter(); byte_k = byte_o = 0
        for i in ins:
            P = by[i]; o = [P[p] for p in P if p.startswith('openzl_')][0]
            if o.get('stored_bytes') is None: c['openzl_missing'] += 1; continue
            f = smallest(P, FRAMES | PROD, o, 'archive_cost_usd'); w = f[0] if f and f[0][0] < o['stored_bytes'] else (o['stored_bytes'], 'openzl_9')
            c[w[1]] += 1; byte_k += w[0]; byte_o += o['stored_bytes']
        kol = sum(v for p, v in c.items() if p not in ('openzl_9', 'openzl_missing'))
        res['H2'][src] = dict(n=len(ins), winners=dict(c), kolmo=kol, openzl=c['openzl_9'], pass_=kol > c['openzl_9'],
                             frontier_bytes_vs_openzl9=round(byte_k / byte_o - 1, 4) if byte_o else None)
    # H3 (P2-D1): research containers on fresh CSVs, speed and archive
    sp = ar = 0; detail = []
    for i in csvs:
        P = by[i]; z = P['zstd_3']; o = [P[p] for p in P if p.startswith('openzl_')][0]
        fs = smallest(P, FRAMES | PROD | CONT, z, 'speed_cost_s'); fs = [x for x in fs if x[0] < z['stored_bytes']]
        s_unique = bool(fs) and fs[0][1] in CONT and (len(fs) == 1 or fs[1][0] > fs[0][0])
        fa = smallest(P, FRAMES | PROD | CONT, o, 'archive_cost_usd') if o.get('stored_bytes') is not None else []
        fa = [x for x in fa if x[0] < o['stored_bytes']] if fa else []
        a_unique = bool(fa) and fa[0][1] in CONT and (len(fa) == 1 or fa[1][0] > fa[0][0])
        sp += s_unique; ar += a_unique
        best_nc_s = next((x for x in fs if x[1] not in CONT), None); best_nc_a = next((x for x in fa if x[1] not in CONT), None)
        detail.append(dict(input=i, speed=fs[0] if fs else None, speed_best_noncontainer=best_nc_s, archive=fa[0] if fa else None, archive_best_noncontainer=best_nc_a))
    n = len(csvs)
    res['H3'] = dict(n=n, container_unique_smallest_speed=sp, container_unique_smallest_archive=ar,
                     useful_in_this_run=bool(n) and (sp >= 0.2 * n or ar >= 0.2 * n), detail=detail)
    out[run] = res
both = lambda k, f: all(f(out[r][k]) for r in out)
out['decisions'] = dict(H1_pass_both_runs=both('H1', lambda h: h['pass_']),
                        H2_ESC50_pass_both=both('H2', lambda h: h['ESC50']['pass_']), H2_F38_pass_both=both('H2', lambda h: h['F38']['pass_']),
                        P2D1_useful_both_runs=both('H3', lambda h: h['useful_in_this_run']))
print(json.dumps(out, indent=1))
