#!/usr/bin/env python3
"""P2-E22 FRESH inputs (preregistered, PROTOCOL.md): rfordatascience/tidytuesday @ 54ea581e1352f6c55b9343feb77df0fc29c78621,
paths only (git ls-tree --name-only on a depth-1 blob-less clone). Candidates: data/YYYY/YYYY-MM-DD/<name>.csv whose
lower-cased path contains none of EXCLUDE. One file per week directory = the path with the smallest
sha256('p2e22|' + path); directories ordered by sha256('p2e22|' + dir); keep a file only if 100,000..5,000,000 bytes
(size is the only property inspected before measurement) until 24 are kept.
usage: prepare_e22.py WORKDIR OUT"""
import hashlib, json, pathlib, re, shutil, subprocess, sys
TT = ('https://github.com/rfordatascience/tidytuesday', '54ea581e1352f6c55b9343feb77df0fc29c78621')
EXCLUDE = ['gapminder', 'covtype', 'covertype', 'collegemsg', 'ghcn', 'owid', 'covid', 'backblaze', 'divvy', 'power', 'quake',
           'usgs', 'fsdd', 'mitdb', 'eeg', 'pageview', 'wiki', 'fivethirtyeight', '538', 'esc50', 'esc-50', 'plotly', 'tsne',
           'olympic', 'classic-rock', 'biopics', 'congress', 'police', 'nba', 'pollster', 'thanksgiving', 'goose', 'comic',
           'boxer', 'riddler', 'impeachment', 'mayweather', 'weather-check', 'twitter', 'sandy', 'food-world-cup', 'tenth-circuit', 'ncaa']
PAT = re.compile(r'^data/\d{4}/\d{4}-\d{2}-\d{2}/[^/]+\.csv$')
def h(s): return hashlib.sha256(('p2e22|' + s).encode()).hexdigest()
def git(*a, cwd=None): return subprocess.run(['git', *a], cwd=cwd, check=True, capture_output=True, text=True).stdout
def main():
    work, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]); work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True)
    d = work / 'tt'; git('init', '-q', str(d)); git('remote', 'add', 'origin', TT[0], cwd=d)
    git('fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', TT[1], cwd=d)
    names = [n for n in git('ls-tree', '-r', '--name-only', TT[1], cwd=d).splitlines() if PAT.match(n) and not any(e in n.lower() for e in EXCLUDE)]
    bydir = {}
    for n in names: bydir.setdefault(n.rsplit('/', 1)[0], []).append(n)
    rows, sizes, kept = [], {}, 0
    for dname in sorted(bydir, key=h):
        n = min(bydir[dname], key=h); git('checkout', '-q', TT[1], '--', n, cwd=d); p = d / n; sz = p.stat().st_size; sizes[n] = sz
        if not 100_000 <= sz <= 5_000_000: continue
        b = p.read_bytes(); fid = 'TT__' + dname.split('/')[-1] + '__' + pathlib.Path(n).stem[:40]
        shutil.copy2(p, out / 'inputs' / (fid + '.csv'))
        rows.append(dict(id=fid, family='TT:' + dname.split('/')[-1], view='csv', profile='csv', file=f'inputs/{fid}.csv', input_bytes=len(b),
                         input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=n)); kept += 1
        if kept == 24: break
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E22', source=dict(url=TT[0], commit=TT[1]), exclude=EXCLUDE,
                                                      candidate_dirs=len(bydir), size_checked=sizes, inputs=rows), indent=1))
    print(len(bydir), 'candidate week directories;', kept, 'fresh CSV kept;', len(sizes), 'size-checked')
if __name__ == '__main__': main()
