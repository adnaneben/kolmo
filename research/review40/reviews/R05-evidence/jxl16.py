import sys
sys.path.insert(0, '/home/user/kolmo-review40-data/R05-img/rivals')
import numpy as np, imagecodecs as ic
for name, W, R, h in (("mr", 512, 9728, 9092), ("x-ray", 1900, 2230, 240)):
    raw = open('/home/user/kolmo-review40-data/silesia/' + name, 'rb').read()
    img = np.frombuffer(raw, dtype="<u2", offset=h).reshape(R, W).copy(); rest = len(raw) - h - 2*R*W
    row = [name, len(raw), 'max=%d' % img.max()]
    for e in (7, 9):
        z = ic.jpegxl_encode(img, lossless=True, effort=e, numthreads=1); b = ic.jpegxl_decode(z, numthreads=1)
        assert np.array_equal(np.asarray(b).reshape(R, W), img)
        row.append('e%d=%d' % (e, len(z) + h + rest))
    if name == 'mr':   # 19 slices as separate 512x512 images (what a slice-aware tool would do)
        for e in (9,):
            t = 0
            for s in range(19):
                z = ic.jpegxl_encode(img[s*512:(s+1)*512], lossless=True, effort=e, numthreads=1); t += len(z)
            row.append('e%d-per-slice=%d' % (e, t + h + rest))
    print(*row, flush=True)
