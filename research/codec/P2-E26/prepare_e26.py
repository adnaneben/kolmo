#!/usr/bin/env python3
"""P2-E26 FRESH inputs (preregistered): rfordatascience/tidytuesday @ 54ea581e... (as P2-E22/E23), excluding EVERY
weekly directory size-checked by P2-E22, P2-E23 or P2-E24 (their manifests' size_checked keys) and P2-E22's exclusion terms.
Paths only; salt 'p2e26|'; one file per week directory; directories ordered by hash; 100,000..5,000,000 bytes; first 36.
usage: prepare_e26.py WORKDIR OUT P2E22_MANIFEST P2E23_MANIFEST P2E24_MANIFEST"""
import hashlib, json, pathlib, shutil, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'P2-E22'))
import prepare_e22 as p22
N = 36
def h(s): return hashlib.sha256(('p2e26|' + s).encode()).hexdigest()
def main():
    work, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    used = set()
    for m in sys.argv[3:6]: used |= {n.rsplit('/', 1)[0] for n in json.loads(pathlib.Path(m).read_text())['size_checked']}
    work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True)
    d = work / 'tt'; p22.git('init', '-q', str(d)); p22.git('remote', 'add', 'origin', p22.TT[0], cwd=d)
    p22.git('fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', p22.TT[1], cwd=d)
    names = [n for n in p22.git('ls-tree', '-r', '--name-only', p22.TT[1], cwd=d).splitlines()
             if p22.PAT.match(n) and not any(e in n.lower() for e in p22.EXCLUDE) and n.rsplit('/', 1)[0] not in used]
    bydir = {}
    for n in names: bydir.setdefault(n.rsplit('/', 1)[0], []).append(n)
    rows, sizes, kept = [], {}, 0
    for dname in sorted(bydir, key=h):
        n = min(bydir[dname], key=h); p22.git('checkout', '-q', p22.TT[1], '--', n, cwd=d); p = d / n; sz = p.stat().st_size; sizes[n] = sz
        if not 100_000 <= sz <= 5_000_000: continue
        b = p.read_bytes(); fid = 'TT__' + dname.split('/')[-1] + '__' + pathlib.Path(n).stem[:40]
        shutil.copy2(p, out / 'inputs' / (fid + '.csv'))
        rows.append(dict(id=fid, family='TT:' + dname.split('/')[-1], view='csv', profile='csv', file=f'inputs/{fid}.csv', input_bytes=len(b),
                         input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=n)); kept += 1
        if kept == N: break
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E26', source=dict(url=p22.TT[0], commit=p22.TT[1]), excluded_dirs=sorted(used),
                                                      candidate_dirs=len(bydir), size_checked=sizes, inputs=rows), indent=1))
    print(len(bydir), 'candidate week directories (P2-E22/E23/E24 dirs excluded);', kept, 'fresh CSV kept;', len(sizes), 'size-checked')
if __name__ == '__main__': main()
