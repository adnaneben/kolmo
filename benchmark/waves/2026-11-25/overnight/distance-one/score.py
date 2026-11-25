import csv
from collections import defaultdict
from statistics import mean, median
from pathlib import Path
root = Path(__file__).resolve().parent
records = defaultdict(lambda:defaultdict(dict))
for r in csv.DictReader((root/'raw.csv').open()):
    if int(r['round']):
        records[r['name'],r['level']][r['round']][r['arm']] = r
with (root/'score.csv').open('w',newline='') as out:
    w=csv.writer(out)
    w.writerow('source level pairs stored_bytes main_dec_MB_s candidate_dec_MB_s paired_dec_gain_pct control_dec_gain_pct main_combined_MB_s candidate_combined_MB_s paired_combined_gain_pct'.split())
    for (src,level), reps in records.items():
        ds, ctrl, combined, base_d, test_d, base_t, test_t = [],[],[],[],[],[],[]
        for triplet in reps.values():
            assert len(triplet)==3 and len({r['frame_sha256'] for r in triplet.values()})==1
            times={k:int(r['dec_ns']) for k,r in triplet.items()}
            total={k:int(r['enc_ns'])+times[k] for k,r in triplet.items()}
            base_d.append(mean(times[k] for k in ('A','A2')))
            test_d.append(times['B'])
            base_t.append(mean(total[k] for k in ('A','A2')))
            test_t.append(total['B'])
            ds.append(100*(base_d[-1]/test_d[-1]-1))
            ctrl.append(100*(times['A']/times['A2']-1))
            combined.append(100*(base_t[-1]/test_t[-1]-1))
        data=triplet['A']
        speed=lambda ns:round(int(data['bytes'])*1000/median(ns),3)
        w.writerow([src,level,len(reps),data['stored'],speed(base_d),speed(test_d),round(median(ds),3),round(median(ctrl),3),speed(base_t),speed(test_t),round(median(combined),3)])
