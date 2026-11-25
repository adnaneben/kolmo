"""Compare both target CLIs to frozen full-file B26 direct routes and a stock decoder."""
import argparse,csv,hashlib,json,random,re,subprocess,tempfile
from pathlib import Path
def digest(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser()
    for k in ('before','after','reader','b26','out'):ap.add_argument('--'+k,required=True,type=Path)
    a=ap.parse_args()
    f=json.loads((a.b26/'freeze.json').read_text())
    direct={(r['file'],r['arm']):r for r in csv.DictReader((a.b26/'settings.csv').open()) if r['eligible']=='True'}
    specs=[('encode',1),('encode',20),('encode',60),('decode',1),('decode',30),('decode',80),('encode',999000),('decode',999000)]
    workloads=[(s,k,rate) for s in f['producers'] for k,rate in specs]
    rng=random.Random(2026112526);rng.shuffle(workloads)
    meta={'b26_freeze_sha256':digest(a.b26/'freeze.json'),'b26_settings_sha256':digest(a.b26/'settings.csv'),
          'executables':{k:digest(getattr(a,k)) for k in ('before','after','reader')},'producers':f['producers'],'seed':2026112526,'threads':1}
    (a.out/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n')
    with tempfile.TemporaryDirectory(dir=a.out) as td, (a.out/'holdout.csv').open('x',newline='') as raw:
        t=Path(td);frame=t/'frame';out=t/'decoded';writer=None
        for name,phase,rate in workloads:
            src=Path(f['producers'][name]['source']); sig=digest(src);assert sig==f['producers'][name]['sha256']
            spec=phase+'>='+('999GB/s' if rate==999000 else f'{rate}MB/s')
            threshold=[(setting,r) for (source,setting),r in direct.items() if source==name and float(r[phase+'_MB_s'])>=rate]
            r={'source':name,'constraint':spec,'input_bytes':src.stat().st_size,'input_sha256':sig,'B26_measured_direct_pass':len(threshold),
               'B26_smallest_direct_passing':min((int(x['stored_bytes']) for _,x in threshold),default='')}
            for arm in rng.sample(['before','after'],2):
                p=subprocess.run([getattr(a,arm),'-v','--target',spec,'-T','1','-f',src,'-o',frame],capture_output=True,text=True,check=True)
                first=p.stderr.splitlines()[0];m=re.search(r"1 chunk: ([\w-]+) x1;.*?predicted (\d+) bytes",first);assert m,first
                route=m[1];digest_f=digest(frame); direct_arm=direct[name,route]
                assert int(direct_arm['stored_bytes'])==frame.stat().st_size and direct_arm['frame_sha256']==digest_f
                subprocess.run([a.reader,'-d','-T','1','-q','-f',frame,'-o',out],check=True)
                assert out.read_bytes()==src.read_bytes()
                r[arm+'_route']=route;r[arm+'_predicted_B']=int(m[2]);r[arm+'_actual_B']=frame.stat().st_size;r[arm+'_frame_sha256']=digest_f
                if arm=='after':r['marked_fallback']='no setting met the predicted floors for 1 chunk' in first
            assert r['before_route']==r['after_route'] and r['before_frame_sha256']==r['after_frame_sha256']
            if r['marked_fallback']: assert r['before_predicted_B']==0 and r['after_predicted_B']==r['after_actual_B']
            else: assert r['before_predicted_B']==r['after_predicted_B']
            selected=direct[name,r['after_route']]
            r['selected_B26_direct_MB_s']=selected[phase+'_MB_s'];r['selected_B26_measured_pass']=float(selected[phase+'_MB_s'])>=rate
            if writer is None:
                fields=['source','constraint','input_bytes','input_sha256','B26_measured_direct_pass','B26_smallest_direct_passing']
                fields += [arm+'_'+k for arm in ['before','after'] for k in ['route','predicted_B','actual_B','frame_sha256']]
                fields+=['marked_fallback','selected_B26_direct_MB_s','selected_B26_measured_pass']
                writer=csv.DictWriter(raw,fieldnames=fields);writer.writeheader()
            writer.writerow(r);raw.flush()
if __name__=='__main__':main()
