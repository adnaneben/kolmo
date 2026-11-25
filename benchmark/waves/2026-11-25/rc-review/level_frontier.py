"""Retrospective bounds on a zero-training v12 encoder portfolio; takes wave2.jsonl."""
import argparse
import collections
import csv
import json
import statistics
import sys

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('receipts')
    args = parser.parse_args()
    groups = collections.defaultdict(lambda: collections.defaultdict(list))
    with open(args.receipts) as src:
        for line in src:
            r = json.loads(line)
            if r['arm'] == 'wave2' and r['threads'] == 1:
                groups[(r['input'],r['input_sha256'])][str(r['level'])].append(r)
    w = csv.writer(sys.stdout)
    w.writerow(['input','input_sha256','baseline_level','choice','base_bytes','chosen_bytes','base_ms','chosen_ms','size_saved_pct','combined_speed_gain_pct','best_ratio_speed_product_level'])
    for (name,sha), levels in sorted(groups.items()):
        if len(levels) != 10:
            continue
        table = {}
        for level, trials in levels.items():
            assert len(trials) == 3
            assert len({r['frame_sha256'] for r in trials}) == 1
            median = statistics.median
            table[level] = (trials[0]['stored_bytes'], median((r['enc']['wall_s']+r['dec']['wall_s'])*1000 for r in trials),
                            median(r['enc']['wall_s'] for r in trials), median(r['dec']['wall_s'] for r in trials),
                            median(r['enc']['rss_bytes'] for r in trials), median(r['dec']['rss_bytes'] for r in trials))
        product_optimum = min(table,key=lambda k: table[k][0]*table[k][1])
        for base in sorted(table,key=lambda k: int(k) if k!='ultra' else 10):
            b = table[base]
            # Retain each phase and process-memory ceiling. Rank remaining choices by bytes, then paired total.
            choices=[k for k,v in table.items() if all(x<=y for x,y in zip(v,b))]
            choice=min(choices,key=lambda k:(table[k][:2],k!=base))
            a=table[choice]
            w.writerow([name,sha,base,choice,b[0],a[0],round(b[1],4),round(a[1],4),round(100*(1-a[0]/b[0]),4),round(100*(b[1]/a[1]-1),4),product_optimum])
if __name__ == '__main__':
    main()
