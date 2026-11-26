"""Find where a change wins: a depth-2 decision tree over cheap features, read as a rule list.

Each sample is one condition row (one file at one level and thread count) labelled win (PASS / KEEP-*) or lose
(FAIL / TRADE); NEUTRAL rows are left out. The tree is fitted on the dev split and scored on holdout. Splits are
one-vs-rest on a categorical value (kind == x, size class == x) or a threshold on an ordered one (size class <= x,
level <= x). Thread count is offered only when the change keeps every frame byte-identical: gates that change
output bytes may never depend on it (docs/ACCEPTANCE.md, section 4)."""

ORDERED = {"size_class": ["S0", "S1", "S2", "S3"], "level": [str(i) for i in range(1, 10)] + ["ultra"],
           "threads": ["1", "4", "10"]}


def _gini(rows):
    n = len(rows)
    if not n:
        return 0.0
    p = sum(r["win"] for r in rows) / n
    return 2 * p * (1 - p)


def _preds(rows, feats):
    out = []
    for f in feats:
        vals = sorted({r[f] for r in rows}, key=lambda v: ORDERED[f].index(v) if f in ORDERED else v)
        if len(vals) < 2:
            continue
        if f in ORDERED:
            for v in vals[:-1]:
                o = ORDERED[f]
                out.append((f"{f} <= {v}", lambda r, f=f, v=v, o=o: o.index(r[f]) <= o.index(v)))
        for v in vals:
            out.append((f"{f} == {v}", lambda r, f=f, v=v: r[f] == v))
    return out


def _best(rows, feats, min_leaf):
    base = _gini(rows)
    best = None
    for name, fn in _preds(rows, feats):
        yes = [r for r in rows if fn(r)]
        no = [r for r in rows if not fn(r)]
        if len(yes) < min_leaf or len(no) < min_leaf:
            continue
        g = (len(yes) * _gini(yes) + len(no) * _gini(no)) / len(rows)
        if best is None or g < best[0] - 1e-12:
            best = (g, name, fn)
    if best is None or best[0] >= base - 1e-9:
        return None
    return best[1], best[2]


def fit(rows, feats, depth=2, min_leaf=3):
    """-> list of leaves: (conditions[list of (name, fn, truth)], n, wins)"""
    leaves = []

    def grow(sub, conds, d):
        s = _best(sub, feats, min_leaf) if d < depth else None
        if s is None:
            leaves.append((conds, len(sub), sum(r["win"] for r in sub)))
            return
        name, fn = s
        grow([r for r in sub if fn(r)], conds + [(name, fn, True)], d + 1)
        grow([r for r in sub if not fn(r)], conds + [(name, fn, False)], d + 1)

    grow(rows, [], 0)
    return leaves


def leaf_text(conds):
    if not conds:
        return "always"
    return " and ".join(n if t else f"not ({n})" for n, _, t in conds)


def matches(conds, r):
    return all(fn(r) == t for _, fn, t in conds)


def score(leaves, rows):
    """holdout check: per leaf (n, wins) on `rows`, and accuracy of the majority prediction"""
    per = []
    correct = 0
    for conds, n, w in leaves:
        pred = w * 2 >= n
        sub = [r for r in rows if matches(conds, r)]
        hw = sum(r["win"] for r in sub)
        correct += hw if pred else len(sub) - hw
        per.append((len(sub), hw))
    return per, (correct / len(rows) if rows else None)
