#!/usr/bin/env python3
"""Independent pure-Python reference for E02C (no shared code with the C harness).

1. Decodes stored-backend experimental blobs (all five representations) from the format description in
   e02c.c's header comment and checks the residual byte stream the C selftest wrote beside them.
2. Re-implements the KG_INT inverse from the node description in src/kgraph.inc and reconstructs
   typed inputs from the production parameters, residual and rest streams captured by the C selftest.
"""
import pathlib, re, sys

def uv(b, i):
    r = s = 0
    while True:
        if i >= len(b) or s >= 64: raise ValueError('varint')
        c = b[i]; i += 1; r |= (c & 127) << s
        if not c & 128: return r, i
        s += 7

def stream(b, i, exact=None):
    cc = b[i]; i += 1
    if cc != 0: raise ValueError('reference handles stored streams only')
    raw, i = uv(b, i); coded, i = uv(b, i)
    if raw != coded or (exact is not None and raw != exact): raise ValueError('stream length')
    return b[i:i + raw], i + raw

def unpack(p, cnt, w):
    if len(p) != (cnt * w + 7) // 8: raise ValueError('packed length')
    acc = int.from_bytes(p, 'little'); mask = (1 << w) - 1
    out = [(acc >> (k * w)) & mask for k in range(cnt)]
    if acc >> (cnt * w): raise ValueError('padding')
    return out

def decode(b, m, ow, pln):
    rep = b[0] - 0xE0; i = 1; seg = 128; nseg = (m + seg - 1) // seg
    if rep == 0:
        r, i = stream(b, i, m * ow); assert i == len(b); return r
    if rep == 1:
        w = b[i]; i += 1; p, i = stream(b, i); z = unpack(p, m, w)
    elif rep == 2:
        ws, i = stream(b, i, nseg); p, i = stream(b, i); z = []; off = 0
        for g in range(nseg):
            l = min(seg, m - g * seg); n = (l * ws[g] + 7) // 8; z += unpack(p[off:off + n], l, ws[g]); off += n
        assert off == len(p)
    elif rep == 3:
        w = b[i]; i += 1; ne, i = uv(b, i); p, i = stream(b, i); gaps, i = stream(b, i); highs, i = stream(b, i)
        z = unpack(p, m, w); gi = hi = 0; prev = -1
        for _ in range(ne):
            g, gi = uv(gaps, gi); h, hi = uv(highs, hi); pos = prev + 1 + g; prev = pos; z[pos] |= h << w
    else:
        ws, i = stream(b, i, nseg); ec, i = stream(b, i, nseg); p, i = stream(b, i); pos_s, i = stream(b, i); highs, i = stream(b, i)
        z = []; off = pi = hi = 0
        for g in range(nseg):
            l = min(seg, m - g * seg); n = (l * ws[g] + 7) // 8; part = unpack(p[off:off + n], l, ws[g]); off += n
            for _ in range(ec[g]):
                h, hi = uv(highs, hi); part[pos_s[pi]] |= h << ws[g]; pi += 1
            z += part
    assert i == len(b), 'trailing bytes'
    assert all(v < (1 << (8 * ow)) for v in z)
    out = bytearray(m * ow)
    for k, v in enumerate(z):
        for c in range(ow):
            out[c * m + k if pln else k * ow + c] = (v >> (8 * c)) & 255
    return bytes(out)

def unzz(u): return (u >> 1) ^ -(u & 1)

def int_inverse(par, res, rest):
    w, fl, ow = par[0], par[1], par[2]; sg, order, planes = fl & 1, (fl >> 1) & 3, (fl >> 3) & 1
    i = 3; head, i = uv(par, i); cnt, i = uv(par, i); g, i = uv(par, i); bz, i = uv(par, i)
    seeds = []
    for _ in range(order): s, i = uv(par, i); seeds.append(unzz(s))
    assert i == len(par)
    M = 1 << 64; bias = unzz(bz) % M; m = cnt - order; vals = []
    for q in range(cnt):
        if q < order: v = seeds[q] % M
        else:
            r = q - order
            z = sum(res[(c * m + r) if planes else (r * ow + c)] << (8 * c) for c in range(ow))
            if order == 0: v = (bias + z * g) % M
            else:
                pred = vals[-1] if order == 1 else (2 * vals[-1] - vals[-2])
                v = (pred + unzz(z) * g) % M
        v &= (1 << (8 * w)) - 1
        if sg and w < 8 and v >> (8 * w - 1): v -= 1 << (8 * w)
        vals.append(v % M)
    body = b''.join(((v % M) & ((1 << (8 * w)) - 1)).to_bytes(w, 'little') for v in vals)
    return rest[:head] + body + rest[head:]

def main(d):
    d = pathlib.Path(d); blobs = typed = 0
    for f in sorted(d.glob('*.blob')):
        mt = re.match(r'ow(\d)_p(\d)_m(\d+)_pat(\d)_(\w+)\.blob', f.name); ow, pln, m = int(mt[1]), int(mt[2]), int(mt[3])
        res = (d / f'ow{ow}_p{pln}_m{m}_pat{mt[4]}.res').read_bytes()
        if decode(f.read_bytes(), m, ow, pln) != res: raise SystemExit(f'REFERENCE MISMATCH {f.name}')
        blobs += 1
    for f in sorted(d.glob('typed_*.in')):
        st = f.with_suffix('')
        out = int_inverse(st.with_suffix('.par').read_bytes(), st.with_suffix('.res').read_bytes(), st.with_suffix('.rest').read_bytes())
        if out != f.read_bytes(): raise SystemExit(f'REFERENCE INT INVERSE MISMATCH {f.name}')
        typed += 1
    if not blobs or not typed: raise SystemExit('REFERENCE: no vectors')
    print(f'E02C REFERENCE PASSED stored_blobs={blobs} typed_int_inverses={typed}')

if __name__ == '__main__': main(sys.argv[1])
