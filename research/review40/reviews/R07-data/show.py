import json,sys
for l in open(sys.argv[1]):
    l=l.strip()
    if not l.startswith('{'): print(l); continue
    r=json.loads(l)
    if 'runs' in r:
        x=r['runs'][0]; print(f"{r['label']:40s} wall {x['wall_s']:8.2f}s peak {x['peak_rss_kb']/1024:8.1f} MiB avg {x['avg_rss_kb']/1024:8.1f} MiB exit {x['exit']} load {r['load1']:.1f}")
    else: print(r)
