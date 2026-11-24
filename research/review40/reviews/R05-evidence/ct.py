import sys, os, numpy as np
src, dst = sys.argv[1], sys.argv[2]
for f in sorted(os.listdir(src)):
    if not f.endswith('.ppm'): continue
    raw = open(os.path.join(src, f), 'rb').read(); parts = raw.split(None, 4)[:4]; w, h = int(parts[1]), int(parts[2]); hl = len(raw) - w*h*3
    a = np.frombuffer(raw, np.uint8, offset=hl).reshape(h, w, 3)
    R, G, B = a[...,0], a[...,1], a[...,2]; b = np.uint8(128)
    sg = np.stack([G, R - G + b, B - G + b], -1)    # subtract green, green first, differences biased by 128 (uint8 wraps)
    ch = np.stack([R, G - R + b, B - G + b], -1)    # chain in file order, biased
    open(os.path.join(dst, f[:-4] + '.sg.ppm'), 'wb').write(raw[:hl] + sg.tobytes())
    open(os.path.join(dst, f[:-4] + '.ch.ppm'), 'wb').write(raw[:hl] + ch.tobytes())
