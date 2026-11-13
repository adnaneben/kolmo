#!/usr/bin/env python3
"""Fail-closed source exposure audit and exact preregistered source preparation."""
from __future__ import annotations
import argparse,csv,gzip,hashlib,io,json,re,struct,subprocess,urllib.request,zipfile
from pathlib import Path
HERE=Path(__file__).resolve().parent
SOURCE_URLS={'covertype':'https://archive.ics.uci.edu/static/public/31/covertype.zip','collegemsg':'https://snap.stanford.edu/data/CollegeMsg.txt.gz'}
def sha(b):return hashlib.sha256(b).hexdigest()
def save(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def npy_u32(values):
 for v in values:
  if not isinstance(v,int) or not 0<=v<2**32:raise ValueError('not exact uint32')
 header=("{'descr': '<u4', 'fortran_order': False, 'shape': ("+str(len(values))+",), }").encode()
 header+=b' '*((-10-len(header)-1)%64)+b'\n'
 return b'\x93NUMPY\x01\x00'+struct.pack('<H',len(header))+header+struct.pack('<'+'I'*len(values),*values)
def exposure(root,out):
 paths=subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0');scanned={};hits={};skipped=[]
 pattern=re.compile(r'covtype|covertype|collegemsg|static/public/31/',re.I)
 for name in paths:
  if not name or name.startswith('research/codec/R3-guard/') or name=='.github/workflows/r3-compatible-validation.yml':continue
  if not name.startswith(('benchmark/','bench/','tune/','research/','.github/')) and name not in ('README.md','AGENTS.md','CLAUDE.md'):continue
  p=root/name
  if not p.is_file():continue
  data=p.read_bytes()
  if len(data)>20*1024*1024:skipped.append({'path':name,'reason':'larger_than_20MiB','sha256':sha(data)});continue
  try:text=data.decode('utf-8')
  except UnicodeDecodeError:skipped.append({'path':name,'reason':'binary','sha256':sha(data)});continue
  scanned[name]=sha(data)
  found=[{'line':i,'text':s[:700]} for i,s in enumerate(text.splitlines(),1) if pattern.search(s)]
  if found:hits[name]=found
 result={'status':'overlap_requires_review' if hits else 'no_identifier_matches','commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),'files':scanned,'hits':hits,'skipped':skipped,'limitation':'text identifier audit plus explicit exposed-source register; not proof against unnamed aliases; no new data fetched if matches found'}
 save(out/'EXPOSURE_AUDIT.json',result)
 if hits:raise RuntimeError('prospective source exposure match; do not substitute sources')
 return result

def gunzip(data,limit):
 with gzip.GzipFile(fileobj=io.BytesIO(data)) as f:raw=f.read(limit+1)
 if len(raw)>limit:raise ValueError('decompression limit')
 return raw

def get_bytes(url,limit):
 req=urllib.request.Request(url,headers={'User-Agent':'Kolmo-research-R3/1.0 (+source-attribution-in-repository)'})
 with urllib.request.urlopen(req,timeout=90) as r:data=r.read(limit+1)
 if len(data)>limit:raise ValueError('download too large')
 return data

def parse_covertype(data, expected=262144):
 rows=csv.reader(io.StringIO(data.decode('ascii')));fields={0:[],5:[],9:[],6:[],54:[]}
 for index,row in enumerate(rows):
  if index==expected:break
  if len(row)!=55:raise ValueError('covertype field count')
  vals=[int(v) if re.fullmatch(r'-?\d+',v) else None for v in row]
  if any(v is None for v in vals):raise ValueError('nonintegral source token')
  for col,values in fields.items():
   v=vals[col]
   if not 0<=v<2**32:raise ValueError('uint32 overflow')
   values.append(v)
 if any(len(v)!=expected for v in fields.values()):raise ValueError('insufficient source rows')
 return fields

def parse_collegemsg(data, expected=59835):
 fields=[[],[],[]]
 for line in data.decode('ascii').splitlines():
  if not line.strip() or line.startswith('#'):continue
  tokens=line.split()
  if len(tokens)!=3 or any(not re.fullmatch(r'\d+',x) for x in tokens):raise ValueError('temporal triple')
  for k,x in enumerate(tokens):
   v=int(x)
   if not 0<=v<2**32:raise ValueError('uint32 overflow')
   fields[k].append(v)
 if any(len(v)!=expected for v in fields):raise ValueError('expected 59835 temporal triples')
 return fields

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--repo',type=Path,required=True);p.add_argument('--capture',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);status={'status':'started','partition':'fresh_validation'};save(out/'STATUS.json',status)
 try:
  exposure(a.repo.resolve(),out)
  downloads=out/'downloads';downloads.mkdir();inputs=out/'inputs';inputs.mkdir();receipts={};cases=[]
  for family,url in SOURCE_URLS.items():
   data=get_bytes(url,32<<20);suffix='.zip' if family=='covertype' else '.gz';(downloads/(family+suffix)).write_bytes(data)
   if family=='covertype':
    z=zipfile.ZipFile(io.BytesIO(data));names=[n for n in z.namelist() if Path(n).name=='covtype.data.gz']
    if len(names)!=1 or z.getinfo(names[0]).file_size>32<<20:raise ValueError('source member')
    packed=z.read(names[0]);raw=gunzip(packed,128<<20)
    if len(raw)>128<<20:raise ValueError('source decompression bound')
    arrays=parse_covertype(raw);labels={0:'elevation',5:'road_distance',9:'fire_distance',6:'hillshade_control',54:'cover_control'}
    series=[(labels[k],v) for k,v in arrays.items()]
    attribution='Blackard, J. (1998), Covertype, UCI, DOI 10.24432/C50K5N, CC BY 4.0'
   else:
    packed=data;raw=gunzip(data,16<<20)
    if len(raw)>16<<20:raise ValueError('source decompression bound')
    arrays=parse_collegemsg(raw);series=list(zip(('source_id','target_id','unix_timestamp'),arrays))
    attribution='Panzarasa, Opsahl and Carley (2009), SNAP CollegeMsg; public anonymized SRC/DST/time metadata; no message text'
   receipts[family]={'url':url,'download_sha256':sha(data),'member_sha256':sha(packed),'uncompressed_sha256':sha(raw),'attribution':attribution}
   for label,values in series:
    name=family+'__'+label;data=npy_u32(values);path=inputs/(name+'.npy');path.write_bytes(data)
    cases.append({'id':name,'family':family,'group':'fresh_real','partition':'fresh_validation','input':str(path),'input_sha256':sha(data),'input_bytes':len(data),'values':len(values),'dtype':'<u4','source_rule':'first262144_rows' if family=='covertype' else 'all59835_triples','frame':str(inputs/(name+'.source.frame'))})
  # Input/source hashes frozen before first codec call.
  save(out/'SOURCE_RECEIPTS.json',receipts);save(out/'INPUT_MANIFEST.json',{'partition':'fresh_validation','inputs':cases})
  for c in cases:
   cmd=[str(a.capture.resolve()),c['input'],c['frame']];cp=subprocess.run(cmd,capture_output=True,text=True,timeout=60)
   (inputs/(c['id']+'.capture.log')).write_text(cp.stdout+cp.stderr)
   if cp.returncode:raise RuntimeError('production capture '+c['id'])
   c['source_frame_sha256']=sha(Path(c['frame']).read_bytes());c['capture_command']=cmd
  save(out/'manifest.json',{'partition':'fresh_validation','inputs':cases,'sources':receipts})
  status.update(status='prepared',cases=len(cases));save(out/'STATUS.json',status)
 except BaseException as e:status.update(status='failed_or_incomplete',error=str(e));save(out/'STATUS.json',status);raise
if __name__=='__main__':main()
