"""Summarise matched rounds. Report every cell and the same-binary noise control."""
import csv
from collections import defaultdict
from statistics import mean, median
from pathlib import Path
d = Path(__file__).resolve().parent
cells = defaultdict(lambda: defaultdict(dict))
for r in csv.DictReader((d/'raw.csv').open()):
    if int(r['round']) > 0:
        key = (r['file'], int(r['bytes']), r['level'], r['threads'])
        cells[key][r['round']][r['arm']] = r
with (d/'summary.csv').open('w', newline='') as f:
    w = csv.writer(f)
    w.writerow('file bytes level threads rounds stored_bytes encode_cpu_delta_pct encode_wall_delta_pct baseline_combined_MB_s candidate_combined_MB_s paired_combined_gain_pct control_combined_gain_pct'.split())
    for key, rounds in cells.items():
        results = defaultdict(list)
        for triplet in rounds.values():
            assert set(triplet) == {'A', 'A2', 'B'}
            assert len({r['frame_sha256'] for r in triplet.values()}) == 1
            for field in ('enc_cpu_ns', 'enc_ns'):
                results[field].append(100*(int(triplet['B'][field])/mean(int(triplet[a][field]) for a in ('A','A2'))-1))
            times = {a:int(r['enc_ns'])+int(r['dec_ns']) for a,r in triplet.items()}
            tbase = mean([times['A'], times['A2']])
            results['base'].append(tbase)
            results['candidate'].append(times['B'])
            results['gain'].append(100*(tbase/times['B']-1))
            results['noise'].append(100*(times['A']/times['A2']-1))
        w.writerow([*key,len(rounds),triplet['A']['stored'],median(results['enc_cpu_ns']),median(results['enc_ns']),
                    key[1]*1000/median(results['base']), key[1]*1000/median(results['candidate']),
                    median(results['gain']),median(results['noise'])])
