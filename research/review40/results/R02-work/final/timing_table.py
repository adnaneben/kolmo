# R02: table of timing.jsonl (measure.py rows): best wall of 3, peak / average RSS of the best-RSS run, load average.
import json, sys, collections
rows = collections.OrderedDict()
for l in open(sys.argv[1]):
    d = json.loads(l); lab = d['label'].split('-', 4)            # R02, who, mode, op, file
    key = (lab[4], lab[2]); rows.setdefault(key, {})[(lab[1], lab[3])] = d
print("file\tmode\tbase c s\tkrans c s\tbase d s\tkrans d s\tbase peak RSS KiB c/d\tkrans peak RSS KiB c/d\tload1")
for (f, m), r in rows.items():
    g = lambda w, o: r[(w, o)]
    pk = lambda w, o: max(x['peak_rss_kb'] for x in g(w, o)['runs'])
    print(f"{f}\t{m}\t{g('base','compress')['best_wall_s']:.4f}\t{g('krans','compress')['best_wall_s']:.4f}\t{g('base','decompress')['best_wall_s']:.4f}\t{g('krans','decompress')['best_wall_s']:.4f}\t{pk('base','compress')}/{pk('base','decompress')}\t{pk('krans','compress')}/{pk('krans','decompress')}\t{g('base','compress')['load1']:.1f}-{g('krans','decompress')['load1']:.1f}")
