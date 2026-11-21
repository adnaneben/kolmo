import numpy as np, struct, sys, os
src, out = sys.argv[1], sys.argv[2]
im = np.load(f"{src}/img/kodim05_odd.npy"); im2 = np.load(f"{src}/img/kodim23_odd.npy")
r = np.random.default_rng(5)
walk = np.cumsum(r.integers(-300, 301, 400000)).astype(np.int16)
dec = np.round(r.normal(20, 5, 200000), 2)
f32 = (np.cumsum(r.normal(0, 1, 300000))*0.01).astype(np.float32)
np.savez(f"{out}/mixed_stored.npz", image=im, image2=im2, walk=walk, dec=dec, f32=f32)
for nm, a in (("kodim05_odd", im), ("kodim23_odd", im2)):
    H, W, C = a.shape; pix = a.tobytes()
    ents = [(256,4,1,W),(257,4,1,H),(258,3,1,8),(259,3,1,1),(262,3,1,2),(273,4,1,8),(277,3,1,C),(278,4,1,H),(279,4,1,len(pix)),(284,3,1,1)]
    ifd_off = 8 + len(pix)
    hdr = b"II*\0" + struct.pack("<I", ifd_off)
    ifd = struct.pack("<H", len(ents)) + b"".join(struct.pack("<HHI", t, ty, c) + (struct.pack("<HH", v, 0) if ty == 3 else struct.pack("<I", v)) for t,ty,c,v in ents) + struct.pack("<I", 0)
    open(f"{out}/{nm}.tif","wb").write(hdr + pix + ifd)
