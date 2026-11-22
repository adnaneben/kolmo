import struct, sys, os
p, out = sys.argv[1], sys.argv[2]; os.makedirs(out, exist_ok=True)
d = open(p,'rb').read()
shoff, = struct.unpack_from('<Q', d, 0x28); shentsize, shnum, shstrndx = struct.unpack_from('<HHH', d, 0x3A)
secs = []
for i in range(shnum):
    name, typ, flags, addr, off, size = struct.unpack_from('<IIQQQQ', d, shoff + i*shentsize)
    if typ == 8 or size == 0: continue  # NOBITS
    secs.append((off, off+size, 'code' if flags & 4 else 'data'))
secs.sort()
# ranges: code sections vs everything else
pieces = []; pos = 0
for a,b,k in secs:
    if k != 'code': continue
    if a > pos: pieces.append((pos, a, 'data'))
    pieces.append((a, b, 'code')); pos = b
pieces.append((pos, len(d), 'data'))
for i,(a,b,k) in enumerate(pieces): open(f"{out}/{i:03d}_{k}", 'wb').write(d[a:b])
print(p, [(k, b-a) for a,b,k in pieces])
