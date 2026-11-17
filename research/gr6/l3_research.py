"""Level-3 decision research: which repeated phrases are worth a dictionary entry?

For each file:
  1. code it with the dictionary-free match model (lzp) and log the bits spent on every field;
  2. mine phrase candidates and measure each one's saving: the bits its later occurrences cost under (1),
     minus the cost of coding its uses and of storing it in the dictionary (label);
  3. train decision makers on the other files (leave one file out) and select phrases for this file;
  4. code the file with each selection and record the real size and time.
Decision makers: all, oracle (true label, an upper bound), rule (deterministic estimate, constant tuned on the
training files), logistic regression, a small neural network (one hidden layer).

usage: python3 l3_research.py GR6L3 WORKDIR file...
"""
import csv, json, math, os, subprocess, sys
import numpy as np

G, W = sys.argv[1], sys.argv[2]
FILES = sys.argv[3:]
os.makedirs(W, exist_ok=True)
USE_BITS, DICT_BITS_PER_BYTE = 10.0, 3.5     # coding one use; one dictionary byte after zstd (measured: 7361 B / 2.1x)


def run(path, policy, env=None, threads=1):
    e = dict(os.environ, **(env or {}))
    out = subprocess.run([G, 'b', path, str(threads), policy], capture_output=True, text=True, env=e, check=True).stdout
    line = out.splitlines()[0]
    size = int(line.split(' -> ')[1].split()[0])
    tm = {k: float(line.split(f' {k} ')[1].split('s')[0]) for k in ('enc', 'dec', 'mine')}
    assert line.rstrip().endswith('OK'), line
    return size, tm, line


def features(rows):
    X = np.array([[math.log2(r['cnt']), r['n'], math.log2(1 + r['bytes']), r['sumsi'], r['maxsi'], r['sumsi'] / r['n'],
                   r['nnum'] / r['n'], r['nempty'] / r['n']] for r in rows], dtype=float)
    return X


def label(r):
    occ = max(r['occ'] - 1, 0)
    return r['truthbits'] - occ * USE_BITS - r['bytes'] * DICT_BITS_PER_BYTE


data = {}
for path in FILES:
    name = os.path.basename(path)
    fb, cf = f'{W}/{name}.fbits', f'{W}/{name}.cands.csv'
    base, tb, _ = run(path, 'lzp', {'FIELDBITS_OUT': fb})
    run(path, 'lzp+rule:1e18', {'FIELDBITS_IN': fb, 'CAND_OUT': cf})
    rows = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(open(cf))]
    none, tn, _ = run(path, 'none')
    data[name] = dict(path=path, rows=rows, base=base, tbase=tb, none=none, tnone=tn)
    y = np.array([label(r) for r in rows])
    print(f'{name}: none {none:,} B  lzp {base:,} B  candidates {len(rows):,}, worth it (label > 0): {int((y > 0).sum()):,}'
          f'  total positive saving {y[y > 0].sum() / 8:,.0f} B', flush=True)


def standardize(Xtr, Xte):
    mu, sd = Xtr.mean(0), Xtr.std(0) + 1e-9
    return (Xtr - mu) / sd, (Xte - mu) / sd


def logreg(X, y, Xte, it=600, lr=0.1, l2=1e-3, w8=None):
    w = np.zeros(X.shape[1]); b = 0.0; w8 = np.ones(len(y)) if w8 is None else w8
    for _ in range(it):
        p = 1 / (1 + np.exp(-(X @ w + b))); g = (p - y) * w8
        w -= lr * (X.T @ g / len(y) + l2 * w); b -= lr * g.mean()
    return 1 / (1 + np.exp(-(Xte @ w + b)))


def mlp(X, y, Xte, h=8, it=1500, lr=0.05, seed=1, w8=None):
    rng = np.random.default_rng(seed); w8 = np.ones(len(y)) if w8 is None else w8
    W1 = rng.normal(0, 0.5, (X.shape[1], h)); b1 = np.zeros(h); W2 = rng.normal(0, 0.5, h); b2 = 0.0
    for _ in range(it):
        H = np.tanh(X @ W1 + b1); p = 1 / (1 + np.exp(-(H @ W2 + b2))); g = (p - y) * w8 / len(y)
        gH = np.outer(g, W2) * (1 - H ** 2)
        W2 -= lr * H.T @ g; b2 -= lr * g.sum(); W1 -= lr * X.T @ gH; b1 -= lr * gH.sum(0)
    H = np.tanh(Xte @ W1 + b1)
    return 1 / (1 + np.exp(-(H @ W2 + b2)))


results = {}
for name, D in data.items():
    tr = [r for k, v in data.items() if k != name for r in v['rows']]
    te = D['rows']
    if not te:
        continue
    Xtr, Xte = standardize(features(tr), features(te))
    ytr = np.array([label(r) for r in tr]); yte = np.array([label(r) for r in te])
    w8 = np.minimum(np.abs(ytr) / 64 + 0.05, 20)                 # weight examples by how many bits are at stake
    # deterministic rule: saving = (cnt-1)*alpha*sumsi - cnt*USE_BITS - DICT*bytes > 0; alpha tuned on the training files
    def rule_sel(rows, alpha):
        return np.array([(r['cnt'] - 1) * alpha * r['sumsi'] - r['cnt'] * USE_BITS - DICT_BITS_PER_BYTE * r['bytes'] > 0 for r in rows])
    best_a, best_v = 0.0, -1e18
    for a in np.linspace(0.05, 1.5, 30):
        s = rule_sel(tr, a); v = ytr[s].sum()
        if v > best_v: best_a, best_v = a, v
    sel = {
        'all': np.ones(len(te), bool),
        'oracle': yte > 0,
        'rule': rule_sel(te, best_a),
        'logreg': logreg(Xtr, (ytr > 0).astype(float), Xte, w8=w8) > 0.5,
        'mlp': mlp(Xtr, (ytr > 0).astype(float), Xte, w8=w8) > 0.5,
    }
    res = {'none': (D['none'], D['tnone']), 'lzp': (D['base'], D['tbase'])}
    for k, s in sel.items():
        idx = [int(r['idx']) for r, keep in zip(te, s) if keep]
        f = f'{W}/{name}.sel.{k}'; open(f, 'w').write('\n'.join(map(str, idx)))
        if not idx:
            res[k] = (D['base'], D['tbase']); continue
        size, tm, _ = run(D['path'], f'lzp+sel:{f}')
        res[k] = (size, tm)
    results[name] = res
    p = {k: int(s.sum()) for k, s in sel.items()}
    print(f'\n{name} (rule alpha {best_a:.2f}; phrases kept {p})')
    for k, (size, tm) in res.items():
        print(f'  {k:7s} {size:>10,} B  ratio {os.path.getsize(D["path"]) / size:7.3f}  mine {tm["mine"]:.2f}s enc {tm["enc"]:.2f}s dec {tm["dec"]:.2f}s', flush=True)

json.dump({k: {p: [v[0], v[1]] for p, v in r.items()} for k, r in results.items()}, open(f'{W}/l3_results.json', 'w'), indent=1)
