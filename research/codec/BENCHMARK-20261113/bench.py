#!/usr/bin/env python3
"""Consolidated benchmark of everything measured on 12-13 Nov 2026 (N1 complete-CLI boundary, GitHub ubuntu runners,
single thread, pinned zstd 1.5.7 and OpenZL v0.3.0). Reads only committed first-run records. Per cohort and pipeline:
  ratio_total = sum(input)/sum(stored); ratio_gmean = geometric mean of per-file ratios;
  enc/dec MB/s = median over files of input_bytes / median wall time (complete CLI incl. process start and I/O);
  mem MB = median over files of the median peak_memory_bytes (N1's per-pair peak, MiB);
  speed_win = fits zstd-3's speed cost on every pair AND smaller; archive_win = fits OpenZL's archive cost on every pair
  AND smaller (only where the cohort has an OpenZL typed/csv level-9 result). Each cohort uses distinct inputs."""
import json, lzma, math, pathlib, statistics, sys
ROOT = pathlib.Path(__file__).resolve().parent.parent
COHORTS = [
 ('A. Typed numeric arrays (NPY), development, 50 inputs / 9 families', [('P2-E17', 'npy'), ('P2-E14', 'npy')]),
 ('B. Whole WAV (FSDD speech), development, 10', [('P2-E17', 'wav'), ('P2-E14', 'wav')]),
 ('C1. Fresh audio ESC-50, 40', [('P2-E20', 'wav')]),
 ('C2. Fresh audio MS-SNSD, 40 (speed rival only)', [('P2-E29', 'wav')]),
 ('D. Regular numeric CSV tables (Covertype, CollegeMsg), development, 2', [('P2-E8', 'csv'), ('P2-E7', 'csv')]),
 ('E1. Fresh real-world CSV, FiveThirtyEight, 24', [('P2-E20', 'csv')]),
 ('E2. Fresh real-world CSV, TidyTuesday + selva86/dsdojo, 158 (P2-E22/23/24/26/27)', [('P2-E22', 'csv'), ('P2-E23', 'csv'), ('P2-E24', 'csv'), ('P2-E26', 'csv'), ('P2-E27', 'csv')]),
]
def load(exp):
    R = json.loads(lzma.decompress((ROOT / exp / 'results/first/records.json.xz').read_bytes()))['records']; by = {}
    for r in R: by.setdefault(r['input_id'], {})[r['policy']['id']] = r
    return by
def med(xs): return statistics.median(xs) if xs else None
out = {}
for title, parts in COHORTS:
    rows = {}  # policy -> list of per-file dicts
    seen = set()  # (input_id, policy): a policy measured in several runs of the same input counts once (first listed)
    for exp, view in parts:
        by = load(exp)
        for i, P in by.items():
            z = P['zstd_3']
            if z['view'] != view: continue
            o = next((P[p] for p in P if p.startswith('openzl_') and p.endswith('_9') and p not in ('openzl_serial_9', 'openzl_numpy_9') and P[p].get('stored_bytes')), None)
            for p, r in P.items():
                if r.get('stored_bytes') is None or not r.get('runs'): continue
                name = 'openzl_typed_9' if (p.startswith('openzl_') and p.endswith('_9') and p not in ('openzl_serial_9', 'openzl_numpy_9')) else p
                if p.startswith('openzl_') and name == p: continue
                if (i, name) in seen: continue
                seen.add((i, name))
                sw = all(a['speed_cost_s'] <= b['speed_cost_s'] for a, b in zip(r['runs'], z['runs'])) and r['stored_bytes'] < z['stored_bytes']
                aw = None if o is None else (all(a['archive_cost_usd'] <= b['archive_cost_usd'] for a, b in zip(r['runs'], o['runs'])) and r['stored_bytes'] < o['stored_bytes'])
                rows.setdefault(name, []).append(dict(n=r['input_bytes'], s=r['stored_bytes'], e=med([a['encode_wall_s'] for a in r['runs']]),
                    d=med([a['decode_wall_s'] for a in r['runs']]), m=med([a['peak_memory_bytes'] for a in r['runs'] if a.get('peak_memory_bytes')]), sw=sw, aw=aw))
    tab = {}
    for p, L in rows.items():
        tab[p] = dict(files=len(L), ratio_total=round(sum(x['n'] for x in L) / sum(x['s'] for x in L), 3),
            ratio_gmean=round(math.exp(statistics.mean(math.log(x['n'] / x['s']) for x in L)), 3),
            enc_MBps=round(med([x['n'] / x['e'] / 1e6 for x in L]), 1), dec_MBps=round(med([x['n'] / x['d'] / 1e6 for x in L]), 1),
            enc_agg_MBps=round(sum(x['n'] for x in L) / sum(x['e'] for x in L) / 1e6, 1), dec_agg_MBps=round(sum(x['n'] for x in L) / sum(x['d'] for x in L) / 1e6, 1),
            mem_max_MiB=round(max(x['m'] for x in L if x['m']) / 2**20, 1) if any(x['m'] for x in L) else None,
            mem_MiB=round(med([x['m'] for x in L if x['m']]) / 2**20, 1) if any(x['m'] for x in L) else None,
            speed_wins=f"{sum(x['sw'] for x in L)}/{len(L)}" if p != 'zstd_3' else '—',
            archive_wins=(f"{sum(bool(x['aw']) for x in L)}/{sum(x['aw'] is not None for x in L)}" if any(x['aw'] is not None for x in L) and p != 'openzl_typed_9' else '—'))
    out[title] = tab
json.dump(out, open(pathlib.Path(__file__).with_name('bench.json'), 'w'), indent=1)
NAMES = {'zstd_3': 'zstd -3 (speed rival)', 'zstd_19': 'zstd -19', 'openzl_typed_9': 'OpenZL typed/csv -9 (archive rival)', 'kolmo_fast': 'Kolmo fast (production)',
 'kolmo_fast_quick': 'Kolmo fast -q', 'kolmo_eco': 'Kolmo eco (production)', 'kolmo_hc': 'Kolmo hc (production)', 'kolmo_max': 'Kolmo max (production)',
 'kolmo_ultra_l1': 'Kolmo ultra -l1'}
order = ['zstd_3', 'zstd_19', 'openzl_typed_9', 'kolmo_fast', 'kolmo_fast_quick', 'kolmo_eco', 'kolmo_hc', 'kolmo_max', 'kolmo_ultra_l1']
for title, tab in out.items():
    print(f'\n### {title}\n\n| Pipeline | Ratio (total) | Ratio (geomean) | Encode MB/s (median file / aggregate) | Decode MB/s (median file / aggregate) | Peak RSS MiB (median / max) | Beats zstd-3 within speed cost | Beats OpenZL within archive cost |\n|---|---|---|---|---|---|---|---|')
    for p in sorted(tab, key=lambda p: (order.index(p) if p in order else 99, -tab[p]['ratio_total'])):
        t = tab[p]; print(f"| {NAMES.get(p, p)} | {t['ratio_total']} | {t['ratio_gmean']} | {t['enc_MBps']} / {t['enc_agg_MBps']} | {t['dec_MBps']} / {t['dec_agg_MBps']} | {t['mem_MiB']} / {t['mem_max_MiB']} | {t['speed_wins']} | {t['archive_wins']} |")
