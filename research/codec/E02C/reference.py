"""Independent packing + integer inverse. Entropy decoding is NOT reimplemented.
The C entropy decoder exports the represented bytes; this reference checks their
packing inverse, complete reconstruction and both research-frame CRC32Cs.
"""
import struct
MASK=(1<<64)-1
_TABLE=[]
for _x in range(256):
    for _ in range(8): _x=(_x>>1)^(0x82F63B78 if _x&1 else 0)
    _TABLE.append(_x)
def crc(data,previous=0):
    c=previous^0xffffffff
    for b in data:c=_TABLE[(c^b)&255]^(c>>8)
    return c^0xffffffff
class Reader:
    def __init__(self,data):self.data=data;self.at=0
    def take(self,n):
        if n<0 or n>len(self.data)-self.at:raise ValueError('truncated')
        b=self.data[self.at:self.at+n];self.at+=n;return b
    def byte(self):return self.take(1)[0]
    def var(self):
        x=0
        for k in range(10):
            c=self.byte()
            if k==9 and c>1:raise ValueError('overflow')
            x|=(c&127)<<(7*k)
            if not c&128:
                if k and not c:raise ValueError('noncanonical')
                return x
        raise ValueError('varint overflow')
    def bits(self,n,b):
        if b>64:raise ValueError('width')
        p=self.take((n*b+7)//8)
        if p and n*b%8 and p[-1]>>(n*b%8):raise ValueError('padding')
        mask=(1<<b)-1
        return [(int.from_bytes(p[(i*b)//8:((i+1)*b+7)//8],'little')>>((i*b)%8))&mask for i in range(n)]
    def done(self):
        if self.at!=len(self.data):raise ValueError('trailing bytes')
def unpack(data,n,method):
    r=Reader(data);out=[]
    if method==1:out=r.bits(n,r.byte())
    else:
        for at in range(0,n,128):
            m=min(n-at,128);b=r.byte()
            if method==4:
                if b not in (1,2,4,8):raise ValueError('plane width')
                p=r.take(m*b);out.extend(sum(p[j*m+i]<<(8*j) for j in range(b)) for i in range(m));continue
            ne=int.from_bytes(r.take(2),'little') if method==3 else 0
            if ne>m or b==64 and ne:raise ValueError('exceptions')
            v=r.bits(m,b);previous=-1
            for _ in range(ne):
                pos=r.byte();hi=r.var()
                if pos<=previous or pos>=m or not hi or b>=64 or hi>MASK>>b:raise ValueError('exception')
                v[pos]|=hi<<b;previous=pos
            out.extend(v)
    r.done();return out
def metadata(par):
    r=Reader(par);w,fl,ow=r.byte(),r.byte(),r.byte();order=(fl>>1)&3
    if w not in (2,4,8) or ow not in (1,2,4,8) or fl>15 or order>2:raise ValueError('params')
    head,count,g,bias=r.var(),r.var(),r.var(),r.var();seeds=[r.var() for _ in range(order)];r.done()
    return dict(width=w,flags=fl,output_width=ow,order=order,head=head,count=count,gcd=g,bias=bias,seeds=seeds)
def unzz(z):return (z>>1)^-(z&1)
def restore(frame,represented):
    if len(frame)<64 or frame[:5]!=b'E2C1\x01':raise ValueError('header')
    method,req,actual=frame[5:8]
    if method>4 or req not in (0,3,13,14) or actual not in (0,req):raise ValueError('ids')
    raw,pn,rn,replen,cn,reslen,original_crc,frame_crc=struct.unpack_from('<6Q2I',frame,8)
    if 64+pn+rn+cn!=len(frame) or len(represented)!=replen:raise ValueError('sizes')
    if crc(frame[64:],crc(frame[:60]))!=frame_crc:raise ValueError('frame CRC')
    par=frame[64:64+pn];rest=frame[64+pn:64+pn+rn];m=metadata(par)
    w,ow,order,count=m['width'],m['output_width'],m['order'],m['count'];n=count-order
    if n<0 or n>1048576 or n*ow!=reslen or count*w+rn!=raw or m['head']>rn or not m['gcd']:raise ValueError('metadata bounds')
    if method==0:
        if len(represented)!=reslen:raise ValueError('residual length')
        values=[sum(represented[j*n+i if m['flags']&8 else i*ow+j]<<(8*j) for j in range(ow)) for i in range(n)]
    else:values=unpack(represented,n,method)
    residual=bytearray(reslen)
    for i,v in enumerate(values):
        if v>>(ow*8):raise ValueError('residual overflow')
        for j in range(ow):residual[j*n+i if m['flags']&8 else i*ow+j]=(v>>(8*j))&255
    result=bytearray(rest[:m['head']]);v1=v2=0
    for i in range(count):
        if i<order:v=unzz(m['seeds'][i])&MASK
        else:
            z=values[i-order]
            v=(unzz(m['bias'])+z*m['gcd'])&MASK if not order else ((v1 if order==1 else 2*v1-v2)+unzz(z)*m['gcd'])&MASK
        result.extend((v&((1<<(8*w))-1)).to_bytes(w,'little'))
        if w<8:
            v&=(1<<(8*w))-1
            if m['flags']&1 and v&(1<<(8*w-1)):v|=MASK^((1<<(8*w))-1)
        v2,v1=v1,v
    result.extend(rest[m['head']:])
    if len(result)!=raw or crc(result)!=original_crc:raise ValueError('input CRC')
    return bytes(result),bytes(residual),m
