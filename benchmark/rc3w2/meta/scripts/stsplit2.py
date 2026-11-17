import json, struct, sys, os
p, outdir, minsz = sys.argv[1], sys.argv[2], int(sys.argv[3]); os.makedirs(outdir, exist_ok=True)
d = open(p,'rb').read(); n = struct.unpack('<Q', d[:8])[0]; h = json.loads(d[8:8+n]); h.pop('__metadata__', None)
base = 8 + n; items = sorted(h.values(), key=lambda v: v['data_offsets'][0])
segs = [[0, base, 'hdr', 1]]
for v in items:
    a, b = v['data_offsets']; a += base; b += base; big = b - a >= minsz
    last = segs[-1]
    if not big and not last[3] and last[2] == v['dtype'] and last[1] == a: last[1] = b
    else: segs.append([a, b, v['dtype'], big])
for i,(a,b,t,_) in enumerate(segs): open(f"{outdir}/{i:04d}_{t}", 'wb').write(d[a:b])
print(len(segs))
