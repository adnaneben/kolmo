# R03 prototype: BWT-oriented word transform (capital flags + position-distinct word codes). Size experiment only.
# usage: wt.py in out n1 nb nc [minlen] [capmode]   ; prints dictionary bytes (raw) ; exact inverse is checked
import sys, re, collections
src = open(sys.argv[1], 'rb').read()
n1, nb, nc = int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
minlen = int(sys.argv[6]) if len(sys.argv) > 6 else 2
capmode = int(sys.argv[7]) if len(sys.argv) > 7 else 1
used = set(src)
free = [b for b in range(1, 256) if b not in used and not (65 <= b <= 90 or 97 <= b <= 122)]
need = n1 + nb + nc + 2
if len(free) < need:
    print("free bytes", len(free), "need", need); sys.exit(1)
CAP, UPP = free[0], free[1]
S1 = free[2:2 + n1]; SB = free[2 + n1:2 + n1 + nb]; SC = free[2 + n1 + nb:2 + n1 + nb + nc]
tok = re.compile(rb'[A-Za-z]+')
cnt = collections.Counter()
def norm(w):
    if capmode and len(w) >= 2 and w[:1].isupper() and w[1:].islower(): return CAP, w.lower()
    if capmode and len(w) >= 2 and w.isupper(): return UPP, w.lower()
    if capmode and len(w) == 1 and w.isupper(): return CAP, w.lower()
    return None, w
for m in tok.finditer(src):
    f, w = norm(m.group())
    if len(w) >= minlen: cnt[w] += 1
# gain ordering: 1-byte codes to the best count*(len-1), then 2-byte codes by count*(len-2)
c1 = sorted(cnt.items(), key=lambda kv: -kv[1] * (len(kv[0]) - 1))[:n1]
d = {w: bytes([S1[i]]) for i, (w, _) in enumerate(c1)}
rest = [(w, c) for w, c in cnt.items() if w not in d and len(w) >= 3 and c >= 4]
rest.sort(key=lambda kv: -kv[1] * (len(kv[0]) - 2))
rest = rest[:nb * nc]
for i, (w, _) in enumerate(rest): d[w] = bytes([SB[i // nc], SC[i % nc]])
out = bytearray(); pos = 0
for m in tok.finditer(src):
    out += src[pos:m.start()]; pos = m.end()
    f, w = norm(m.group())
    if f is not None: out.append(f)
    out += d.get(w, w)
out += src[pos:]
open(sys.argv[2], 'wb').write(out)
dic = b'\n'.join([w for w, _ in c1] + [w for w, _ in rest])
open(sys.argv[2] + '.dict', 'wb').write(dic)
# inverse check
inv = {v: k for k, v in d.items()}
res = bytearray(); i = 0; flag = None; n = len(out)
s1 = set(S1); sb = set(SB)
def emit(w):
    global flag
    if flag == CAP: w = w[:1].upper() + w[1:]
    elif flag == UPP: w = w.upper()
    flag = None; res.extend(w)
while i < n:
    b = out[i]
    if b == CAP or b == UPP: flag = b; i += 1
    elif b in s1: emit(inv[bytes([b])]); i += 1
    elif b in sb: emit(inv[bytes(out[i:i + 2])]); i += 2
    elif 65 <= b <= 90 or 97 <= b <= 122:
        j = i
        while j < n and (65 <= out[j] <= 90 or 97 <= out[j] <= 122): j += 1
        emit(bytes(out[i:j])); i = j
    else: res.append(b); i += 1
print("in", len(src), "out", len(out), "dict", len(dic), "words", len(d), "free", len(free), "inverse", "ok" if bytes(res) == src else "FAIL")
