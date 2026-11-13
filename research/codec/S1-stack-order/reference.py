#!/usr/bin/env python3
"""Independent KST1 parsing and integer inverse; entropy readers reuse pinned code."""
import argparse,hashlib,json,pathlib,struct,subprocess,tempfile
MASK=(1<<64)-1
TAB=[]
for x in range(256):
 for _ in range(8):x=(x>>1)^(0x82f63b78 if x&1 else 0)
 TAB.append(x)
CRC_CACHE={}
def crc(b):
 h=hashlib.sha256(b).digest()
 if h not in CRC_CACHE:
  c=0xffffffff
  for x in b:c=TAB[(c^x)&255]^(c>>8)
  CRC_CACHE[h]=c^0xffffffff
 return CRC_CACHE[h]
class Reader:
 def __init__(self,b):self.b=b;self.i=0
 def take(self,n):
  if n<0 or n>len(self.b)-self.i:raise ValueError('truncated')
  r=self.b[self.i:self.i+n];self.i+=n;return r
 def get(self,n):return int.from_bytes(self.take(n),'little')
 def var(self):
  v=0
  for k in range(10):
   b=self.get(1)
   if k==9 and b>1:raise ValueError('overflow')
   v|=(b&127)<<(7*k)
   if not b&128:return v
  raise ValueError('varint')
def unzz(v):return (v>>1)^-(v&1)
class Decoder:
 def __init__(self,leaf,work):self.leaf=leaf;self.work=work;self.cache={};self.inverse_cache={};self.leaf_calls=0
 def raw(self,c,b,n):
  if n>(64<<20):raise ValueError('resource limit')
  key=(c,n,hashlib.sha256(b).digest())
  if key not in self.cache:
   inp=self.work/'in';out=self.work/'out';inp.write_bytes(b)
   subprocess.run([str(self.leaf),chr(c),str(inp),str(n),str(out)],check=True,timeout=30,capture_output=True)
   self.cache[key]=out.read_bytes();self.leaf_calls+=1
  v=self.cache[key]
  if len(v)!=n:raise ValueError('leaf size')
  return v
 def unpack(self,b,depth,n):
  if depth not in (1,2):raise ValueError('depth')
  for _ in range(depth):
   r=Reader(b);c=r.get(1);length=r.get(8);clen=r.get(8);payload=r.take(clen)
   if r.i!=len(b):raise ValueError('stage tail')
   if not c:
    if length or clen:raise ValueError('empty')
    b=b''
   else:b=self.raw(c,payload,length)
  if len(b)!=n:raise ValueError('decoded size')
  return b
 def inverse(self,par,res,rest,mode,ow,pln,n):
  key=hashlib.sha256(par+res+rest+bytes([mode,ow,pln])).digest()
  if key in self.inverse_cache:return self.inverse_cache[key]
  p=Reader(par);w=p.get(1);flags=p.get(1);width=p.get(1);order=(flags>>1)&3
  head,count,g,bias=[p.var() for _ in range(4)];seeds=[p.var() for _ in range(order)]
  if w not in (1,2,4,8) or width!=ow or order>2 or p.i!=len(par) or count<order or (count-order)*ow!=len(res) or head>len(rest) or not g or count*w+len(rest)!=n:raise ValueError('params')
  out=bytearray(rest[:head]);prev=prev2=0;m=count-order
  for i in range(count):
   if i<order:v=unzz(seeds[i])&MASK
   else:
    j=i-order;z=sum(res[k*m+j if mode!=1 else j*ow+k]<<(8*k) for k in range(ow))
    v=(unzz(bias)+z*g)&MASK if not order else ((prev if order==1 else 2*prev-prev2)+unzz(z)*g)&MASK
   out.extend((v&((1<<(8*w))-1)).to_bytes(w,'little'))
   if w<8:
    v&=(1<<(8*w))-1
    if flags&1 and v&(1<<(8*w-1)):v|=MASK^((1<<(8*w))-1)
   prev2,prev=prev,v
  out+=rest[head:];self.inverse_cache[key]=bytes(out);return bytes(out)
 def frame(self,b,n,level=0):
  r=Reader(b)
  if r.take(4)!=b'KST1' or level>1:raise ValueError('magic/depth')
  mode=r.get(1);size=r.get(8);checksum=r.get(4)
  if size!=n:raise ValueError('input size')
  if mode==4:
   rawsize=r.get(8);cn=r.get(8);inner=self.unpack(r.take(cn),1,rawsize);result=self.frame(inner,n,level+1)
  else:
   pn,tn,rn=[r.get(8) for _ in range(3)];ow=r.get(1);pln=r.get(1);groups=r.get(4)
   if groups not in range(1,9):raise ValueError('groups')
   par=r.take(pn);rest=r.take(tn);res=b''
   for _ in range(groups):
    dep=r.get(1);cn=r.get(8);res+=self.unpack(r.take(cn),dep,rn//groups)
   result=res if mode==0 else self.inverse(par,res,rest,mode,ow,pln,n)
  if r.i!=len(b) or len(result)!=n or crc(result)!=checksum:raise ValueError('frame integrity')
  return result

def main():
 p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('--reader',type=pathlib.Path,required=True);a=p.parse_args();r=a.root;inputs=json.loads((r/'evidence/INPUTS.json').read_text());count=0
 with tempfile.TemporaryDirectory() as tmp:
  d=Decoder(a.reader.resolve(),pathlib.Path(tmp))
  for inp in inputs:
   original=(r/'inputs'/inp['name']).read_bytes()
   for f in sorted((r/'first'/inp['name']).glob('*.frame')):
    frame=f.read_bytes();out=d.raw(ord('Z'),frame,len(original)) if f.name.startswith('zstd3') else d.frame(frame,len(original))
    if out!=original:raise ValueError('reference mismatch '+str(f))
    count+=1
   print('reference-ok',inp['name'],flush=True)
  receipt={'frames_reconstructed':count,'entropy_reader_calls_after_exact_byte_cache':d.leaf_calls,'integer_inverse_cache_entries':len(d.inverse_cache),'status':'PASS','limitation':'independent framing and integer inverse; pinned entropy decoder implementation reused'}
 (r/'evidence/REFERENCE.json').write_text(json.dumps(receipt,indent=2)+'\n');print(json.dumps(receipt))
if __name__=='__main__':main()
