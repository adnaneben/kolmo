#!/usr/bin/env python3
"""Aggregate E02C raw records into the result tables.

usage: analyze.py RAW_JSONL OUT_DIR    -> OUT_DIR/summary.json, OUT_DIR/tables.md
Statistics: per (input, configuration) the MEDIAN over all repetitions (no repetition dropped); IQR reported for
aggregate timings. Sizes are exact and deterministic. Families: real/derived inputs and synthetic inputs are
aggregated separately; synthetic inputs never enter the real-data aggregates.
"""
import collections, json, pathlib, statistics, sys

HERE = pathlib.Path(__file__).resolve().parent
REPS = ['exist', 'gbp', 'lbp128', 'gpp', 'lpp128']; BES = ['store', 'huf', 'rans', 'zstd3']
CONFS = [f'{r}+{b}' for r in REPS for b in BES] + [f'sel+{b}' for b in BES] + [f'planes+{b}' for b in BES] + ['exist+ent', 'planes+ent', 'lpp128+ent'] \
        + ['exist+zstd19', 'planes+zstd19', 'exist+prod3', 'planes+prod3', 'exist+prod19', 'planes+prod19', 'exist+prod3g', 'planes+prod3g']
FIXED = CONFS[:20] + CONFS[24:28] + CONFS[31:33]

def med(v): return statistics.median(v) if v else None
def q(v, p): v = sorted(v); return v[min(len(v) - 1, int(p * (len(v) - 1) + 0.5))] if v else None

