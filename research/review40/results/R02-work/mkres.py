# R02 helper: IMG8 residual leaf (LOCO-I median, zigzag) of a binary PPM/PGM, as kg_img8_fwd computes it. usage: mkres.py in.ppm out.res
import sys
sys.path.insert(0, '/home/user/kolmo-review40-data/R05-img/rivals')
import numpy as np
d = open(sys.argv[1], 'rb').read()
t = d.split(None, 4) if d[:2] == b'P6' or d[:2] == b'P5' else None
C = 3 if d[:2] == b'P6' else 1; w, h = int(t[1]), int(t[2]); head = len(d) - w * h * C
p = np.frombuffer(d, dtype=np.uint8, offset=head).astype(np.int32).reshape(h, w * C)
W = w * C
a = np.zeros_like(p); a[:, C:] = p[:, :-C]          # left
b = np.zeros_like(p); b[1:] = p[:-1]                # up
c = np.zeros_like(p); c[1:, C:] = p[:-1, :-C]       # up-left
med = np.clip(a + b - c, np.minimum(a, b), np.maximum(a, b))
pred = med.copy(); pred[0, :] = a[0, :]; pred[0, :C] = 0; pred[1:, :C] = b[1:, :C]
v = ((p - pred + 128) & 255) - 128
z = np.where(v < 0, -2 * v - 1, 2 * v).astype(np.uint8)
open(sys.argv[2], 'wb').write(z.tobytes()); print(sys.argv[2], W, C, z.size)
