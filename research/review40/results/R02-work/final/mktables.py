# R02: markdown tables for results/R02.md from the measured files of this directory (nothing is typed by hand).
import collections, json
def rd(p): return [l.rstrip('\n').split('\t') for l in open(p)]
def pct(a, b): return f"{100 * (a / b - 1):+.1f}%"
def n(v): return f"{int(v):,}"
img = {(r[0], r[1]): r for r in rd('img.tsv')}
plan = {(r[0], r[1]): r for r in rd('plan_img.tsv')}
riv = collections.defaultdict(dict)
for r in rd('../rivals.tsv'): riv[r[0]][r[1]] = int(r[3])
files = sorted({k[0] for k in img})
print("### T1 images per file\n")
print("| file | raw | rc3 balanced | R02 balanced | change | rc3 hc | R02 hc | rc3 ratio | R02 ratio | JPEG-LS | JXL e3 | JXL e9 |")
print("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|")
T = collections.Counter()
for f in files:
    b, h, r = img[(f, 'balanced')], img[(f, 'hc')], img[(f, 'ratio')]
    print(f"| {f} | {n(b[2])} | {n(b[3])} | {n(b[5])} | {pct(int(b[5]), int(b[3]))} | {n(h[3])} | {n(h[5])} | {n(r[3])} | {n(r[5])} | {n(riv[f]['jpegls'])} | {n(riv[f]['jxl_e3'])} | {n(riv[f]['jxl_e9'])} |")
    for m in ('speed', 'balanced', 'eco', 'hc', 'ratio'):
        T['raw'] += int(img[(f, m)][2]) if m == 'balanced' else 0; T['b_' + m] += int(img[(f, m)][3]); T['k_' + m] += int(img[(f, m)][5])
    for m in ('balanced', 'hc', 'ratio'): T['p_' + m] += int(plan[(f, m)][5])
    for c in riv[f]: T[c] += riv[f][c]
print(f"| **total (17)** | {n(T['raw'])} | {n(T['b_balanced'])} | {n(T['k_balanced'])} | {pct(T['k_balanced'], T['b_balanced'])} | {n(T['b_hc'])} | {n(T['k_hc'])} | {n(T['b_ratio'])} | {n(T['k_ratio'])} | {n(T['jpegls'])} | {n(T['jxl_e3'])} | {n(T['jxl_e9'])} |")
print("\n### T2 image totals per mode\n")
print("| mode | rc3 | R02 opt-in | change | R02 opt-in + planner patch (#59, not on the branch) | change |")
print("|---|---:|---:|---:|---:|---:|")
for m in ('speed', 'balanced', 'eco', 'hc', 'ratio'):
    p = f"{n(T['p_' + m])} | {pct(T['p_' + m], T['b_' + m])}" if T['p_' + m] else "not run | "
    print(f"| {m} | {n(T['b_' + m])} | {n(T['k_' + m])} | {pct(T['k_' + m], T['b_' + m])} | {p} |")
print("\n### T3 rivals on the 17 images (totals)\n")
print("| rival | total | rc3 balanced vs it | R02 balanced | R02 hc | rc3 ratio | R02 ratio | R02 balanced + #59 patch |")
print("|---|---:|---:|---:|---:|---:|---:|---:|")
for c, nm in (('png9', 'PNG level 9'), ('jpegls', 'JPEG-LS (CharLS)'), ('webp_lossless', 'WebP lossless (method 6)'), ('jxl_e3', 'JPEG XL e3'), ('jxl_e7', 'JPEG XL e7'), ('jxl_e9', 'JPEG XL e9')):
    print(f"| {nm} | {n(T[c])} | {pct(T['b_balanced'], T[c])} | {pct(T['k_balanced'], T[c])} | {pct(T['k_hc'], T[c])} | {pct(T['b_ratio'], T[c])} | {pct(T['k_ratio'], T[c])} | {pct(T['p_balanced'], T[c])} |")
cnt = collections.Counter()
for f in files:
    k = int(img[(f, 'balanced')][5]); b = int(img[(f, 'balanced')][3])
    cnt['krans<jpegls'] += k < riv[f]['jpegls']; cnt['base<jpegls'] += b < riv[f]['jpegls']; cnt['krans<jxl3'] += k < riv[f]['jxl_e3']; cnt['base<jxl3'] += b < riv[f]['jxl_e3']
