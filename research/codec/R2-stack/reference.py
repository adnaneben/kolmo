"""Independent integer reconstruction; does not reimplement entropy decoding."""
import struct
MASK=(1<<64)-1
TAB=[]
for x in range(256):
 for _ in range(8): x=(x>>1)^(0x82F63B78 if x&1 else 0)
 TAB.append(x)
def crc(data,previous=0):
 x=previous^0xffffffff
 for b in data:x=TAB[(x^b)&255]^(x>>8)
 return x^0xffffffff
class Reader:
 def __init__(self,p):self.p=p;self.at=0
 def var(self):
  v=0
  for k in range(10):
   if self.at>=len(self.p):raise ValueError('truncated varint')
   b=self.p[self.at];self.at+=1
   if k==9 and b>1:raise ValueError('overflow')
   v|=(b&127)<<(k*7)
   if not b&128:
    if k and not b:raise ValueError('noncanonical')
    return v
  raise ValueError('varint length')
def unzz(v):return (v>>1)^-(v&1)
def restore(frame,residual):
 if len(frame)<64 or frame[:6]!=b'E2C1\x01\x00':raise ValueError('header')
 if frame[6] not in (0,13,14) or frame[7] not in (0,frame[6]):raise ValueError('codec')
 raw,pn,rn,replen,cn,reslen,checksum,fcrc=struct.unpack_from('<6Q2I',frame,8)
 if len(frame)!=64+pn+rn+cn or replen!=reslen or len(residual)!=reslen:raise ValueError('length')
 if crc(frame[64:],crc(frame[:60]))!=fcrc:raise ValueError('frame crc')
 p=frame[64:64+pn];rest=frame[64+pn:64+pn+rn]
 if len(p)<3:raise ValueError('params')
 w,flags,ow=p[:3];order=(flags>>1)&3
 if w not in (1,2,4,8) or ow not in (1,2,4,8) or order>2 or flags>15:raise ValueError('params values')
 r=Reader(p);r.at=3;head,count,gcd,bias=(r.var() for _ in range(4));seeds=[r.var() for _ in range(order)]
 if r.at!=len(p) or count<order or (count-order)*ow!=reslen or count*w+rn!=raw or head>rn or not gcd:raise ValueError('metadata')
 n=count-order;out=bytearray(rest[:head]);v1=v2=0
 for i in range(count):
  if i<order:v=unzz(seeds[i])&MASK
  else:
   j=i-order;z=sum(residual[b*n+j if flags&8 else j*ow+b]<<(b*8) for b in range(ow))
   v=(unzz(bias)+z*gcd)&MASK if not order else ((v1 if order==1 else 2*v1-v2)+unzz(z)*gcd)&MASK
  out.extend((v&((1<<(8*w))-1)).to_bytes(w,'little'))
  if w<8:
   v&=(1<<(8*w))-1
   if flags&1 and v&(1<<(8*w-1)):v|=MASK^((1<<(8*w))-1)
  v2,v1=v1,v
 out.extend(rest[head:])
 if len(out)!=raw or crc(out)!=checksum:raise ValueError('input checksum')
 return bytes(out)
