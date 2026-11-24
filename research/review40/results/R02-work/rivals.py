# R02 rivals for the image table (measurement only, never linked into Kolmo): lossless sizes of the 17 PNM images with
# the codecs of the imagecodecs wheel that R05 installed under /home/user/kolmo-review40-data/R05-img/rivals.
# usage: python3 -I rivals.py OUT.tsv FILE.ppm|pgm ...
import sys, time
sys.path.insert(0, '/home/user/kolmo-review40-data/R05-img/rivals')
import numpy as np, imagecodecs as ic
out = open(sys.argv[1], 'a')
def load(path):
    d = open(path, 'rb').read(); t = d.split(None, 4); C = 3 if d[:2] == b'P6' else 1; w, h = int(t[1]), int(t[2])
    a = np.frombuffer(d, dtype=np.uint8, offset=len(d) - w * h * C)
    return a.reshape(h, w, 3) if C == 3 else a.reshape(h, w)
codecs = [
    ('jpegls', lambda a: ic.jpegls_encode(a), lambda z: ic.jpegls_decode(z)),
    ('png9', lambda a: ic.png_encode(a, level=9), lambda z: ic.png_decode(z)),
    ('webp_lossless', lambda a: ic.webp_encode(a if a.ndim == 3 else np.stack([a] * 3, -1), lossless=True, level=100, method=6), None),
    ('jxl_e3', lambda a: ic.jpegxl_encode(a, lossless=True, effort=3), lambda z: ic.jpegxl_decode(z)),
    ('jxl_e7', lambda a: ic.jpegxl_encode(a, lossless=True, effort=7), lambda z: ic.jpegxl_decode(z)),
    ('jxl_e9', lambda a: ic.jpegxl_encode(a, lossless=True, effort=9), lambda z: ic.jpegxl_decode(z)),
]
for path in sys.argv[2:]:
    a = load(path); name = path.split('/')[-1]
    for cn, enc, dec in codecs:
        try:
            t0 = time.perf_counter(); z = enc(a); t1 = time.perf_counter(); ok = 'n/a'
            if dec is not None: b = dec(z); ok = 'RT_OK' if np.array_equal(np.asarray(b).reshape(a.shape), a) else 'RT_FAIL'
            t2 = time.perf_counter()
            out.write(f"{name}\t{cn}\t{a.size}\t{len(z)}\t{ok}\t{t1 - t0:.4f}\t{t2 - t1:.4f}\n"); out.flush()
        except Exception as e:
            out.write(f"{name}\t{cn}\t{a.size}\tERROR {type(e).__name__}: {e}\n"); out.flush()
