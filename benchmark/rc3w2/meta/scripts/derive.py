import numpy as np, zipfile, io, os, sys, struct, tarfile, shutil, glob
src, out = sys.argv[1], sys.argv[2]
z = np.load(os.path.join(src, "misc/mnist.npz"))
arr = {k: z[k] for k in z.files}
np.savez(os.path.join(out, "mnist_stored.npz"), **arr)          # NPZ, members stored
np.save(os.path.join(out, "mnist_x.npy"), arr["x_train"])       # 60000x28x28 uint8
# y4m -> PGM/PPM/BMP of frames
y = open(os.path.join(src, "misc/akiyo.y4m"), "rb").read()
hdr_end = y.index(b"\n"); W, H = 352, 288
fs = W*H*3//2; frames = []
p = hdr_end + 1
while p < len(y) and len(frames) < 300:
    p = y.index(b"\n", p) + 1; frames.append(np.frombuffer(y[p:p+fs], np.uint8)); p += fs
Y = frames[0][:W*H].reshape(H, W)
U = frames[0][W*H:W*H+W*H//4].reshape(H//2, W//2).repeat(2,0).repeat(2,1)
V = frames[0][W*H+W*H//4:].reshape(H//2, W//2).repeat(2,0).repeat(2,1)
Yf = Y.astype(np.float32); Uf = U.astype(np.float32)-128; Vf = V.astype(np.float32)-128
rgb = np.stack([Yf+1.402*Vf, Yf-0.344*Uf-0.714*Vf, Yf+1.772*Uf], -1).clip(0,255).astype(np.uint8)
big = np.concatenate([np.concatenate([rgb]*3,1)]*3,0)          # 864x1056 rgb
open(os.path.join(out,"frame.ppm"),"wb").write(b"P6\n%d %d\n255\n"%(big.shape[1],big.shape[0])+big.tobytes())
gy = np.concatenate([np.concatenate([Y]*3,1)]*3,0)
open(os.path.join(out,"frame.pgm"),"wb").write(b"P5\n%d %d\n255\n"%(gy.shape[1],gy.shape[0])+gy.tobytes())
g16 = (gy.astype(np.uint16)*257 + np.random.default_rng(1).integers(0,64,gy.shape)).astype(">u2")
open(os.path.join(out,"frame16.pgm"),"wb").write(b"P5\n%d %d\n65535\n"%(gy.shape[1],gy.shape[0])+g16.tobytes())
np.save(os.path.join(out,"frame16.npy"), g16.astype("<u2"))
np.save(os.path.join(out,"frame_rgb.npy"), big)
# BMP 24-bit bottom-up, rows padded to 4
h, w = big.shape[:2]; row = (w*3+3)//4*4
bgr = big[::-1, :, ::-1]; pad = np.zeros((h, row), np.uint8); pad[:, :w*3] = bgr.reshape(h, w*3)
bmp = b"BM"+struct.pack("<IHHI",54+row*h,0,0,54)+struct.pack("<IiiHHIIiiII",40,w,h,1,24,0,row*h,2835,2835,0,0)+pad.tobytes()
open(os.path.join(out,"frame.bmp"),"wb").write(bmp)
