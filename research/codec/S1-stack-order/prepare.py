#!/usr/bin/env python3
"""Reproduce the exact observed cohort and six labelled synthetic mechanisms."""
import argparse,hashlib,json,pathlib,random,struct,zipfile
ARTIFACT='00ae71bdc5e4f3612d017dfeabb0f5154742feda67bde2be92f1bfeefb50ebe1'
def sha(b):return hashlib.sha256(b).hexdigest()
def npy(v):
 h="{'descr': '<u4', 'fortran_order': False, 'shape': ("+str(len(v))+",), }"
 h+=' '*((64-(10+len(h)+1)%64)%64)+'\n'
 return b'\x93NUMPY\x01\x00'+struct.pack('<H',len(h))+h.encode()+b''.join(struct.pack('<I',x&0xffffffff) for x in v)
def main():
 p=argparse.ArgumentParser();p.add_argument('artifact',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);a=p.parse_args()
 if sha(a.artifact.read_bytes())!=ARTIFACT:raise ValueError('artifact identity')
 out=a.output;out.mkdir(exist_ok=False);(out/'inputs').mkdir();(out/'evidence').mkdir();records=[]
 with zipfile.ZipFile(a.artifact) as z:
  if len(z.namelist())!=len(set(z.namelist())):raise ValueError('duplicate member')
  manifest=json.loads(z.read('MANIFEST.json'))
  for name,digest in manifest.items():
   if sha(z.read(name))!=digest:raise ValueError('archive manifest')
  names=sorted(n for n in z.namelist() if n.startswith('prepared/inputs/') and n.endswith('.npy'))
  if len(names)!=8:raise ValueError('cohort count')
  for path in names:
   n=pathlib.PurePosixPath(path).name;b=z.read(path);(out/'inputs'/n).write_bytes(b)
   records.append({'name':n,'bytes':len(b),'sha256':sha(b),'kind':'observed_real','family':n.split('__')[0]})
 N=65536;r=random.Random(20261113)
 cases={'smooth_counts':[100000+3*i+(i//32)%3 for i in range(N)],'sparse_outliers':[r.getrandbits(32) if i%257==0 else r.randrange(16) for i in range(N)],'categorical_runs':[(i//128)%17 for i in range(N)],'dense_bounded':[r.randrange(4096) for i in range(N)],'fullwidth_noise':[r.getrandbits(32) for i in range(N)],'zeros':[0 for i in range(N)]}
 for label,values in cases.items():
  n='synthetic__'+label+'.npy';b=npy(values);(out/'inputs'/n).write_bytes(b)
  records.append({'name':n,'bytes':len(b),'sha256':sha(b),'kind':'synthetic_mechanism','family':label,'seed':20261113})
 (out/'evidence/INPUTS.json').write_text(json.dumps(records,indent=2)+'\n')
if __name__=='__main__':main()
