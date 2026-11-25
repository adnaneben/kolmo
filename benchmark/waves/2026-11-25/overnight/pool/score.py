"""Tabulate medians across fresh processes and paired release minus retain differences."""
import csv
from pathlib import Path
from statistics import median
from collections import defaultdict
out=Path(__file__).resolve().parent
raw=list(csv.DictReader((out/'public.csv').open()))
samples=defaultdict(list)
for row in raw:
    if row['role']=='parent':
        samples[row['stage'], row['policy'], int(row['pair'])].append(row)
stages=['small-cold-T1','large-first-T4','large-repeat-T4','small-after-T4','policy','small-after-policy','large-after-policy','small-final-T1','final-release','double-release']
def total(r,key,prior=None):
    return sum(int(x[key])-(int(x[prior]) if prior else 0) for x in r)
def metrics(r):
    return {'wall_us':total(r,'elapsed_ns')/1000,'cpu_us':total(r,'cpu_us'),
        'minor_faults':total(r,'minor_after','minor_before'),'major_faults':total(r,'major_after','major_before'),
        'rss_before_B':int(r[0]['rss_before']),'rss_after_B':int(r[-1]['rss_after']),
        'current_rss_delta_B':total(r,'rss_after','rss_before'),
        'peak_before_B':int(r[0]['peak_before']),'peak_after_B':int(r[-1]['peak_after']),
        'footprint_after_B':int(r[-1]['footprint_after']),
        'peak_delta_B':total(r,'peak_after','peak_before')}
result=[]
pairs=len({int(x['pair']) for x in raw})
for stage in stages:
    arms={arm:[metrics(samples[stage,arm,p]) for p in range(pairs)] for arm in ('retain','release')}
    for arm,xs in arms.items():
        result.append({'stage':stage,'quantity':arm,'pairs':pairs,**{k:median(x[k] for x in xs) for k in xs[0]}})
    result.append({'stage':stage,'quantity':'paired-release-minus-retain','pairs':pairs,**{
            k:median(a[k]-b[k] for a,b in zip(arms['release'],arms['retain'])) for k in arms['retain'][0]}})
with (out/'score.csv').open('w',newline='') as f:
    w=csv.DictWriter(f,fieldnames=result[0]);w.writeheader();w.writerows(result)
