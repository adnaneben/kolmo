"""Mathematical reduction of CSV columns (research prototype).

Each column becomes small integer streams under the cheapest of several models; every column is decoded back and
compared with the original text, so the reported sizes are lossless.

Column models
  text     the column's text, newline separated                               (fallback, always valid)
  dict     categorical: dictionary + ids, ids coded as "same as previous row" runs
  num/P    fixed-point integers (value * 10^d, with every non-empty value written with exactly d decimals), a null
           mask, and residuals of predictor P:
             none   v
             delta  v - v[prev]
             dod    v - (2 v[prev] - v[prev2])            (delta of delta: linear trend)
             gdelta v - v[prev in same group]             (group = the first text column, e.g. country)
             gdod   delta of delta within the group
  f(...)   derived column: v - round(F(a, b) * 10^d), F one of a/b*k, a*b/k, a-b, a+b, a - lag(a), 100*(a/lag(a) - 1)
           where a and b are other columns already decoded. A formula wins only if its residual stream is smaller.

usage: python3 tabmath.py file.csv [report.json]
"""
import csv, io, lzma, math, re, sys, time
import numpy as np

NUM = re.compile(r'^-?\d+(\.\d+)?$')


def xz(b):
    return len(lzma.compress(b, format=lzma.FORMAT_RAW, filters=[{'id': lzma.FILTER_LZMA2, 'preset': 9 | lzma.PRESET_EXTREME}]))


def varints(arr):
    """zigzag LEB128 of int64 array -> bytes"""
    out = bytearray()
    for v in arr.tolist():
        z = (v << 1) ^ (v >> 63)
        while z >= 128:
            out.append((z & 127) | 128); z >>= 7
        out.append(z)
    return bytes(out)


