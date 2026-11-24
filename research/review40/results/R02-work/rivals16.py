# R02 rivals for the 16-bit image table (measurement only, never linked into Kolmo): lossless sizes of Silesia mr and
# x-ray with the codecs of the imagecodecs wheel that R05 installed under /home/user/kolmo-review40-data/R05-img/rivals.
# Geometry as benchmark/rc2/run_spec2.py (little-endian u16; the file's header bytes are added to every size).
# usage: python3 -I rivals16.py OUT.tsv SILESIA_DIR      (columns: file codec raw coded+header round_trip enc_s dec_s)
import sys, time
sys.path.insert(0, '/home/user/kolmo-review40-data/R05-img/rivals')
import numpy as np, imagecodecs as ic
out = open(sys.argv[1], 'a')
codecs = [
    ('jpegls', lambda a: ic.jpegls_encode(a), lambda z: ic.jpegls_decode(z)),
    ('jxl_e3', lambda a: ic.jpegxl_encode(a, lossless=True, effort=3, numthreads=1), lambda z: ic.jpegxl_decode(z, numthreads=1)),
    ('jxl_e7', lambda a: ic.jpegxl_encode(a, lossless=True, effort=7, numthreads=1), lambda z: ic.jpegxl_decode(z, numthreads=1)),
    ('jxl_e9', lambda a: ic.jpegxl_encode(a, lossless=True, effort=9, numthreads=1), lambda z: ic.jpegxl_decode(z, numthreads=1)),
]
for name, W, R, h in (("mr", 512, 9728, 9092), ("x-ray", 1900, 2230, 240)):
    raw = open(sys.argv[2] + '/' + name, 'rb').read()
    a = np.frombuffer(raw, dtype='<u2', offset=h).reshape(R, W).copy()
    for cn, enc, dec in codecs:
        try:
            t0 = time.perf_counter(); z = enc(a); t1 = time.perf_counter(); b = dec(z); t2 = time.perf_counter()
            ok = 'RT_OK' if np.array_equal(np.asarray(b).reshape(a.shape), a) else 'RT_FAIL'
            out.write(f"{name}\t{cn}\t{len(raw)}\t{len(z) + h}\t{ok}\t{t1 - t0:.4f}\t{t2 - t1:.4f}\n"); out.flush()
        except Exception as e:
            out.write(f"{name}\t{cn}\t{len(raw)}\tERROR {type(e).__name__}: {e}\n"); out.flush()
