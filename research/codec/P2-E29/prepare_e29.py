#!/usr/bin/env python3
"""P2-E29 FRESH audio (preregistered, paths only): microsoft/MS-SNSD @ fe61c4ba0d9ac8dd7e23d719cc79f8947e1dc742
(never used before). Candidates: *.wav. Key = '<top dir>:<stem without trailing _digits / digits>' (one per noise
type or speaker group); one file per key = smallest sha256('p2e29|' + path); keys ordered by sha256('p2e29|' + key);
keep a file only if 100,000..5,000,000 bytes; first 40 kept. OpenZL profile le-i16 (whole file), view 'wav'.
usage: prepare_e29.py WORKDIR OUT"""
import hashlib, json, pathlib, re, shutil, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'P2-E22'))
import prepare_e22 as p22
SRC = ('https://github.com/microsoft/MS-SNSD', 'fe61c4ba0d9ac8dd7e23d719cc79f8947e1dc742'); N = 40
def h(s): return hashlib.sha256(('p2e29|' + s).encode()).hexdigest()
def main():
    work, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]); work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True)
    d = work / 'snsd'; p22.git('init', '-q', str(d)); p22.git('remote', 'add', 'origin', SRC[0], cwd=d)
    p22.git('fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', SRC[1], cwd=d)
    keys = {}
    for n in p22.git('ls-tree', '-r', '--name-only', SRC[1], cwd=d).splitlines():
        if not n.lower().endswith('.wav'): continue
        top = n.split('/')[0] if '/' in n else '.'; stem = re.sub(r'[_\-\s]*\d+$', '', pathlib.Path(n).stem)
        keys.setdefault(f'{top}:{stem}', []).append(n)
    rows, sizes, kept = [], {}, 0
    for key in sorted(keys, key=h):
        n = min(keys[key], key=h); p22.git('checkout', '-q', SRC[1], '--', n, cwd=d); p = d / n; sz = p.stat().st_size; sizes[n] = sz
        if not 100_000 <= sz <= 5_000_000: continue
        b = p.read_bytes(); fid = 'SNSD__' + re.sub(r'[^A-Za-z0-9_.-]', '_', pathlib.Path(n).stem)[:40] + '_' + h(key)[:6]
        shutil.copy2(p, out / 'inputs' / (fid + '.wav'))
        rows.append(dict(id=fid, family='SNSD:' + key.split(':')[0], view='wav', profile='le-i16', file=f'inputs/{fid}.wav', input_bytes=len(b),
                         input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=n)); kept += 1
        if kept == N: break
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E29', source=dict(url=SRC[0], commit=SRC[1]), candidate_keys=len(keys),
                                                      size_checked=sizes, inputs=rows), indent=1))
    print(len(keys), 'candidate keys;', kept, 'fresh WAV kept;', len(sizes), 'size-checked')
if __name__ == '__main__': main()