def main(raw, out, manifest=None):
    out = pathlib.Path(out); out.mkdir(parents=True, exist_ok=True)
    man = {i['name']: i for i in json.loads(pathlib.Path(manifest or HERE / 'corpus/manifest.json').read_text())['inputs']}
    R = [json.loads(l) for l in open(raw)]
    fam = lambda n: man[n]['family']; real = lambda n: man[n]['origin'] != 'synthetic'
    S = {}
    # ---------------- regime A: typed ----------------
    capA = {d['input']: d for d in R if d['kind'] == 'typed_capture'}
    naA = sorted(d['input'] for d in R if d['kind'] == 'typed_na')
    totA = collections.defaultdict(dict)
    for d in R:
        if d['kind'] == 'typed_total': totA[d['input']][d['config']] = d['payload']
    # ---------------- regime B: planner ----------------
    frB = {(d['input'], d['mode']): d for d in R if d['kind'] == 'planner_frame'}
    totB = collections.defaultdict(dict)
    for d in R:
        if d['kind'] == 'planner_total': totB[(d['input'], d['mode'])][d['config']] = d['total']
    # ---------------- kernel sizes / times ----------------
    ksz = {}; kt = collections.defaultdict(lambda: collections.defaultdict(list))
    for d in R:
        if d['kind'] == 'kernel_size': ksz[(d['regime'], d['input'], d.get('mode', ''), d['config'])] = d
        if d['kind'] == 'kernel_time':
            for k in ('enc_wall_ns', 'enc_cpu_ns', 'dec_wall_ns', 'dec_cpu_ns'): kt[(d['regime'], d['input'], d['config'])][k].append(d[k])
    # kernel records for regime B carry no mode: they are emitted once per (input, mode) job; disambiguate by job
    kszB = collections.defaultdict(dict); jobmode = {}
    for d in R:
        if d['kind'] == 'planner_frame': jobmode[d['job']] = d['mode']
    ktB = collections.defaultdict(lambda: collections.defaultdict(list))
    for d in R:
        if d.get('regime') == 'B_planner' and d['kind'] == 'kernel_size': kszB[(d['input'], jobmode[d['job']])][d['config']] = d
        if d.get('regime') == 'B_planner' and d['kind'] == 'kernel_time':
            for k in ('enc_wall_ns', 'enc_cpu_ns', 'dec_wall_ns', 'dec_cpu_ns'): ktB[(d['input'], jobmode[d['job']], d['config'])][k].append(d[k])
    e2e = collections.defaultdict(list)
    for d in R:
        if d['kind'] == 'e2e_time': e2e[(d['input'], d['mode'], d['what'], d.get('config', ''))].append((d['wall_ns'], d['cpu_ns']))
    mem = collections.defaultdict(list)
    for d in R:
        if d['kind'] == 'mem': mem[(d['input'], d['op'], d['config'])].append(d['peak_extra_kb'])

    T = []  # markdown
    def table(title, header, rows):
        T.append(f'\n### {title}\n'); T.append('| ' + ' | '.join(header) + ' |'); T.append('|' + '---|' * len(header))
        for r in rows: T.append('| ' + ' | '.join(str(x) for x in r) + ' |')

    # ===== A: sizes =====
    A = dict(not_applicable=naA, inputs={})
    for n, c in capA.items():
        A['inputs'][n] = dict(family=fam(n), origin=man[n]['origin'], bytes=c['bytes'], production_payload=c['payload'], zstd3=c['zstd3_whole_input'],
                              production_residual_codec=c['residual_codec'], order=c['order'], ow=c['ow'], m=c['m'], configs=totA[n])
    def agg(sel_inputs, getprod, getconf):
        res = {}
        for cf in CONFS:
            tot = sum(getconf(n, cf) for n in sel_inputs); res[cf] = tot
        return res
    for scope, pred in (('real', real), ('synthetic', lambda n: not real(n))):
        ins = [n for n in capA if pred(n)]
        if not ins: continue
        prod = sum(capA[n]['payload'] for n in ins); z3 = sum(capA[n]['zstd3_whole_input'] for n in ins); raw = sum(capA[n]['bytes'] for n in ins)
        rows = []
        for cf in CONFS:
            t = sum(totA[n][cf] for n in ins); wins = sum(totA[n][cf] < capA[n]['payload'] for n in ins); loss = sum(totA[n][cf] > capA[n]['payload'] for n in ins)
            ex = sum(totA[n][cf.split('+')[0] == 'sel' and 'exist+' + cf.split('+')[1] or 'exist+' + cf.split('+')[1]] for n in ins)
            rows.append([cf, t, f'{100 * (t / prod - 1):+.2f}%', f'{100 * (t / ex - 1):+.2f}%', f'{wins}/{loss}/{len(ins) - wins - loss}'])
        A[f'aggregate_{scope}'] = dict(inputs=ins, raw=raw, production=prod, zstd3=z3, rows=rows)
        table(f'Regime A (typed, kg_encode_num at zstd level 3), {scope} inputs: total stored bytes (n={len(ins)}; input {raw} B; production typed payload {prod} B; zstd -3 whole input {z3} B)',
              ['config', 'bytes', 'vs production payload', 'vs exist, same backend', 'inputs smaller/larger/equal vs production'], rows)
    # per-family A
    rows = []
    for f in sorted({fam(n) for n in capA}):
        ins = [n for n in capA if fam(n) == f]; prod = sum(capA[n]['payload'] for n in ins)
        best = min(FIXED, key=lambda cf: sum(totA[n][cf] for n in ins))
        r = [f, len(ins), prod]
        for cf in ('exist+huf', 'exist+rans', 'lbp128+rans', 'gpp+rans', 'lpp128+huf', 'lpp128+rans', 'lpp128+zstd3', 'sel+rans'):
            r.append(f'{100 * (sum(totA[n][cf] for n in ins) / prod - 1):+.2f}%')
        r.append(best); rows.append(r)
    table('Regime A per family: change vs production typed payload (fixed configurations)', ['family', 'n', 'production B', 'exist+huf', 'exist+rans', 'lbp128+rans', 'gpp+rans', 'lpp128+huf', 'lpp128+rans', 'lpp128+zstd3', 'sel+rans', 'smallest fixed config'], rows)
    S['A_typed'] = A

    # ===== B: sizes =====
    B = {}
    for mode in ('fast', 'hc'):
        keys = [k for k in frB if k[1] == mode]
        for scope, pred in (('real', real), ('synthetic', lambda n: not real(n))):
            ins = [k for k in keys if pred(k[0])]
            if not ins: continue
            prod = sum(frB[k]['frame'] for k in ins); z3 = sum(frB[k]['zstd3_whole_input'] for k in ins); raw = sum(frB[k]['bytes'] for k in ins)
            withint = [k for k in ins if frB[k]['int_blocks']]
            rows = []
            for cf in CONFS:
                t = sum(totB[k].get(cf, frB[k]['frame']) for k in ins)
                wins = sum(totB[k].get(cf, frB[k]['frame']) < frB[k]['frame'] for k in ins); loss = sum(totB[k].get(cf, frB[k]['frame']) > frB[k]['frame'] for k in ins)
                ex = sum(totB[k].get('exist+' + cf.split('+')[1], frB[k]['frame']) for k in ins)
                rows.append([cf, t, f'{100 * (t / prod - 1):+.2f}%', f'{100 * (t / ex - 1):+.2f}%', f'{wins}/{loss}/{len(ins) - wins - loss}'])
            B[f'{mode}_{scope}'] = dict(inputs=[k[0] for k in ins], with_int=[k[0] for k in withint], raw=raw, production_frame=prod, zstd3=z3, rows=rows)
            table(f'Regime B (complete planner, {mode}), {scope} inputs: complete-input stored bytes (n={len(ins)}, {len(withint)} with INT blocks; input {raw} B; production frame {prod} B; zstd -3 {z3} B)',
                  ['config', 'bytes', 'vs production frame', 'vs exist, same backend', 'inputs smaller/larger/equal'], rows)
    rows = []
    for k in sorted(frB):
        f = frB[k]; ib = [b for b in f['blocks_detail'] if 'int' in b]
        rows.append([k[0], k[1], man[k[0]]['origin'], f['bytes'], f['frame'], f['zstd3_whole_input'], f'{f["int_blocks"]}/{f["blocks"]}',
                     ','.join(sorted({b['int']['residual_codec'] for b in ib})) or '-', ','.join(sorted({f'w{b["int"]["w"]}o{b["int"]["order"]}ow{b["int"]["ow"]}' for b in ib})) or '-',
                     min(((totB[k][cf], cf) for cf in FIXED), default=(f['frame'], '-'))[1] if ib else '-'])
    table('Regime B per input: production planner selection', ['input', 'mode', 'origin', 'bytes', 'frame', 'zstd -3', 'INT blocks', 'prod residual codec', 'INT shape', 'smallest fixed config'], rows)
    S['B_planner'] = B
    cand = ['exist+zstd3', 'gpp+huf', 'gpp+zstd3', 'lpp128+huf', 'lpp128+rans', 'lpp128+zstd3', 'sel+huf', 'sel+rans', 'sel+zstd3']
    rows = []
    for k in sorted(frB):
        if not frB[k]['int_blocks']: continue
        rows.append([k[0], k[1], man[k[0]]['origin'], frB[k]['frame']] + [f"{100 * (totB[k][cf] / frB[k]['frame'] - 1):+.2f}%" for cf in cand])
    table('Regime B per input with INT blocks: complete-input change vs production frame', ['input', 'mode', 'origin', 'production frame'] + cand, rows)
    rows = []
    for n in sorted(capA):
        rows.append([n, man[n]['origin'], capA[n]['payload'], f"o{capA[n]['order']}ow{capA[n]['ow']}"] + [f'{100 * (totA[n][cf] / capA[n]["payload"] - 1):+.2f}%' for cf in cand])
    table('Regime A per input: typed payload change vs production typed payload', ['input', 'origin', 'production payload', 'INT shape'] + cand, rows)

    # ===== kernel timing =====
    K = {}
    for reg, label in (('A_typed', 'A typed'), ('B_planner_fast', 'B planner fast'), ('B_planner_hc', 'B planner hc')):
        if reg == 'A_typed':
            items = [(n, kt[('A_typed', n, cf)], cf) for n in capA for cf in CONFS]; resb = {n: capA[n]['m'] * capA[n]['ow'] for n in capA}
            ins = [n for n in capA if real(n)]; get = lambda n, cf: kt[('A_typed', n, cf)]
        else:
            mode = reg.split('_')[-1]; ins = [k[0] for k in frB if k[1] == mode and frB[k]['int_blocks'] and real(k[0])]
            get = lambda n, cf, mode=mode: ktB[(n, mode, cf)]
            resb = {k[0]: sum(b['int']['m'] * b['int']['ow'] for b in frB[k]['blocks_detail'] if 'int' in b) for k in frB if k[1] == mode}
        if not ins: continue
        tot_res = sum(resb[n] for n in ins); rows = []
        for cf in CONFS:
            e = [med(get(n, cf)['enc_wall_ns']) for n in ins]; d = [med(get(n, cf)['dec_wall_ns']) for n in ins]
            ec = [med(get(n, cf)['enc_cpu_ns']) for n in ins]; dc = [med(get(n, cf)['dec_cpu_ns']) for n in ins]
            # spread: per-rep sums across inputs
            nrep = min(len(get(n, cf)['enc_wall_ns']) for n in ins)
            es = [sum(get(n, cf)['enc_wall_ns'][r] for n in ins) for r in range(nrep)]; ds = [sum(get(n, cf)['dec_wall_ns'][r] for n in ins) for r in range(nrep)]
            rows.append([cf, f'{sum(e) / 1e6:.2f}', f'{q(es, .25) / 1e6:.2f}–{q(es, .75) / 1e6:.2f}', f'{tot_res / (sum(e) / 1e9) / 1e6:.0f}', f'{sum(ec) / 1e6:.2f}',
                         f'{sum(d) / 1e6:.2f}', f'{q(ds, .25) / 1e6:.2f}–{q(ds, .75) / 1e6:.2f}', f'{tot_res / (sum(d) / 1e9) / 1e6:.0f}', f'{sum(dc) / 1e6:.2f}'])
        K[reg] = dict(inputs=ins, residual_bytes=tot_res, rows=rows)
        table(f'Native kernel timing, regime {label}, real inputs (n={len(ins)}, {tot_res} residual bytes; residual stream -> blob -> residual stream; sum over inputs of per-input medians, 1 thread)',
              ['config', 'enc ms (wall)', 'enc IQR of per-rep sums', 'enc MB/s', 'enc ms (cpu)', 'dec ms (wall)', 'dec IQR', 'dec MB/s', 'dec ms (cpu)'], rows)
    S['kernel'] = K

    # ===== end-to-end timing =====
    E = {}
    for mode in ('fast', 'hc'):
        ins = [k[0] for k in frB if k[1] == mode and frB[k]['int_blocks'] and real(k[0])]
        if not ins: continue
        def m_(n, what, cf=''): v = e2e[(n, mode, what, cf)]; return (med([x[0] for x in v]), med([x[1] for x in v])) if v else (None, None)
        pc = sum(m_(n, 'prod_compress')[0] for n in ins); pd = sum(m_(n, 'prod_decompress_api')[0] for n in ins); ps = sum(m_(n, 'prod_decode_seq')[0] for n in ins)
        pl = sum(m_(n, 'prod_residual_leaf_reencode')[0] for n in ins); zc = sum(m_(n, 'zstd3_whole')[0] for n in ins); zd = sum(m_(n, 'zstd3_whole_decompress')[0] for n in ins)
        raw = sum(frB[(n, mode)]['bytes'] for n in ins); prodb = sum(frB[(n, mode)]['frame'] for n in ins); zb = sum(frB[(n, mode)]['zstd3_whole_input'] for n in ins)
        rows = [['production kolmo (unmodified)', prodb, f'{pc / 1e6:.1f}', f'{pd / 1e6:.2f}', f'{ps / 1e6:.2f}'], ['zstd -3 whole input (direct control)', zb, f'{zc / 1e6:.1f}', f'{zd / 1e6:.2f}', '-']]
        for cf in CONFS:
            enc = sum(m_(n, 'exp_residual_encode', cf)[0] for n in ins); dec = sum(m_(n, 'exp_decode_seq', cf)[0] for n in ins)
            t = sum(totB[(n, mode)][cf] for n in ins)
            rows.append([cf, t, f'{(pc - pl + enc) / 1e6:.1f}', '-', f'{dec / 1e6:.2f}'])
        E[mode] = dict(inputs=ins, raw=raw, prod_compress_ms=pc / 1e6, prod_leaf_reencode_ms=pl / 1e6, rows=rows)
        table(f'End-to-end, complete planner {mode}, real inputs with INT blocks (n={len(ins)}, {raw} B). Encode for experimental rows = production compress - production residual-leaf re-encode ({pl / 1e6:.1f} ms) + experimental residual encode (derived; the subtraction removes only the final leaf coding, not production leaf trials, so it overstates experimental encode). Decode = actual sequential single-thread reconstruction of every block.',
              ['pipeline', 'stored bytes', 'encode ms (wall)', 'decode API ms (1 thread)', 'decode sequential ms'], rows)
    S['e2e'] = E

    # ===== memory =====
    M = {}; rows = []
    ins = sorted({k[0] for k in mem})
    for op, cf in [('prod_compress', ''), ('prod_decompress_api', ''), ('prod_decode_seq', '')] + [(o, c) for c in CONFS for o in ('exp_residual_encode', 'exp_residual_decode', 'exp_decode_seq')]:
        v = [max(mem[(n, op, cf)]) for n in ins if mem[(n, op, cf)] and frB.get((n, 'fast'), {}).get('int_blocks') and real(n)]
        if v: M[f'{op}:{cf}'] = dict(max_kb=max(v), median_kb=med(v)); rows.append([op, cf or '-', max(v), med(v)])
    table('Peak memory, fast mode, real inputs with INT blocks: process RSS high-water above the RSS just before the operation (KiB; /proc clear_refs reset after malloc_trim; input, frame and output buffers pre-touched)',
          ['operation', 'config', 'max over inputs', 'median over inputs'], rows)
    S['memory'] = M
    (out / 'summary.json').write_text(json.dumps(S, indent=1) + '\n'); (out / 'tables.md').write_text('\n'.join(T) + '\n')
    print(f'wrote {out}/summary.json, {out}/tables.md')

if __name__ == '__main__': main(*sys.argv[1:4])