print("\nfiles smaller than the rival at balanced:", dict(cnt))
print("\n### T4 16-bit images\n")
sm = {(r[0], r[1]): r for r in rd('silesia_modes.tsv')}; sb = {(r[0], r[1]): r for r in rd('silesia.tsv')}; r16 = collections.defaultdict(dict)
for r in rd('rivals16.tsv'): r16[r[0]][r[1]] = int(r[3])
x = {(r[0], r[1]): r for r in rd('x.tsv')}
print("| file | mode | rc3 | R02 opt-in | change | codec 34 frame read by rc3 |")
print("|---|---|---:|---:|---:|---|")
for f in ('mr', 'x-ray'):
    for m in ('speed', 'balanced', 'eco', 'hc', 'ratio', 'x'):
        r = sb[(f, m)] if m == 'balanced' else x[(f, m)] if m == 'x' else sm[(f, m)]
        print(f"| {f} | {'-x' if m == 'x' else m} | {n(r[3])} | {n(r[5])} | {pct(int(r[5]), int(r[3]))} | {r[7]} |")
print("\n| file | JPEG-LS | JXL e3 | JXL e7 | JXL e9 | R02 balanced vs JPEG-LS | R02 hc vs JPEG-LS | R02 hc vs JXL e3 |")
print("|---|---:|---:|---:|---:|---:|---:|---:|")
for f in ('mr', 'x-ray'):
    kb, kh = int(sb[(f, 'balanced')][5]), int(sm[(f, 'hc')][5]); q = r16[f]
    print(f"| {f} | {n(q['jpegls'])} | {n(q['jxl_e3'])} | {n(q['jxl_e7'])} | {n(q['jxl_e9'])} | {pct(kb, q['jpegls'])} | {pct(kh, q['jpegls'])} | {pct(kh, q['jxl_e3'])} |")
print("\n### T5 Silesia balanced\n")
print("| file | raw | rc3 | R02 without opt-in | R02 opt-in | change |"); print("|---|---:|---:|---|---:|---:|")
tb = tk = tr = 0
for r in rd('silesia.tsv'):
    print(f"| {r[0]} | {n(r[2])} | {n(r[3])} | {r[4]} | {n(r[5])} | {pct(int(r[5]), int(r[3]))} |"); tb += int(r[3]); tk += int(r[5]); tr += int(r[2])
print(f"| **total** | {n(tr)} | {n(tb)} | | {n(tk)} | {100 * (tk / tb - 1):+.2f}% |")
print("\n### T6 arrays and table\n")
print("| file | mode | rc3 | R02 opt-in | change |"); print("|---|---|---:|---:|---:|")
for r in rd('npy.tsv'): print(f"| {r[0]} | {r[1]} | {n(r[3])} | {n(r[5])} | {100 * (int(r[5]) / int(r[3]) - 1):+.2f}% |")
for r in rd('silesia_modes.tsv'):
    if r[0] in ('sao', 'osdb', 'xml'): print(f"| {r[0]} | {r[1]} | {n(r[3])} | {n(r[5])} | {100 * (int(r[5]) / int(r[3]) - 1):+.2f}% |")
print("\n### T7 timings (indicative)\n")
rows = collections.OrderedDict()
for l in open('timing.jsonl'):
    d = json.loads(l); lab = d['label'].split('-', 4); rows.setdefault((lab[4], lab[2]), {})[(lab[1], lab[3])] = d
raw = {r[0]: int(r[2]) for r in rd('img.tsv') + rd('silesia.tsv') + rd('npy.tsv')}
print("| file | mode | compress s rc3 -> R02 | decompress s rc3 -> R02 | decompress MB/s rc3 -> R02 | peak RSS KiB compress rc3 -> R02 | peak RSS KiB decompress rc3 -> R02 | load |")
print("|---|---|---|---|---|---|---|---|")
for (f, m), r in rows.items():
    g = lambda w, o: r[(w, o)]; pk = lambda w, o: max(x['peak_rss_kb'] for x in g(w, o)['runs'])
    bd, kd = g('base', 'decompress')['best_wall_s'], g('krans', 'decompress')['best_wall_s']
    print(f"| {f} | {m} | {g('base','compress')['best_wall_s']:.3f} -> {g('krans','compress')['best_wall_s']:.3f} | {bd:.4f} -> {kd:.4f} | {raw[f] / bd / 1e6:.0f} -> {raw[f] / kd / 1e6:.0f} | {n(pk('base','compress'))} -> {n(pk('krans','compress'))} | {n(pk('base','decompress'))} -> {n(pk('krans','decompress'))} | {g('base','compress')['load1']:.1f} |")
