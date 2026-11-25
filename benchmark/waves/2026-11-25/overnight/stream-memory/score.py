"""Calculate cell medians and observed capacity ranges, keeping memory categories separate."""
import csv,json
from collections import defaultdict
from pathlib import Path
from statistics import median
p=Path(__file__).resolve().parent
meta=json.loads((p/'manifest.json').read_text())
for source,name,keys,metrics in [
    ('raw.csv','score.csv',['source','format','api','input_kind','threads'],
      ['compressed_input_peak_B','fd_index_B','fd_packed_workers_peak_B','stream_output_window_peak_B','tls_retained_after_B','caller_mapped_B','rss_peak_B','elapsed_ns']),
    ('cli.csv','cli-score.csv',['source','format','input_kind','threads'],['rss_peak_B','elapsed_ns'])]:
    groups=defaultdict(list)
    for r in csv.DictReader((p/source).open()):
        assert r['restored_sha256']==meta['inputs'][r['source']]['sha256']
        if 'frame_sha256' in r:assert r['frame_sha256']==meta['inputs'][r['source']]['frames'][r['format']]['sha256']
        groups[tuple(r[k] for k in keys)].append(r)
    with (p/name).open('w',newline='') as out:
        writer=csv.writer(out);writer.writerow([*keys,'repetitions',*(m+'_median' for m in metrics),'rss_peak_min_B','rss_peak_max_B'])
        for k,rs in sorted(groups.items()):
            assert len(rs)==3
            writer.writerow([*k,len(rs),*(median(int(r[m]) for r in rs) for m in metrics),min(int(r['rss_peak_B']) for r in rs),max(int(r['rss_peak_B']) for r in rs)])
