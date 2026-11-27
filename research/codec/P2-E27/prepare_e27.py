#!/usr/bin/env python3
"""P2-E27 FRESH inputs from TWO aggregators not used before (preregistered, paths only):
  selva86/datasets         @ c943fa8851270387c44d26efb0f9c0aac1559e11
  datasciencedojo/datasets @ f0ccab6a7ceafdff780052166fb6fab3311398eb
Candidates: *.csv whose lower-cased path contains none of EXCLUDE (P2-E22's list + 'tidytuesday').
Dataset key = '<repo>:<parent dir>' for files in a sub-directory, '<repo>:<normalized stem>' for top-level files;
one file per key = smallest sha256('p2e27|' + repo + '/' + path). Keys ordered by sha256('p2e27|' + key); a file is kept
only if 100,000..5,000,000 bytes AND its normalized stem (lower-case, letters only) was not already kept from either
repository (cross-aggregator duplicates such as the same public dataset); first 36 kept.
usage: prepare_e27.py WORKDIR OUT"""
import hashlib, json, pathlib, re, shutil, subprocess, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / 'P2-E22'))
import prepare_e22 as p22
SRC = [('selva86', 'https://github.com/selva86/datasets', 'c943fa8851270387c44d26efb0f9c0aac1559e11'),
       ('dsdojo', 'https://github.com/datasciencedojo/datasets', 'f0ccab6a7ceafdff780052166fb6fab3311398eb')]
EXCLUDE = p22.EXCLUDE + ['tidytuesday']
N = 36
def h(s): return hashlib.sha256(('p2e27|' + s).encode()).hexdigest()
def norm(stem): return re.sub(r'[^a-z]', '', stem.lower())
def main():
    work, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]); work.mkdir(parents=True); (out / 'inputs').mkdir(parents=True)
    keys, where = {}, {}
    for tag, url, sha in SRC:
        d = work / tag; p22.git('init', '-q', str(d)); p22.git('remote', 'add', 'origin', url, cwd=d)
        p22.git('fetch', '-q', '--depth', '1', '--filter=blob:none', 'origin', sha, cwd=d); where[tag] = (d, sha)
        for n in p22.git('ls-tree', '-r', '--name-only', sha, cwd=d).splitlines():
            if not n.lower().endswith('.csv') or any(e in n.lower() for e in EXCLUDE): continue
            key = f'{tag}:' + (n.rsplit('/', 1)[0] if '/' in n else norm(pathlib.Path(n).stem))
            keys.setdefault(key, []).append((tag, n))
    rows, sizes, seen, kept = [], {}, set(), 0
    for key in sorted(keys, key=h):
        tag, n = min(keys[key], key=lambda t: h(t[0] + '/' + t[1])); d, sha = where[tag]
        p22.git('checkout', '-q', sha, '--', n, cwd=d); p = d / n; sz = p.stat().st_size; sizes[f'{tag}/{n}'] = sz
        st = norm(pathlib.Path(n).stem)
        if not 100_000 <= sz <= 5_000_000 or st in seen: continue
        seen.add(st); b = p.read_bytes(); fid = f'X__{tag}__' + re.sub(r'[^A-Za-z0-9_.-]', '_', pathlib.Path(n).stem)[:40] + '_' + h(key)[:6]
        shutil.copy2(p, out / 'inputs' / (fid + '.csv'))
        rows.append(dict(id=fid, family='X:' + key, view='csv', profile='csv', file=f'inputs/{fid}.csv', input_bytes=len(b),
                         input_sha256=hashlib.sha256(b).hexdigest(), origin='fresh', source_path=f'{tag}/{n}')); kept += 1
        if kept == N: break
    (out / 'manifest.json').write_text(json.dumps(dict(experiment='P2-E27', sources=[dict(tag=t, url=u, commit=s) for t, u, s in SRC], exclude=EXCLUDE,
                                                      candidate_keys=len(keys), size_checked=sizes, inputs=rows), indent=1))
    print(len(keys), 'candidate dataset keys;', kept, 'fresh CSV kept;', len(sizes), 'size-checked')
if __name__ == '__main__': main()
