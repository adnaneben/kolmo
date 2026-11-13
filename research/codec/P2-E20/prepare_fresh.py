#!/usr/bin/env python3
"""P2-E20: deterministic, metadata-only selection and download of FRESH source-disjoint inputs (frozen before reading).

Sources (pinned commits; neither is referenced anywhere in the repository before P2-E20):
  ESC-50   https://github.com/karolpiczak/ESC-50  @ 33c8ce9eb2cf0b1c2f8bcf322eb349b6be34dbb6  (CC BY-NC 3.0)
  538 data https://github.com/fivethirtyeight/data @ 4c1ff5e3aef1816ae04af63218015066e186c147  (CC BY 4.0)
Selection uses only paths (git ls-tree --name-only on a blob-less clone; no file content is read):
  ESC-50: audio/{fold}-{src}-{take}-{target}.wav; family = target // 10 (5 ESC-50 major categories); per family the 8
          paths with the smallest sha256('p2e20|' + path), skipping a path whose source-clip id {src} is already chosen.
  538:    *.csv files; one per top-level dataset directory = the path with the smallest sha256('p2e20|' + path) in it;
          directories ordered by sha256('p2e20|' + dir); take files in that order, keeping a file only if its size
          is 100,000..5,000,000 bytes (size is the only property inspected before measurement), until 24 are kept.
Output: N1-format prepared dir (manifest.json with id, family, view, profile, file, input_bytes, input_sha256).
OpenZL profile: le-i16 for ESC-50 WAV (whole file), csv for 538 CSV.
usage: prepare_fresh.py WORKDIR OUT"""
import hashlib, json, pathlib, shutil, subprocess, sys
ESC = ('https://github.com/karolpiczak/ESC-50', '33c8ce9eb2cf0b1c2f8bcf322eb349b6be34dbb6')
F38 = ('https://github.com/fivethirtyeight/data', '4c1ff5e3aef1816ae04af63218015066e186c147')
def h(s): return hashlib.sha256(('p2e20|' + s).encode()).hexdigest()
def git(*a, cwd=None): return subprocess.run(['git', *a], cwd=cwd, check=True, capture_output=True, text=True).stdout
def clone(url, sha, d):
    git('clone', '-q', '--filter=blob:none', '--no-checkout', url, str(d)); git('fetch', '-q', 'origin', sha, cwd=d)
    names = git('ls-tree', '-r', '--name-only', sha, cwd=d).splitlines(); return names
def fetch(d, sha, path):
    git('checkout', '-q', sha, '--', path, cwd=d); return d / path
def main():
    work, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]); work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True)
    rows, sel = [], {}
    d = work / 'esc'; names = [n for n in clone(*ESC, d) if n.startswith('audio/') and n.endswith('.wav')]
    fam = {}
    for n in names:
        parts = pathlib.Path(n).stem.split('-'); fam.setdefault(int(parts[3]) // 10, []).append((h(n), n, parts[1]))
    for f in sorted(fam):
        used, k = set(), 0
        for _, n, src in sorted(fam[f]):
            if src in used: continue
            used.add(src); k += 1; p = fetch(d, ESC[1], n); b = p.read_bytes()
            fid = f'ESC50__{pathlib.Path(n).stem}'; shutil.copy2(p, out / 'inputs' / (fid + '.wav'))
            rows.append(dict(id=fid, family=f'ESC50:major{f}', view='wav', profile='le-i16', file=f'inputs/{fid}.wav', input_bytes=len(b),
                             input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=n))
            if k == 8: break
    d = work / 'f38'; names = [n for n in clone(*F38, d) if n.endswith('.csv') and '/' in n]
    bydir = {}
    for n in names: bydir.setdefault(n.split('/')[0], []).append(n)
    kept = 0
    for dname in sorted(bydir, key=h):
        n = min(bydir[dname], key=h); p = fetch(d, F38[1], n); sz = p.stat().st_size
        sel[n] = sz
        if not 100_000 <= sz <= 5_000_000: continue
        b = p.read_bytes(); fid = 'F38__' + dname.replace(' ', '_')[:60]
        shutil.copy2(p, out / 'inputs' / (fid + '.csv'))
        rows.append(dict(id=fid, family=f'F38:{dname}', view='csv', profile='csv', file=f'inputs/{fid}.csv', input_bytes=len(b),
                         input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=n)); kept += 1
        if kept == 24: break
    m = dict(experiment='P2-E20', split='fresh_validation', sources=dict(esc50=dict(url=ESC[0], commit=ESC[1], license='CC BY-NC 3.0'),
             fivethirtyeight=dict(url=F38[0], commit=F38[1], license='CC BY 4.0')), size_checked=sel, inputs=rows)
    (out / 'manifest.json').write_text(json.dumps(m, indent=1))
    print(len([r for r in rows if r['view'] == 'wav']), 'ESC-50 WAV,', kept, '538 CSV,', sum(r['input_bytes'] for r in rows), 'bytes')
if __name__ == '__main__': main()
