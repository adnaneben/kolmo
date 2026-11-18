"""Study of regex option (i): numbers routed to a typed stream (docs/PRIORS.md, RESULTS.md).
   python3 -I typed_study.py KOLMO FILE...
For each file: every maximal run of digits is replaced by one byte 0x01 in the text, and the numbers go to a second
stream, either as decimal text one per line (`raw`) or as zig-zag varints of the difference from the number in the same
slot (same index among the numbers of the line) of the line before (`delta`), with the run length kept when it has
leading zeros. Both streams are coded by `KOLMO -x -T1`; the sum is compared with `-x` on the file itself."""
import subprocess, sys, os, tempfile

def kx(kolmo, data):
    with tempfile.TemporaryDirectory() as d:
        a, b = os.path.join(d, 'a'), os.path.join(d, 'a.kz')
        open(a, 'wb').write(data)
        subprocess.run([kolmo, '-x', '-T1', a, b], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        return os.path.getsize(b)

def varint(v, out):
    v = (v << 1) ^ (v >> 63)
    while v >= 128:
        out.append(v & 127 | 128); v >>= 7
    out.append(v)

def split(data):
    text, raw, delta = bytearray(), bytearray(), bytearray()
    prev, cur = [], []
    i, n = 0, len(data)
    while i < n:
        c = data[i]
        if 48 <= c <= 57:
            j = i
            while j < n and 48 <= data[j] <= 57: j += 1
            s = data[i:j]; text.append(1); raw += s + b'\n'
            v = int(s) if len(s) <= 18 else 0
            k = len(cur); p = prev[k] if k < len(prev) else 0
            lead = s[0] == 48 and len(s) > 1
            delta.append(min(len(s), 127) | (128 if lead or len(s) > 18 else 0))
            if len(s) > 18: delta += s
            else: varint(v - p, delta)
            cur.append(v); i = j
            continue
        text.append(c)
        if c == 10: prev, cur = cur, []
        i += 1
    return bytes(text), bytes(raw), bytes(delta)

kolmo = sys.argv[1]
print('file whole text+raw text+delta (text, raw, delta)')
for f in sys.argv[2:]:
    data = open(f, 'rb').read()
    t, r, d = split(data)
    w, ct, cr, cd = kx(kolmo, data), kx(kolmo, t), kx(kolmo, r), kx(kolmo, d)
    print(f'{os.path.basename(f)} {w} {ct + cr} ({100 * (ct + cr - w) / w:+.1f}%) {ct + cd} ({100 * (ct + cd - w) / w:+.1f}%) ({ct}, {cr}, {cd})', flush=True)
