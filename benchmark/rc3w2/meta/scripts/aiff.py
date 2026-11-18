import struct, sys, numpy as np
src, dst = sys.argv[1], sys.argv[2]
d = open(src,'rb').read(); i = 12; fmt = None
while i < len(d):
    cid, ln = d[i:i+4], struct.unpack('<I', d[i+4:i+8])[0]
    if cid == b'fmt ': fmt = struct.unpack('<HHIIHH', d[i+8:i+24])
    if cid == b'data': pcm = d[i+8:i+8+ln]
    i += 8 + ln + (ln & 1)
ch, rate, bits = fmt[1], fmt[2], fmt[5]
be = np.frombuffer(pcm, '<i2').astype('>i2').tobytes(); nf = len(pcm)//(2*ch)
def ext(r):  # 80-bit float
    import math; e = int(math.floor(math.log2(r))); m = int(r / 2**e * 2**63)
    return struct.pack('>HQ', e + 16383, m)
comm = struct.pack('>hIh', ch, nf, bits) + ext(rate)
ssnd = struct.pack('>II', 0, 0) + be
body = b'AIFF' + b'COMM' + struct.pack('>I', len(comm)) + comm + b'SSND' + struct.pack('>I', len(ssnd)) + ssnd
open(dst,'wb').write(b'FORM' + struct.pack('>I', len(body)) + body)
