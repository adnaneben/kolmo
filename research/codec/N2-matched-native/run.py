#!/usr/bin/env python3
"""Execute/verify the fixed development matrix; no learned or hindsight online selector."""
from __future__ import annotations
import argparse, hashlib, importlib.util, json, math, pathlib, statistics, subprocess, sys, zipfile
ARMS=('zstd3','kolmo_fast','tp_prod3','tp_phuf_pg16','tp_pent','tp_arch19',
      'oz_exact6','oz_exact9','oz_whole9','oz_serial9','zstd3_duplicate','oz_exact9_duplicate')
ARTIFACT_SHA='00ae71bdc5e4f3612d017dfeabb0f5154742feda67bde2be92f1bfeefb50ebe1'
POLICY_SHA='1ff10706bbb737c4b949583ecc99746fb28dcb710c24bc51d382c5f614dee657'
def sha(b):return hashlib.sha256(b).hexdigest()
def load_policy(path):
    if sha(path.read_bytes())!=POLICY_SHA:raise ValueError('active reference policy changed; review before execution')
    spec=importlib.util.spec_from_file_location('n2_policy',path);mod=importlib.util.module_from_spec(spec);sys.modules[spec.name]=mod;spec.loader.exec_module(mod);return mod

def prepare(artifact,out):
    if sha(artifact.read_bytes())!=ARTIFACT_SHA:raise ValueError('R3 ZIP mismatch')
    with zipfile.ZipFile(artifact) as z:
        if len(z.namelist())!=len(set(z.namelist())):raise ValueError('duplicate ZIP member')
        manifest=json.loads(z.read('MANIFEST.json'))
        for name,digest in manifest.items():
            if sha(z.read(name))!=digest:raise ValueError('manifest mismatch: '+name)
        names=sorted(n for n in z.namelist() if n.startswith('prepared/inputs/') and n.endswith('.npy'))
        if len(names)!=8:raise ValueError('unexpected cohort')
        records=[]
        for n in names:
            name=pathlib.PurePosixPath(n).name
            if any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789_.' for c in name):raise ValueError('input name')
            data=z.read(n);p=out/name;p.write_bytes(data);records.append({'name':name,'sha256':sha(data),'bytes':len(data)})
    return records

def validate_rows(rows,rounds):
    expected={(a,o,r) for a in ARMS for o in ('encode','decode') for r in range(rounds)}
    by={}
    sizes={}
    for x in rows:
        k=(x['arm'],x['operation'],x['round'])
        if k not in expected or k in by:raise ValueError('unexpected/duplicate sample')
        for f in ('wall_ns','cpu_ns','batch','input_bytes','stored_bytes','round'):
            if type(x[f]) is not int or x[f]<0:raise ValueError('bad integer '+f)
        if min(x['wall_ns'],x['batch'],x['input_bytes'],x['stored_bytes'])<=0:raise ValueError('zero sample')
        if x['batch']>16:raise ValueError('batch cap')
        shape=(x['stored_bytes'],x['input_bytes'],x['batch'])
        if x['arm'] in sizes and sizes[x['arm']]!=shape:raise ValueError('changed measured shape')
        sizes[x['arm']]=shape;by[k]=x
    if set(by)!=expected:raise ValueError('missing samples')
    if len({v[1:] for v in sizes.values()})!=1:raise ValueError('different input/batch')
    return by

