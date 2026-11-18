import zlib, struct, sys, os, numpy as np
def readpng(p):
    d = open(p,'rb').read(); i = 8; idat = b''; W=H=0
    while i < len(d):
        L, t = struct.unpack('>I4s', d[i:i+8]); c = d[i+8:i+8+L]
        if t == b'IHDR': W, H = struct.unpack('>II', c[:8])
        if t == b'IDAT': idat += c
        i += 12 + L
    raw = zlib.decompress(idat); bpp = 3; st = W*3
    out = np.zeros((H, st), np.int32); prev = np.zeros(st, np.int32)
    for y in range(H):
        f = raw[y*(st+1)]; r = np.frombuffer(raw[y*(st+1)+1:(y+1)*(st+1)], np.uint8).astype(np.int32); cur = np.zeros(st, np.int32)
        if f == 0: cur = r
        elif f == 2: cur = (r + prev) & 255
        else:
            for x in range(st):
                a = cur[x-bpp] if x >= bpp else 0; b = prev[x]; c = prev[x-bpp] if x >= bpp else 0
                if f == 1: p = a
                elif f == 3: p = (a + b) >> 1
                else:
                    pa, pb, pc = abs(b-c), abs(a-c), abs(a+b-2*c); p = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                cur[x] = (r[x] + p) & 255
        out[y] = cur; prev = cur
    return out.astype(np.uint8).reshape(H, W, 3)
src, dst = sys.argv[1], sys.argv[2]
for f in sorted(os.listdir(src)):
    im = readpng(os.path.join(src, f)); b = f[:-4]
    for tag, a in (("", im), ("_odd", im[:509, :765])):
        H, W = a.shape[:2]
        open(f"{dst}/{b}{tag}.ppm","wb").write(b"P6\n%d %d\n255\n"%(W,H)+a.tobytes())
        g = (a[...,0]*0.299+a[...,1]*0.587+a[...,2]*0.114).astype(np.uint8)
        open(f"{dst}/{b}{tag}.pgm","wb").write(b"P5\n%d %d\n255\n"%(W,H)+g.tobytes())
        np.save(f"{dst}/{b}{tag}.npy", a)
        row=(W*3+3)//4*4; pad=np.zeros((H,row),np.uint8); pad[:,:W*3]=a[::-1,:,::-1].reshape(H,W*3)
        open(f"{dst}/{b}{tag}.bmp","wb").write(b"BM"+struct.pack("<IHHI",54+row*H,0,0,54)+struct.pack("<IiiHHIIiiII",40,W,H,1,24,0,row*H,2835,2835,0,0)+pad.tobytes())
        g16 = (g.astype(np.uint16)*64 + (a[...,1].astype(np.uint16) & 63))   # 14-bit-ish
        open(f"{dst}/{b}{tag}_16.pgm","wb").write(b"P5\n%d %d\n16383\n"%(W,H)+g16.astype('>u2').tobytes())
