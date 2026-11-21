import json, struct, sys, numpy as np
def H(x, k):
    c = np.bincount(x, minlength=k).astype(np.float64); c = c[c>0]; p = c/c.sum(); return -(p*np.log2(p)).sum()*len(x)/8
p = sys.argv[1]; d = open(p,'rb').read(); n = struct.unpack('<Q', d[:8])[0]; h = json.loads(d[8:8+n]); h.pop('__metadata__',None); base=8+n
tot_glob_parts = {}; per = 0; per_hi_lo = 0; allexp=[]; allsm=[]; size=0
for k,v in h.items():
    if v['dtype'] not in ('BF16','F16'): continue
    a,b = v['data_offsets']; x = np.frombuffer(d[base+a:base+b], '<u2').astype(np.int64); size += b-a
    if v['dtype']=='BF16': e = (x>>7)&255; sm = ((x>>15)<<7)|(x&127)
    else: e = (x>>10)&31; sm = ((x>>15)<<10)|(x&1023)
    allexp.append(e); allsm.append(sm)
    per += H(e,256) + H(sm, 2048)
    per_hi_lo += H(x>>8,256)+H(x&255,256)
E = np.concatenate(allexp); S = np.concatenate(allsm); X=None
print(p, 'bytes', size, 'global exp+sm', int(H(E,256)+H(S,2048)), 'per-tensor exp+sm', int(per), 'per-tensor hi/lo bytes', int(per_hi_lo))
