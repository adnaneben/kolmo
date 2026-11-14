#!/usr/bin/env python3
"""Read-only reanalysis of archived E03 native frames using exact pinned leaves.
No selector tuning, performance measurement, production edit, or fresh holdout.
Plane payload sizes include their existing HUF/rANS chunk metadata; whole-file
experimental totals reuse the archived E2C1 overhead and are NOT CZN1 results.
"""
from __future__ import annotations
import argparse,collections,hashlib,json,math,struct,subprocess,zipfile
from pathlib import Path
EXPECTED='5d02cee72dbbbc8a396e85e29c2cb3f5bf91f26abd2e808995b9a54205660e82'
def sha(b:bytes)->str:return hashlib.sha256(b).hexdigest()
def entropy(data:bytes)->float:
 if not data:return 0.0
 counts=collections.Counter(data);n=len(data)
 return -sum((c/n)*math.log2(c/n) for c in counts.values())
def plane_stats(data:bytes,planes:int)->dict:
 if not planes or len(data)%planes:raise ValueError('invalid planes')
 n=len(data)//planes;parts=[data[i*n:(i+1)*n] for i in range(planes)]
 ideal=(len(data)*entropy(data)-sum(len(x)*entropy(x) for x in parts))/8
 return {'planes':planes,'constant_planes':sum(len(set(x))==1 for x in parts),
         'zero_fraction':data.count(0)/len(data) if data else 0,
         'ideal_plane_conditioning_bytes':max(0,ideal),
         'ideal_gain_caveat':'empirical order-0 entropy difference; excludes tables/framing; not compressed output'}
def validate_zip(z:zipfile.ZipFile)->int:
 names=z.namelist()
 if len(names)!=len(set(names)):raise ValueError('duplicate ZIP members')
 manifest=json.loads(z.read('MANIFEST.json'))
 for name,h in manifest.items():
  if name.startswith('/') or '..' in Path(name).parts:raise ValueError('unsafe path')
  if sha(z.read(name))!=h:raise ValueError(f'manifest mismatch: {name}')
 return len(manifest)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--archive',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 archive=a.archive.read_bytes()
 if sha(archive)!=EXPECTED:raise ValueError('unexpected archive')
 out=a.output.resolve();out.mkdir(parents=True,exist_ok=False);probe=a.probe.resolve();rows=[]
 with zipfile.ZipFile(a.archive) as z:
  count=validate_zip(z);records=[json.loads(s) for s in z.read('first/measurements.jsonl').decode().splitlines()]
  records=[r for r in records if r['policy']=='native_base']
  if len(records)!=58:raise ValueError('incomplete baseline')
  for r in records:
   frame=z.read('first/'+r['frame_file'])
   if sha(frame)!=r['frame_sha256'] or frame[:6]!=b'E2C1\x01\x00':raise ValueError('frame mismatch')
   raw,pn,rn,replen,cn,reslen=struct.unpack_from('<6Q',frame,8)
   if len(frame)!=64+pn+rn+cn or replen!=reslen:raise ValueError('size mismatch')
   params=frame[64:64+pn];rest=frame[64+pn:64+pn+rn];payload=frame[64+pn+rn:]
   if sha(params)!=r['capture']['params'] or sha(rest)!=r['capture']['rest']:raise ValueError('metadata mismatch')
   temp=out/'payload.bin';temp.write_bytes(payload);res=out/'residual.bin'
   subprocess.run([str(probe),'decode',str(frame[7]),str(temp),str(reslen),str(res)],check=True,timeout=30)
   residual=res.read_bytes()
   if sha(residual)!=r['capture']['residual']:raise ValueError('old decoder mismatch')
   planes=params[2] if params[1]&8 else 1
   cp=subprocess.run([str(probe),str(res),str(planes)],check=True,capture_output=True,text=True,timeout=30)
   sizes=json.loads(cp.stdout);overhead=64+pn+rn
   def valid(k):return sizes[k] if sizes[k]>0 else reslen
   old=overhead+min(reslen,valid('huf_original_payload'),valid('rans_original_payload'))
   if old!=r['stored_bytes']:raise ValueError('frozen baseline not reproduced')
   huf=overhead+min(reslen,valid('huf_original_payload'))
   aligned_huf=overhead+min(reslen,valid('huf_plane_aligned_payload'))
   fixed_rans=overhead+min(reslen,valid('rans_singleton_enabled_payload'))
   aligned_rans=overhead+min(reslen,valid('rans_plane_aligned_payload'))
   offline=overhead+min(reslen,valid('huf_original_payload'),valid('huf_plane_aligned_payload'),valid('rans_original_payload'),valid('rans_singleton_enabled_payload'),valid('rans_plane_aligned_payload'))
   rows.append({k:r[k] for k in ['id','source','family','kind','size_class','input_bytes']} | sizes | plane_stats(residual,planes) | {'baseline_bytes':old,'huf_original_frame_bytes':huf,'huf_aligned_frame_bytes':aligned_huf,'rans_singleton_frame_bytes':fixed_rans,'rans_aligned_frame_bytes':aligned_rans,'offline_best_compatible_frame_bytes':offline,'residual_sha256':sha(residual),'scope':'frozen residual and unchanged shared E2C1 overhead; no selector/timing/production integration'})
  (out/'payload.bin').unlink();(out/'residual.bin').unlink()
 groups={}
 for label,pred in {'C_real':lambda r:r['source']=='C' and r['kind']=='real_source_derived','D_full':lambda r:r['source']=='D' and r['size_class']=='full','synthetic':lambda r:r['kind']=='synthetic'}.items():
  rr=[r for r in rows if pred(r)];keys=['baseline_bytes','huf_original_frame_bytes','huf_aligned_frame_bytes','rans_singleton_frame_bytes','rans_aligned_frame_bytes','offline_best_compatible_frame_bytes']
  groups[label]={'cases':len(rr),'totals':{k:sum(r[k] for r in rr) for k in keys},'multi_byte_cases':sum(r['planes']>1 for r in rr),'constant_planes':sum(r['constant_planes'] for r in rr),'rans_previously_rejected':sum(r['rans_original_payload']==0 for r in rr)}
 result={'status':'passed','archive_sha256':EXPECTED,'manifest_files_verified':count,'probe_sha256':sha(probe.read_bytes()),'cases':len(rows),'groups':groups,'rows':rows,'limitations':['observed development only','exact pinned leaf functions, not complete production encoder','no timing or selector cost','no new format or defaults','not independent entropy decoding','vendor code uninstrumented in sanitizer test']}
 (out/'AUDIT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(groups,indent=2))
if __name__=='__main__':main()
