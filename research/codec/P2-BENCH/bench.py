#!/usr/bin/env python3
"""P2-BENCH: one table set from every committed N1-boundary run of 13 Nov 2026 (first runs; confirmation runs agree,
see each experiment). Per data group and pipeline: aggregate ratio (sum input / sum stored), geometric-mean ratio,
encode and decode throughput (sum input bytes / sum of per-input median wall seconds, complete CLI incl. process
start), median peak RSS of the measured process, and fit counts against the P2 rival on every pair.
Peak RSS is the child process (N1 rss_scope), not codec scratch alone."""
import json, lzma, math, pathlib, statistics, collections, sys
C = pathlib.Path(__file__).resolve().parent.parent
def load(exp, run='first'):
    R = json.loads(lzma.decompress((C / exp / 'results' / run / 'records.json.xz').read_bytes()))['records']; by = {}
    for r in R:
        if r.get('stored_bytes') is not None and r.get('medians'):
            pid = r['policy']['id']
            if pid.startswith('openzl_') and not pid.startswith('openzl_serial'): pid = 'openzl_' + pid.rsplit('_', 1)[1]   # typed or csv profile, per input
            r['policy']['id'] = pid; by.setdefault(r['input_id'], {})[pid] = r
    return by
def fits(r, o, key): return all(a[key] <= b[key] for a, b in zip(r['runs'], o['runs']))
def table(name, by, pols, select=lambda i, P: True, rival_speed='zstd_3', rival_arch=None, note=''):
    ins = [i for i, P in by.items() if select(i, P) and all(p in P for p in pols if p in P)]
    rows = []
    for p in pols:
        use = [i for i in ins if p in by[i]]
        if not use: continue
        tin = sum(by[i][p]['input_bytes'] for i in use); tout = sum(by[i][p]['stored_bytes'] for i in use)
        gm = math.exp(statistics.mean(math.log(by[i][p]['input_bytes'] / by[i][p]['stored_bytes']) for i in use))
        te = sum(by[i][p]['medians']['encode_wall_s'] for i in use); td = sum(by[i][p]['medians']['decode_wall_s'] for i in use)
        rssv = [by[i][p]['medians']['peak_memory_bytes'] / 2**20 for i in use]; rss = statistics.median(rssv); rmax = max(rssv)
        sp = sum(1 for i in use if rival_speed in by[i] and p != rival_speed and fits(by[i][p], by[i][rival_speed], 'speed_cost_s') and by[i][p]['stored_bytes'] < by[i][rival_speed]['stored_bytes'])
        ar = sum(1 for i in use if (o := by[i].get('openzl_9')) and p != 'openzl_9' and fits(by[i][p], o, 'archive_cost_usd') and by[i][p]['stored_bytes'] < o['stored_bytes']) if rival_arch else None
        rows.append(dict(pipeline=p, n=len(use), ratio=round(tin / tout, 3), gmean_ratio=round(gm, 3), enc_MBps=round(tin / te / 1e6, 1), dec_MBps=round(tin / td / 1e6, 1),
                         median_peak_rss_MiB=round(rss, 1), max_peak_rss_MiB=round(rmax, 1), beats_zstd3_within_speed=sp, beats_openzl9_within_archive=ar))
    return dict(group=name, n_inputs=len(ins), note=note, rows=rows)
def ozl(P): return next(k for k in P if k.startswith('openzl_') and k.endswith('_9') and k != 'openzl_serial_9') if any(k.startswith('openzl_') and k.endswith('_9') and k != 'openzl_serial_9' for k in P) else next((k for k in P if k.startswith('openzl_')), None)
out = []
e17 = load('P2-E17'); e14 = load('P2-E14'); e10 = load('P2-E10')
fam = lambda P: P['zstd_3']['family']
GROUPS = [('Typed numeric arrays: all 61 development inputs (11 families)', lambda i, P: True),
          ('  audio / biosignals (FSDD WAV, speech, MIT-BIH ECG, EEG, dense EEG)', lambda i, P: any(k in fam(P) for k in ('fsdd', 'FSDD', 'speech', 'mitbih', 'MIT', 'eeg', 'EEG'))),
          ('  weather (GHCN daily)', lambda i, P: 'ghcn' in fam(P).lower()),
          ('  counts / telemetry (pageviews, Backblaze, OWID, controls)', lambda i, P: not any(k in fam(P).lower() for k in ('fsdd', 'speech', 'mitbih', 'mit-bih', 'eeg', 'ghcn')))]
for g, sel in GROUPS:
    out.append(table(g + ' — archive run P2-E17', e17, ['zstd_3', 'openzl_9', 'kolmo_fast', 'kolmo_hc', 'kolmo_max', 'tb_best', 'tp_pent', 'tp_arch19'],
                     sel, rival_arch=lambda P: ozl, note='OpenZL = typed le-* profile, level 9, whole file'))
    out.append(table(g + ' — speed run P2-E14', e14, ['zstd_3', 'kolmo_fast', 'tp_phuf_pg16', 'tp_phuf', 'tb_lazy', 'tb_best', 'tp_pent'], sel))
e20 = load('P2-E20')
out.append(table('Fresh audio: ESC-50 (40 clips, P2-E20)', e20, ['zstd_3', 'openzl_9', 'kolmo_fast', 'kolmo_hc', 'kolmo_max', 'tp_phuf_pg16', 'tb_best', 'tp_pent', 'tp_arch19'],
                 lambda i, P: P['zstd_3']['view'] == 'wav', rival_arch=lambda P: ozl))
e28 = load('P2-E28')
out.append(table('ESC-50 with the research fast-inverse decoder (P2-E28)', e28, ['zstd_3', 'kolmo_fast', 'tp_phuf_pg16', 'tp_phuf_pg16_fd', 'tb_lazy_fd']))
e29 = load('P2-E29')
out.append(table('Fresh audio: MS-SNSD noise + speech (40 files, P2-E29)', e29, ['zstd_3', 'kolmo_fast', 'tp_phuf_pg16', 'tp_phuf_pg16_fd', 'tb_lazy_fd']))
out.append(table('Real-world CSV: FiveThirtyEight (23, P2-E20)', e20, ['zstd_3', 'openzl_9', 'kolmo_fast', 'kolmo_hc', 'kolmo_max', 'mcsv_planes_huf', 'mcsv_prod19'],
                 lambda i, P: P['zstd_3']['view'] == 'csv' and P.get('openzl_9'), rival_arch=lambda P: ozl))
for exp, label in (('P2-E24', 'TidyTuesday 35 (P2-E24)'), ('P2-E26', 'TidyTuesday 35 (P2-E26, picker)'), ('P2-E27', 'selva86/dsdojo 28 (P2-E27, picker)')):
    b = load(exp); pols = ['zstd_3', 'openzl_9', 'kolmo_fast', 'kolmo_eco', 'tt_zstd12'] + (['pick'] if exp != 'P2-E24' else ['kolmo_hc', 'tt_bsc0'])
    out.append(table('Real-world CSV: ' + label, b, pols, lambda i, P: P.get('openzl_9') is not None, rival_arch=lambda P: ozl))
for exp, label in (('P2-E8', 'Regular numeric CSV tables: Covertype + CollegeMsg (P2-E8)'),):
    try:
        b = load(exp); pols = sorted({p for P in b.values() for p in P})
        out.append(table(label, b, pols, rival_arch=lambda P: ozl))
    except Exception as e: out.append(dict(group=label, error=str(e)))
json.dump(out, sys.stdout, indent=1)
