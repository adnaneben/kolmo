"""Data-structure models for compression (research prototype). Sizes are after xz -9e (raw LZMA2) unless noted.

  layout   CSV row-major (as is) vs column-major (one list per column)
  fdgraph  functional-dependency graph between CSV columns: if column A determines column B (A -> B for every row),
           B is stored as the mapping only
  wgraph   word-transition graph for text: each token is coded as its rank in the previous token's successor list
           (move-to-front adjacency lists), escapes coded as a global id
  trie     a sorted vocabulary as plain list vs front coding (shared prefix length + suffix), i.e. a flattened trie
usage: python3 dstruct.py layout|fdgraph file.csv ... | wgraph file.txt | trie file.txt
"""
import collections, csv, io, lzma, math, re, sys, time


def xz(b):
    return len(lzma.compress(b, format=lzma.FORMAT_RAW, filters=[{'id': lzma.FILTER_LZMA2, 'preset': 9 | lzma.PRESET_EXTREME}]))


def varints(vals):
    out = bytearray()
    for z in vals:
        while z >= 128:
            out.append((z & 127) | 128); z >>= 7
        out.append(z)
    return bytes(out)


def h0_bytes(vals):
    c = collections.Counter(vals); n = len(vals)
    return -sum(v * math.log2(v / n) for v in c.values()) / 8


def read_csv(path):
    raw = open(path, 'rb').read()
    rows = list(csv.reader(io.StringIO(raw.decode()), ))
    return raw, rows[0], rows[1:]


def layout(path):
    raw, header, body = read_csv(path)
    k = len(header)
    colmajor = '\n'.join('\x1e'.join(r[j] for r in body) for j in range(k)).encode()
    a, b = xz(raw), xz(colmajor)
    print(f'| {path.split("/")[-1]} | {len(raw):,} | {a:,} ({len(raw) / a:.2f}) | {b:,} ({len(raw) / b:.2f}) | {100 * (a - b) / a:+.1f}% |')


def fdgraph(path):
    raw, header, body = read_csv(path)
    k = len(header); n = len(body)
    cols = [[r[j] for r in body] for j in range(k)]
    card = [len(set(c)) for c in cols]
    edges = []
    for a in range(k):
        if card[a] > n / 2:
            continue
        for b in range(k):
            if a == b or card[b] <= 1:
                continue
            m = {}; ok = True
            for x, y in zip(cols[a], cols[b]):
                if m.setdefault(x, y) != y:
                    ok = False; break
            if ok and card[b] > 1:
                edges.append((a, b, m))
    print(f'\n### functional dependencies in {path.split("/")[-1]} ({n:,} rows)')
    saved = 0
    seen = set()
    for a, b, m in edges:
        if b in seen:
            continue
        seen.add(b)
        plain = xz('\n'.join(cols[b]).encode())
        mapped = xz('\n'.join(f'{x}\t{y}' for x, y in m.items()).encode())
        if mapped < plain:
            saved += plain - mapped
            print(f'  {header[a]} -> {header[b]}: column {plain:,} B -> mapping {mapped:,} B')
    print(f'  total saved vs per-column xz: {saved:,} B')


def tokens(text):
    return re.findall(r"[A-Za-z']+[^A-Za-z']*|[^A-Za-z']+", text)


def wgraph(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    raw = text.encode()
    toks = tokens(text)
    t0 = time.time()
    # baseline: global ids by first appearance (decoder learns the vocabulary in order)
    gid = {}; ids = []; vocab = []
    for t in toks:
        if t not in gid:
            gid[t] = len(gid); vocab.append(t)
        ids.append(gid[t])
    vocab_b = '\0'.join(vocab).encode()
    # graph: per previous token, a move-to-front successor list; rank 0 = most recent successor
    adj = collections.defaultdict(list)
    ranks = []; esc = []
    prev = None
    for t in toks:
        lst = adj[prev]
        try:
            r = lst.index(t)
            lst.pop(r); ranks.append(r + 1)
        except ValueError:
            ranks.append(0); esc.append(gid[t])
        lst.insert(0, t)
        prev = t
    el = time.time() - t0
    s_ids = xz(varints(ids)) + xz(vocab_b)
    s_graph = xz(varints(ranks)) + xz(varints(esc)) + xz(vocab_b)
    hits = sum(1 for r in ranks if r)
    top1 = sum(1 for r in ranks if r == 1)
    print(f'\n### word graph on {path.split("/")[-1]}: {len(raw):,} bytes, {len(toks):,} tokens, {len(vocab):,} distinct')
    print(f'  successor found in graph: {hits / len(toks):.1%}; rank 1 (repeat of the last successor): {top1 / len(toks):.1%}')
    print(f'  order-0 entropy: ids {h0_bytes(ids) / len(toks) * 8:.2f} bits/token, graph ranks {h0_bytes(ranks) / len(toks) * 8:.2f} bits/token')
    print(f'  xz raw text       : {xz(raw):,}  ratio {len(raw) / xz(raw):.3f}')
    print(f'  token ids + vocab : {s_ids:,}  ratio {len(raw) / s_ids:.3f}')
    print(f'  graph ranks + esc : {s_graph:,}  ratio {len(raw) / s_graph:.3f}   ({el:.1f} s python)')


def trie(path):
    text = open(path, encoding='utf-8', errors='replace').read()
    vocab = sorted(set(re.findall(r"[A-Za-z']+", text)))
    plain = '\n'.join(vocab).encode()
    fc = bytearray(); prev = ''
    for w in vocab:
        p = 0
        while p < min(len(w), len(prev)) and w[p] == prev[p]:
            p += 1
        fc.append(p); fc += w[p:].encode(); fc.append(10); prev = w
    print(f'\n### sorted vocabulary of {path.split("/")[-1]}: {len(vocab):,} words, {len(plain):,} bytes')
    print(f'  plain list   : raw {len(plain):,}  xz {xz(plain):,}')
    print(f'  front-coded  : raw {len(fc):,}  xz {xz(bytes(fc)):,}')


cmd = sys.argv[1]
if cmd == 'layout':
    print('| file | bytes | row-major xz (ratio) | column-major xz (ratio) | change |\n|---|---|---|---|---|')
for p in sys.argv[2:]:
    globals()[cmd](p)
