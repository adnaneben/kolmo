#!/usr/bin/env python3
"""P2-E21 inputs. (a) DEVELOPMENT: the 24 FiveThirtyEight CSVs of P2-E20 (now observed), re-selected by P2-E20's frozen
prepare_fresh.py rule and verified against P2-E20's recorded hashes. (b) FRESH (preregistered, PROTOCOL.md):
plotly/datasets @ 0c447c47b757ad74edecab31f0d72f849d2e67c2, paths only: every *.csv whose lower-cased path contains none
of EXCLUDE; ordered by sha256('p2e21|' + path); keep a file only if 100,000..5,000,000 bytes; first 24 kept.
usage: prepare_e21.py WORKDIR OUT P2E20_MANIFEST_JSON"""
import hashlib, json, pathlib, shutil, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'P2-E20'))
import prepare_fresh as pf
PLOTLY = ('https://github.com/plotly/datasets', '0c447c47b757ad74edecab31f0d72f849d2e67c2')
EXCLUDE = ['gapminder', 'covtype', 'covertype', 'collegemsg', 'ghcn', 'owid', 'covid', 'backblaze', 'divvy', 'power', 'quake',
           'usgs', 'fsdd', 'mitdb', 'eeg', 'pageview', 'wiki', 'fivethirtyeight', '538', 'esc50', 'esc-50']
def h(s): return hashlib.sha256(('p2e21|' + s).encode()).hexdigest()
def main():
    work, out, old = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), json.loads(pathlib.Path(sys.argv[3]).read_text())
    work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True); rows = []
    d = work / 'f38'; pf.clone(*pf.F38, d)
    for r in old['inputs']:
        if r['view'] != 'csv': continue
        p = pf.fetch(d, pf.F38[1], r['source_path']); b = p.read_bytes()
        if hashlib.sha256(b).hexdigest() != r['input_sha256']: raise SystemExit('P2-E20 input changed: ' + r['id'])
        shutil.copy2(p, out / r['file']); rows.append(dict(r, origin='development_observed_in_P2-E20'))
    d = work / 'plotly'; names = [n for n in pf.clone(*PLOTLY, d) if n.lower().endswith('.csv') and not any(e in n.lower() for e in EXCLUDE)]
    kept = 0; sizes = {}
    for n in sorted(names, key=h):
        p = pf.fetch(d, PLOTLY[1], n); sz = p.stat().st_size; sizes[n] = sz
        if not 100_000 <= sz <= 5_000_000: continue
        b = p.read_bytes(); fid = 'PLOTLY__' + pathlib.Path(n).stem.replace(' ', '_')[:60] + '_' + h(n)[:6]
        shutil.copy2(p, out / 'inputs' / (fid + '.csv'))
        rows.append(dict(id=fid, family='PLOTLY:' + pathlib.Path(n).stem[:40], view='csv', profile='csv', file=f'inputs/{fid}.csv', input_bytes=len(b),
                         input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=n)); kept += 1
        if kept == 24: break
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E21', sources=dict(plotly=dict(url=PLOTLY[0], commit=PLOTLY[1])),
                                                      exclude=EXCLUDE, size_checked=sizes, inputs=rows), indent=1))
    print(sum(r['origin'] != 'fresh' for r in rows), 'development CSV,', kept, 'fresh plotly CSV')
if __name__ == '__main__': main()
