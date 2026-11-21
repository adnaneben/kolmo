import struct, sys, numpy as np
p, out, mode = sys.argv[1], sys.argv[2], sys.argv[3]
d = bytearray(open(p,'rb').read())
shoff, = struct.unpack_from('<Q', d, 0x28); shes, shn = struct.unpack_from('<HH', d, 0x3A)
for i in range(shn):
    name, typ, flags, addr, off, size = struct.unpack_from('<IIQQQQ', d, shoff + i*shes)
    if typ == 8 or not size or not (flags & 4): continue
    a = off + (-off % 4); m = (off + size - a) // 4
    w = np.frombuffer(bytes(d[a:a+4*m]), '<u4').astype(np.uint64); pc = (np.arange(m, dtype=np.uint64)*4 + a)
    bl = (w & 0xFC000000) == 0x94000000
    w2 = w.copy()
    w2[bl] = 0x94000000 | ((w[bl] + (pc[bl] >> 2)) & 0x03FFFFFF)
    if mode == 'adrp':
        ad = (w & 0x9F000000) == 0x90000000
        src = ((w >> 29) & 3) | ((w >> 3) & 0x001FFFFC)
        ok = ad & (((src + 0x00020000) & 0x001C0000) == 0)
        dest = (src + (pc >> 12)) & 0x1FFFFFFF   # xz: dest += pc>>12
        # xz keeps 18 bits + sign-extend trick: dest & 0x0003FFFF, then -(dest & 0x00020000)
        dest = ((dest & 0x0003FFFF) - (dest & 0x00020000)) & 0x001FFFFF
        nw = (w & 0x9000001F) | ((dest & 3) << 29) | ((dest & 0x001FFFFC) << 3)
        w2[ok] = nw[ok]
    d[a:a+4*m] = w2.astype('<u4').tobytes()
open(out,'wb').write(d)
