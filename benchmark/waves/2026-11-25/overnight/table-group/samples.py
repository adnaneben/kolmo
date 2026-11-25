"""Score both group policies on three header-preserving row-aligned samples per whole CSV."""
import argparse,csv,hashlib,json,os,subprocess,tempfile
from pathlib import Path
a=argparse.ArgumentParser();a.add_argument('--bin',type=Path,required=True);a.add_argument('--reader',type=Path,required=True);a.add_argument('--out',type=Path,required=True);a=a.parse_args()
meta=json.loads((a.out/'manifest.json').read_text());span=256<<10
with tempfile.TemporaryDirectory(dir=a.out) as td,(a.out/'samples.csv').open('x',newline='') as raw:
    t=Path(td);included=t/'table.csv';frame=t/'frame';restored=t/'out';w=csv.writer(raw)
    w.writerow('source location original_offset body_bytes header_bytes input_bytes input_sha256 chosen_B no_group_B chosen_sha256 no_group_sha256'.split())
    for source in ['owid','noaa']:
        b=Path(meta['sources'][source]['path']).read_bytes();assert hashlib.sha256(b).hexdigest()==meta['sources'][source]['sha256']
        header=b[:b.index(b'\n')+1]
        for label,base in [('start',len(header)),('middle',(len(b)-span)//2),('end',len(b)-span)]:
            lo=base if label=='start' else b.index(b'\n',base)+1
            hi=len(b) if label=='end' else b.rindex(b'\n',lo,min(lo+span,len(b)))+1
            sample=header+b[lo:hi];included.write_bytes(sample);sizes=[];hashes=[]
            for off in (False,True):
                env=os.environ.copy();env.pop('KOLMO_B40_NO_GROUP',None);env.pop('KOLMO_B40_TRACE',None)
                if off:env['KOLMO_B40_NO_GROUP']='1'
                subprocess.run([a.bin,'-3','-T','1','-f','-q',included,'-o',frame],check=True,env=env)
                subprocess.run([a.reader,'-d','-T','1','-f','-q',frame,'-o',restored],check=True)
                assert restored.read_bytes()==sample
                payload=frame.read_bytes();sizes.append(len(payload));hashes.append(hashlib.sha256(payload).hexdigest())
            w.writerow([source,label,lo,hi-lo,len(header),len(sample),hashlib.sha256(sample).hexdigest(),*sizes,*hashes])
