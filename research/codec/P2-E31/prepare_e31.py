#!/usr/bin/env python3
"""P2-E31 FRESH audio (preregistered, paths only; neither repository is referenced in this repo before P2-E31):
  soerenab/AudioMNIST    @ 630d7dab4c040882834de6fa21baf9a60372accd : key = speaker directory; up to 30 files
  iver56/audiomentations @ f5e45049dfa8b33e8ee79a900b2b796a9dcae45f : key = parent dir + stem without digits; up to 10
Per source: *.wav candidates; one file per key = smallest sha256('p2e31|' + repo + path); keys ordered by
sha256('p2e31|' + key); keep a file only if 20,000..5,000,000 bytes. OpenZL profile le-i16, view 'wav'.
usage: prepare_e31.py WORKDIR OUT"""
import hashlib, json, pathlib, re, shutil, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'P2-E22'))
import prepare_e22 as p22
SRC = [('amnist', 'https://github.com/soerenab/AudioMNIST', '630d7dab4c040882834de6fa21baf9a60372accd', 30),
       ('augm', 'https://github.com/iver56/audiomentations', 'f5e45049dfa8b33e8ee79a900b2b796a9dcae45f', 10)]
def h(s): return hashlib.sha256(('p2e31|' + s).encode()).hexdigest()
def main():
    work, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]); work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True)
    rows, sizes = [], {}
    for tag, url, sha, cap in SRC:
        d = work / tag; p22.git('init', '-q', str(d)); p22.git('remote', 'add', 'origin', url, cwd=d)
        p22.git('fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', sha, cwd=d)
        keys = {}
        for n in p22.git('ls-tree', '-r', '--name-only', sha, cwd=d).splitlines():
            if not n.lower().endswith('.wav'): continue
            parent = n.rsplit('/', 1)[0] if '/' in n else '.'
            key = f'{tag}:{parent}' if tag == 'amnist' else f'{tag}:{parent}:' + re.sub(r'[\d_\-\s]+', '', pathlib.Path(n).stem.lower())
            keys.setdefault(key, []).append(n)
        kept = 0
        for key in sorted(keys, key=h):
            n = min(keys[key], key=lambda p: h(tag + '/' + p)); p22.git('checkout', '-q', sha, '--', n, cwd=d); p = d / n; sz = p.stat().st_size; sizes[f'{tag}/{n}'] = sz
            if not 20_000 <= sz <= 5_000_000: continue
            b = p.read_bytes(); fid = f'A31__{tag}__' + re.sub(r'[^A-Za-z0-9_.-]', '_', pathlib.Path(n).stem)[:40] + '_' + h(key)[:6]
            shutil.copy2(p, out / 'inputs' / (fid + '.wav'))
            rows.append(dict(id=fid, family=f'A31:{tag}', view='wav', profile='le-i16', file=f'inputs/{fid}.wav', input_bytes=len(b),
                             input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=f'{tag}/{n}')); kept += 1
            if kept == cap: break
        print(tag, len(keys), 'keys;', kept, 'kept')
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E31', sources=[dict(tag=t, url=u, commit=s) for t, u, s, _ in SRC], size_checked=sizes, inputs=rows), indent=1))
if __name__ == '__main__': main()
