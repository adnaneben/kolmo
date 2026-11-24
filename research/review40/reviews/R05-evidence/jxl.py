import sys, os, hashlib
sys.path.insert(0, '/home/user/kolmo-review40-data/R05-img/rivals')
import numpy as np, imagecodecs as ic
print('imagecodecs', ic.__version__, 'jpegxl', ic.jpegxl_version() if hasattr(ic,'jpegxl_version') else '?')
def read_pnm(raw):
    parts, pos = [], 0
    while len(parts) < 4:
        while raw[pos:pos+1].isspace(): pos += 1
        e = pos
        while not raw[e:e+1].isspace(): e += 1
        parts.append(raw[pos:e]); pos = e
    pos += 1
    return pos, int(parts[1]), int(parts[2]), 3 if parts[0] == b'P6' else 1
efforts = [int(x) for x in sys.argv[2].split(',')]
for f in sys.argv[3:]:
    raw = open(f, 'rb').read(); pos, w, h, c = read_pnm(raw)
    a = np.frombuffer(raw, np.uint8, offset=pos).reshape((h, w, c) if c == 3 else (h, w))
    row = [os.path.basename(f), len(raw)]
    for e in efforts:
        z = ic.jpegxl_encode(a, lossless=True, effort=e, numthreads=1)
        b = ic.jpegxl_decode(z, numthreads=1)
        assert np.array_equal(a, b)
        row.append('e%d=%d' % (e, len(z) + pos))
    print(*row, flush=True)
