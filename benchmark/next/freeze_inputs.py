#!/usr/bin/env python3
"""Freeze regression, public holdout and explicitly synthetic input families."""
import array,hashlib,json,pathlib,random,sys,tarfile
root=pathlib.Path(__file__).resolve().parents[2]
base=root/'benchmark/next/data';(base/'synthetic').mkdir(parents=True,exist_ok=True)
manifest=root/'benchmark/next/inputs.json'
if manifest.exists():raise SystemExit('Input manifest exists; verify it rather than replacing the benchmark.')
archive=base/'cantrbry.tar.gz'
if archive.exists():
 with tarfile.open(archive) as t:
  dest=base/'canterbury';dest.mkdir(exist_ok=True)
  for member in t.getmembers():
   if not member.isfile():continue
   name=pathlib.PurePosixPath(member.name).name
   assert name and '/' not in name
   (dest/name).write_bytes(t.extractfile(member).read())
count=2*1024*1024
for kind in ['interleaved-u32','quadratic-u32','random-walk-u32','random-u32']:
 rng=random.Random(20261107);values=array.array('I');assert values.itemsize==4;v=123456
 for i in range(count):
  if kind=='interleaved-u32':
   lane=i%8;row=i//8;v=1000*lane+17*row+(lane+1)*(row%7)
  elif kind=='quadratic-u32':v=i*i
  elif kind=='random-walk-u32':
   v=v+rng.randrange(-2,3)
   if i%1024==0:v=rng.getrandbits(32)
  else:v=rng.getrandbits(32)
  values.append(v&0xffffffff)
 if sys.byteorder!='little':values.byteswap()
 (base/'synthetic'/f'{kind}.bin').write_bytes(values.tobytes())
files=[]
def add(path,family,source):
 files.append({'path':str(path.relative_to(root)),'family':family,'source':source,'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
for p in sorted((root/'benchmark/audit/data/silesia').iterdir()):add(p,'regression-silesia','https://sun.aei.polsl.pl/~sdeor/corpus/silesia.zip')
add(root/'benchmark/audit/data/owid-co2-data.csv','regression-table','https://github.com/owid/co2-data')
for p in sorted((base/'canterbury').iterdir()):add(p,'holdout-canterbury','https://corpus.canterbury.ac.nz/resources/cantrbry.tar.gz')
add(base/'mitdb-100.dat','holdout-signal','https://physionet.org/files/mitdb/1.0.0/100.dat')
for p in sorted((base/'synthetic').iterdir()):add(p,'synthetic-numeric','freeze_inputs.py; seed 20261107; explicitly artificial')
manifest.write_text(json.dumps({'date':'2026-11-07','policy':'Freeze before performance evaluation. Holdout means unused in previous Kolmo audit, not globally unseen data. Do not tune on these results and continue calling them held out.','files':files},indent=2)+'\n')
print('Frozen',len(files),'inputs;',sum(x['bytes'] for x in files),'bytes')
