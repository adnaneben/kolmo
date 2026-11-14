"""Recreate the preregistered RC1 pilot inputs from delivered, hashed archives."""
import hashlib,io,json,pathlib,zipfile,argparse
P=argparse.ArgumentParser();P.add_argument('--archives',type=pathlib.Path,default=pathlib.Path('/mnt/data'));P.add_argument('--output',type=pathlib.Path,required=True);a=P.parse_args();o=a.output;o.mkdir(parents=True,exist_ok=False);rows=[]
def sha(b):return hashlib.sha256(b).hexdigest()
def put(name,b,group,origin,notes=''):
 p=o/name;p.write_bytes(b);rows.append(dict(id=p.stem,file=name,group=group,origin=origin,notes=notes,input_bytes=len(b),input_sha256=sha(b)))
z=zipfile.ZipFile(a.archives/'Kolmo_Performance_Memory_Evidence.zip')
for n in sorted(z.namelist()):
 if n.startswith('inputs/') and not n.endswith('/'):
  p=pathlib.Path(n);put(p.name,z.read(n),'Numeric arrays' if p.suffix=='.npy' else 'Regular CSV','M1/'+n)
z=zipfile.ZipFile(a.archives/'Kolmo_C1_E27_43407137403.zip')
for key in ['CNAE9','Wage','diamonds','segmentationData']:
 matches=[n for n in z.namelist() if n.startswith('prepared/inputs/') and ('__'+key+'_') in n];assert len(matches)==1,(key,matches)
 put(key+'.csv',z.read(matches[0]),'CSV regression controls','E27/'+matches[0])
p=a.archives/'Kolmo_RC1_Corpus_43442527737.zip';assert sha(p.read_bytes())=='83a0963c66061f98df36112a1d78590b15a137d854627e85125a0e54d6964848'
z=zipfile.ZipFile(p);s=zipfile.ZipFile(io.BytesIO(z.read('silesia.zip')))
for n in sorted(s.namelist()):
 if n.endswith('/'):continue
 b=s.read(n);put('silesia_'+n+'_1MiB.bin',b[:1048576],'Mixed corpus prefixes','Silesia/'+n,'First min(1 MiB, file length) of '+str(len(b))+' original bytes; NOT full-corpus evidence')
n=sorted(s.namelist())[0];put('small_'+n+'_64KiB.bin',s.read(n)[:65536],'Small six-mode control','Silesia/'+n,'Overlapping 64 KiB prefix; separate cohort, not independent data')
p=a.archives/'Kolmo_RC1_Logs_43444873722.zip';assert sha(p.read_bytes())=='8e57ae2661030a3b9898c9e28e33c2c1c6d5502962ec501b790acc149a142f89'
z=zipfile.ZipFile(p)
for n in sorted(z.namelist()):
 if n.endswith('.log'):put(pathlib.Path(n).name,z.read(n),'Log samples','Loghub dd61d0952749ee7963bde24220d1be5ede023033/'+n,'Complete published 2k-line sample, not complete production log')
p=a.archives/'Kolmo_RC1_Audio_43438206296.zip';assert sha(p.read_bytes())=='66c4b5defb3695976d9af600eed1eed8ce809eeb4e92513ab8fbb39b26857496'
z=zipfile.ZipFile(p)
for tag in ['amnist','augm']:
 ns=sorted(n for n in z.namelist() if n.startswith('prepared/inputs/') and ('__'+tag+'__') in n)
 for n in ns[:2]:put(pathlib.Path(n).name,z.read(n),'Audio samples','E31/'+n,'First two lexicographic filenames per source; fixtures are not independent natural recordings')
(o/'manifest.json').write_text(json.dumps(rows,indent=2)+'\n');print(len(rows),sum(x['input_bytes'] for x in rows))