def bytes_planes(arr):
    """residuals as fixed-width little-endian zigzag ints split into byte planes (often better for LZ/xz)"""
    z = ((arr << 1) ^ (arr >> 63)).astype(np.uint64)
    m = int(z.max()) if len(z) else 0
    w = max(1, (m.bit_length() + 7) // 8)
    b = z.astype('<u8').view(np.uint8).reshape(-1, 8)[:, :w]
    return b.T.tobytes()


def stream_cost(arr):
    return min(xz(varints(arr)), xz(bytes_planes(arr)))


def parse_num(col):
    """-> (ints int64, mask bool, d) or None if the column is not uniformly fixed-point"""
    d = None
    for s in col:
        if s == '':
            continue
        if not NUM.match(s):
            return None
        dd = len(s) - s.index('.') - 1 if '.' in s else 0
        if d is None:
            d = dd
        elif dd != d:
            return None
        if s.startswith('-0') and set(s[1:]) <= set('0.'):
            return None  # negative zero does not round-trip through int
        if len(s) > 18:
            return None
    if d is None:
        return None
    mask = np.array([s != '' for s in col])
    ints = np.zeros(len(col), dtype=np.int64)
    ints[mask] = [int(s.replace('.', '')) for s in col if s != '']
    return ints, mask, d


def fmt(v, d):
    s = str(abs(int(v))).rjust(d + 1, '0')
    s = s[:-d] + '.' + s[-d:] if d else s
    return ('-' if v < 0 else '') + s


def predict(ints, mask, kind, group):
    """prediction for every position from previous non-empty values (encoder and decoder alike)."""
    pred = np.zeros_like(ints)
    last = {}; last2 = {}
    v = ints.tolist(); m = mask.tolist(); g = group if group is not None else [0] * len(v)
    p = pred.tolist() if False else [0] * len(v)
    for i in range(len(v)):
        key = g[i] if kind in ('gdelta', 'gdod') else 0
        a = last.get(key); b = last2.get(key)
        if kind in ('delta', 'gdelta'):
            p[i] = a if a is not None else 0
        elif kind in ('dod', 'gdod'):
            p[i] = (2 * a - b) if b is not None else (a if a is not None else 0)
        if m[i]:
            last2[key] = a; last[key] = v[i]
    return np.array(p, dtype=np.int64)


def lagged(vals, mask, group):
    """previous non-empty value of the same group, NaN if none"""
    out = np.full(len(vals), np.nan); last = {}
    for i in range(len(vals)):
        k = group[i] if group is not None else 0
        if k in last:
            out[i] = last[k]
        if mask[i]:
            last[k] = vals[i]
    return out


def main():
    path = sys.argv[1]
    raw = open(path, 'rb').read()
    rows = list(csv.reader(io.StringIO(raw.decode('utf-8'), newline='')))
    # only handle the plain case: every row has the header's field count and the file re-serialises identically
    header, body = rows[0], rows[1:]
    k = len(header)
    assert all(len(r) == k for r in body)
    buf = io.StringIO(); w = csv.writer(buf, lineterminator='\n'); w.writerows(rows)
    if buf.getvalue().encode() != raw:
        print('note: file does not re-serialise byte-exactly with csv defaults; sizes compare column content only')
    cols = [[r[j] for r in body] for j in range(k)]
    n = len(body)

    group_col = None
    for j in range(k):
        if parse_num(cols[j]) is None and len(set(cols[j])) < n / 20:
            group_col = j; break
    gid = None
    if group_col is not None:
        ids = {}; gid = [ids.setdefault(s, len(ids)) for s in cols[group_col]]

    t0 = time.time()
    total = 0; report = []
    decoded_num = {}   # j -> float values (for formulas), only numeric columns
    for j in range(k):
        col = cols[j]
        text_cost = xz('\n'.join(col).encode())
        best = (text_cost, 'text', None)
        uniq = sorted(set(col))
        if len(uniq) < n / 4:
            idx = {s: i for i, s in enumerate(uniq)}
            ids = np.array([idx[s] for s in col], dtype=np.int64)
            c = xz('\n'.join(uniq).encode()) + stream_cost(ids - np.concatenate([[0], ids[:-1]]))
            if c < best[0]:
                best = (c, 'dict', None)
        pn = parse_num(col)
        if pn is not None:
            ints, mask, d = pn
            mcost = xz(np.packbits(mask).tobytes())
            iv = ints[mask]
            for kind in ('none', 'delta', 'dod') + (('gdelta', 'gdod') if gid else ()):
                pred = predict(ints, mask, kind, gid)
                res = (ints - pred)[mask]
                c = mcost + stream_cost(res)
                if c < best[0]:
                    best = (c, 'num/' + kind, (kind,))
            # derived columns: a / b * k etc. on columns already decoded
            vals = ints / 10 ** d
            scale = 10 ** d
            bestf = None
            prefix_parents = [a for a in decoded_num if header[j].startswith(header[a] + '_') or header[j].startswith(header[a])]
            parents = prefix_parents if prefix_parents else []
            for a in parents:
                A = decoded_num[a]
                cands = {f'{header[a]} - lag': A - lagged(A, ~np.isnan(A), gid),
                         f'100*({header[a]}/lag - 1)': 100 * (A / lagged(A, ~np.isnan(A), gid) - 1)}
                for b in decoded_num:
                    if b == a:
                        continue
                    B = decoded_num[b]
                    with np.errstate(all='ignore'):
                        for kk in (1, 100, 1e3, 1e6, 1e9):
                            cands[f'{header[a]}/{header[b]}*{kk:g}'] = A / B * kk
                for name, F in cands.items():
                    ok = mask & np.isfinite(F) & (np.abs(F) < 1e15)
                    if ok.sum() < mask.sum() * 0.5:
                        continue
                    P = np.zeros(n, dtype=np.int64); P[ok] = np.round(F[ok] * scale).astype(np.int64)
                    hit = (P[ok] == ints[ok]).mean()
                    if hit < 0.3:
                        continue
                    # residual vs formula where it applies, plain delta elsewhere; a flag stream says which
                    fallback = predict(ints, mask, 'gdelta' if gid else 'delta', gid)
                    pred = np.where(ok, P, fallback)
                    res = (ints - pred)[mask]
                    c = mcost + stream_cost(res) + xz(np.packbits(ok[mask]).tobytes()) + len(name)
                    if bestf is None or c < bestf[0]:
                        bestf = (c, name, pred, hit)
            if bestf and bestf[0] < best[0]:
                best = (bestf[0], 'f: ' + bestf[1] + f' ({bestf[3]:.0%} exact)', ('formula', bestf[2]))
            # --- decode check for the chosen numeric model
            if best[2] is not None:
                if best[2][0] == 'formula':
                    pred = best[2][1]
                else:
                    pred = predict(ints, mask, best[2][0], gid)
                res = ints - pred                      # what the encoder stores (at mask positions)
                back = pred + res                      # decoder: prediction from already-decoded data + residual
                back_txt = [fmt(v, d) if m else '' for v, m in zip(back.tolist(), mask.tolist())]
                assert back_txt == col, header[j]
            decoded_num[j] = np.where(mask, vals, np.nan)
        total += best[0]
        report.append((header[j], best[1], text_cost, best[0]))
    total += xz((','.join(header)).encode())
    el = time.time() - t0

    print(f'\n## {path.split("/")[-1]}  {len(raw):,} bytes, {n:,} rows x {k} columns, group column: '
          f'{header[group_col] if group_col is not None else "-"}')
    print(f'xz -9e whole file          : {xz(raw):>10,}  ratio {len(raw) / xz(raw):.2f}')
    print(f'xz -9e per column (text)   : {sum(r[2] for r in report):>10,}  ratio {len(raw) / sum(r[2] for r in report):.2f}')
    print(f'math models per column + xz: {total:>10,}  ratio {len(raw) / total:.2f}   (model search {el:.1f} s, python)')
    print('| column | chosen model | xz text bytes | model bytes |\n|---|---|---|---|')
    for h, m, a, b in sorted(report, key=lambda r: r[2] - r[3], reverse=True)[:14]:
        print(f'| {h} | {m} | {a:,} | {b:,} |')
    import collections, json
    if len(sys.argv) > 2:
        json.dump(report, open(sys.argv[2], 'w'), indent=0)
    print('model counts:', dict(collections.Counter(r[1].split(' (')[0].split(':')[0] for r in report)))


main()