def analyze(rows,rounds,policy):
    by=validate_rows(rows,rounds);out={};model=policy.CostModel();model.validate()
    for arm in ARMS:
        samples=[]
        for r in range(rounds):
            a,b=by[arm,'encode',r],by[arm,'decode',r]
            row={op+'_'+clock+'_s': x[clock+'_ns']/x['batch']/1e9 for op,x in [('encode',a),('decode',b)] for clock in ('wall','cpu')}
            row['speed_cost_s']=policy.run_cost(row,a['stored_bytes'],True,model)
            row['archive_cost_usd']=policy.run_cost(row,a['stored_bytes'],False,model)
            samples.append(row)
        out[arm]={'stored_bytes':by[arm,'encode',0]['stored_bytes'],'runs':samples,
                  'medians':{key:statistics.median(r[key] for r in samples) for key in samples[0]}}
    candidates=['tp_prod3','tp_phuf_pg16','tp_pent','tp_arch19']
    for arm in candidates:
        out[arm]['comparisons']={}
        for rival,cost in [('zstd3','speed_cost_s'),('oz_exact9','archive_cost_usd'),('oz_whole9','archive_cost_usd')]:
            c,o=out[arm],out[rival]
            out[arm]['comparisons'][rival]={'smaller':c['stored_bytes']<o['stored_bytes'],
                  'fits_every_pair':all(x[cost]<=y[cost] for x,y in zip(c['runs'],o['runs'])),
                  'size_ratio':c['stored_bytes']/o['stored_bytes']}
    return out

def main():
    p=argparse.ArgumentParser();p.add_argument('--binary',type=pathlib.Path,required=True);p.add_argument('--artifact',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);p.add_argument('--policy',type=pathlib.Path,required=True)
    a=p.parse_args();out=a.output.resolve();out.mkdir(exist_ok=False);(out/'inputs').mkdir();policy=load_policy(a.policy)
    inputs=prepare(a.artifact,out/'inputs');(out/'INPUTS.json').write_text(json.dumps(inputs,indent=2)+'\n')
    binary=a.binary.resolve();summaries={};identities={};status={'status':'NOT_FINISHED','binary_sha256':sha(binary.read_bytes()),'policy_sha256':POLICY_SHA,'timed_batch_count':0,'all_data_observed':True,'online_selector':False}
    try:
        for passname,rounds,seed in [('first',7,2026111301),('confirmation',5,2026111302)]:
            summaries[passname]={}
            for i,inp in enumerate(inputs):
                dest=out/passname/inp['name'];dest.mkdir(parents=True)
                cmd=[str(binary),str(out/'inputs'/inp['name']),str(dest),str(rounds),str(seed+i)]
                result=subprocess.run(cmd,capture_output=True,text=True,timeout=180)
                (dest/'stdout.jsonl').write_text(result.stdout);(dest/'stderr.log').write_text(result.stderr)
                if result.returncode:raise RuntimeError('native failure: '+inp['name'])
                rows=[json.loads(x) for x in result.stdout.splitlines()];summaries[passname][inp['name']]=analyze(rows,rounds,policy)
                hashes={arm:sha((dest/(arm+'.frame')).read_bytes()) for arm in ARMS}
                for arm in ARMS:
                    if (dest/(arm+'.frame')).stat().st_size!=summaries[passname][inp['name']][arm]['stored_bytes']:raise ValueError('frame length')
                for arm,dup in [('zstd3','zstd3_duplicate'),('oz_exact9','oz_exact9_duplicate')]:
                    if hashes[arm]!=hashes[dup]:raise ValueError('duplicate control bytes differ')
                if passname=='confirmation' and hashes!=identities[inp['name']]:raise ValueError('repeat frame mismatch')
                identities[inp['name']]=hashes;status['timed_batch_count']+=len(rows)
        status['status']='DEVELOPMENT_MATRIX_EXECUTED_NOT_FRESH_ACCEPTANCE'
        (out/'SUMMARY.json').write_text(json.dumps(summaries,indent=2)+'\n')
        (out/'FRAME_HASHES.json').write_text(json.dumps(identities,indent=2)+'\n')
    finally:
        (out/'STATUS.json').write_text(json.dumps(status,indent=2)+'\n')
        m={str(f.relative_to(out)):sha(f.read_bytes()) for f in sorted(out.rglob('*')) if f.is_file() and f.name!='MANIFEST.json'}
        (out/'MANIFEST.json').write_text(json.dumps(m,indent=2)+'\n')
if __name__=='__main__':main()
