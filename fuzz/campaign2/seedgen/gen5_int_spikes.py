import random, struct, sys
R=random.Random(11); O=sys.argv[1]
def series(fmt, n, base, step, spike, lo, hi, every=997):
    out=bytearray(); v=base
    for i in range(n):
        v=max(lo,min(hi,v+R.randint(-step,step)))
        x=v
        if i%every==500: x=max(lo,min(hi,v+spike))
        out+=struct.pack(fmt,x)
    return bytes(out)
S={
 'w2o4_s.bin':('<h',0,40,40000,-32768,32767),   'w2o4_u.bin':('<H',30000,40,35000,0,65535),
 'w2o2_s.bin':('<h',0,40,3000,-32768,32767),
 'w4o2_s.bin':('<i',0,40,20000,-2**31,2**31-1), 'w4o2_u.bin':('<I',10**6,40,20000,0,2**32-1),
 'w4o4_s.bin':('<i',0,40,2**24,-2**31,2**31-1), 'w4o4_u.bin':('<I',10**6,40,2**24,0,2**32-1),
 'w4o8_s.bin':('<i',0,40,2**31-1,-2**31,2**31-1),'w4o8_u.bin':('<I',10**6,40,2**32-10**7,0,2**32-1),
 'w8o8_s.bin':('<q',0,40,2**62,-2**63,2**63-1), 'w8o2_u.bin':('<q',10**9,40,20000,0,2**62),
}
for name,(fmt,base,step,spike,lo,hi) in S.items():
    n = 600000//struct.calcsize(fmt)
    open(O+'/'+name,'wb').write(series(fmt,n,base,step,spike,lo,hi))
