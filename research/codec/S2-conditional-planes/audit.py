#!/usr/bin/env python3
"""Independently validate S2 ledgers and compute complete-input costs."""
import argparse,hashlib,json,pathlib,statistics,lzma,csv,io
from run import ARMS,CONTROLS,PAIRS,validate

def sha(b):return hashlib.sha256(b).hexdigest()
def charged(row):
    wall=row['wall_ns']/row['batch']/1e9;cpu=row['cpu_ns']/row['batch']/1e9
    return cpu if cpu>1.03*wall else wall

def main():
    p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);a=p.parse_args();root=a.root
    inputs=json.loads((root/'s1/evidence/INPUTS.json').read_text());summary={};raw=[];ident={};comparisons=0
    for passid,(pas,nr) in enumerate([('first',7),('confirmation',5)]):
        ss={};status=json.loads((root/pas/'STATUS.json').read_text())
        if status['status']!='PASS_DEVELOPMENT_NOT_RELEASE' or status['rounds']!=nr:raise ValueError('incomplete run')
        for idx,inp in enumerate(inputs):
            name=inp['name'];x=(root/'s1/inputs'/name).read_bytes()
            if sha(x)!=inp['sha256'] or len(x)!=inp['bytes']:raise ValueError('input identity')
            dest=root/pas/name;values=[json.loads(line) for line in (dest/'stdout.jsonl').read_text().splitlines()]
            meta={r['arm']:r for r in values if r['kind']=='frame'};rows=[r for r in values if r['kind']=='sample']
            validate(rows,meta,nr,len(x));by={(r['arm'],r['op'],r['round']):r for r in rows};per={}
            for arm in ARMS:
                f=(dest/(arm+'.frame')).read_bytes();digest=sha(f)
                if len(f)!=meta[arm]['bytes']:raise ValueError('size disagreement')
                key=name+'|'+arm
                if key in ident and ident[key]!=digest:raise ValueError('changed repeat frame')
                ident[key]=digest
                if f!=(root/'sanitized'/name/(arm+'.frame')).read_bytes():raise ValueError('sanitizer mismatch')
                if arm in CONTROLS and f!=(root/'s1/first'/name/(CONTROLS[arm]+'.frame')).read_bytes():raise ValueError('S1 reference changed')
                comparisons+=1
                e=[charged(by[arm,0,r]) for r in range(nr)];d=[charged(by[arm,1,r]) for r in range(nr)]
                costs=[e[r]+d[r]+len(f)/125000000 for r in range(nr)]
                per[arm]={'bytes':len(f),'sha256':digest,'encode':e,'decode':d,'cost':costs,'median':{k:statistics.median(v) for k,v in [('encode',e),('decode',d),('cost',costs)]},'decisions':meta[arm]['decisions']}
            for x,y in PAIRS:
                if per[x]['sha256']!=per[y]['sha256']:raise ValueError('control mismatch')
            ss[name]=per
            for r in rows:raw.append([passid,idx,ARMS.index(r['arm']),r['op'],r['round'],r['batch'],r['input'],r['bytes'],r['wall_ns'],r['cpu_ns']])
        summary[pas]=ss
    reals=[i['name'] for i in inputs if i['kind']=='observed_real'];synth=[i['name'] for i in inputs if i['kind']!='observed_real']
    aggregate={}
    for group,names in [('real',reals),('synthetic',synth)]:
        aggregate[group]={pas:{arm:{'bytes':sum(ss[n][arm]['bytes'] for n in names),**{k:sum(ss[n][arm]['median'][k] for n in names) for k in ['encode','decode','cost']}} for arm in ARMS} for pas,ss in summary.items()}
    wins={arm:[n for n in reals if all(ss[n][arm]['bytes']<ss[n]['zstd3']['bytes'] and all(x<=y for x,y in zip(ss[n][arm]['cost'],ss[n]['zstd3']['cost'])) for ss in summary.values())] for arm in ARMS}
    noise={}
    for x,y in PAIRS:
        noise[x+'__'+y]={k:[min(ss[n][y]['median'][k]/ss[n][x]['median'][k] for n in reals for ss in summary.values()),max(ss[n][y]['median'][k]/ss[n][x]['median'][k] for n in reals for ss in summary.values())] for k in ['encode','decode','cost']}
    decisions={}
    for arm in ['gate_HZ','gate_menu']:
        ds=[d for n in reals for d in summary['first'][n][arm]['decisions']]
        decisions[arm]={'planes':len(ds),'trials':sum(d['tried'] for d in ds),'rejected_full_trials':sum(d['rejected'] for d in ds),'sampled_bytes':sum(d['sampled'] for d in ds),'sample_calls':sum(bool(d['sample_z'])+bool(d['sample_l']) for d in ds),'selected':{k:sum(d['selected']==k for d in ds) for k in ['H','Z','HZ','HL']}}
    profile={}
    for inp in inputs:
        rows=[json.loads(line) for line in (root/'profile'/(inp['name']+'.jsonl')).read_text().splitlines()]
        profile[inp['name']]={a:{k:statistics.median(r[k] for r in rows if r['arm']==a) for k in ['total_ns','typing_ns','forward_ns','codec_ns','crc_ns','codec_calls']} for a in ARMS}
        for v in profile[inp['name']].values():v['remainder_ns']=v['total_ns']-sum(v[k] for k in ['typing_ns','forward_ns','codec_ns','crc_ns'])
    audit={'status':'PASS_OBSERVED_DEVELOPMENT_NOT_RELEASE','inputs':len(inputs),'arms':len(ARMS),'configurations_per_pass':len(inputs)*len(ARMS),'timed_batches':len(raw),'medians_recomputed':len(inputs)*len(ARMS)*6,'frame_instances_compared':len(ident)*3,'original_S1_control_frames':len(inputs)*len(CONTROLS),'aggregate':aggregate,'observed_speed_wins_both_passes':wins,'identical_frame_ratios':noise,'gate_decisions_real':decisions}
    out=root/'audited';out.mkdir(exist_ok=True)
    for name,obj in [('AUDIT',audit),('SUMMARY',summary),('PROFILE',profile)]:
        (out/(name+'.json')).write_text(json.dumps(obj,indent=2)+'\n')
    buf=io.StringIO();w=csv.writer(buf);w.writerow(['pass','input','arm','op','round','batch','input_bytes','stored_bytes','wall_ns','cpu_ns']);w.writerows(raw)
    rawbytes=buf.getvalue().encode();(out/'timings.csv.xz').write_bytes(lzma.compress(rawbytes))
    (out/'schema.json').write_text(json.dumps({'passes':['first','confirmation'],'inputs':[i['name'] for i in inputs],'arms':ARMS,'rows':len(raw),'uncompressed_sha256':sha(rawbytes)},indent=2)+'\n')
    print(json.dumps({'configurations':audit['configurations_per_pass'],'timed_batches':len(raw),'medians':audit['medians_recomputed'],'decisions':decisions,'wins':wins},indent=2))
if __name__=='__main__':main()
