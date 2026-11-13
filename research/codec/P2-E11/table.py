import json,glob,os,sys
import importlib.util
spec=importlib.util.spec_from_file_location('pp','/home/claude/kolmo/research/codec/competitor-policy-v2/policy.py'); pp=importlib.util.module_from_spec(spec); sys.modules['pp']=pp; spec.loader.exec_module(pp)
M=pp.CostModel(); D=sys.argv[1]
C=sys.argv[2].split(',')
tot={c:[0,0] for c in C}
print(f"{'input':20}"+''.join(f"{c[3:] if c.startswith('tf_') else c:>17}" for c in C))
for f in sorted(glob.glob(D+'/*.jsonl')):
    R=[json.loads(l) for l in open(f)]
    size={r['candidate']:r['stored'] for r in R if r['kind'].endswith('_size')}
    runs={}
    for r in R:
        if r['kind'].endswith('_time'): runs.setdefault(r['candidate'],{})[r['pair_id']]=pp.run_cost(r,size[r['candidate']],True,M)
    z=runs['zstd3']; row=''
    for c in C:
        fit=all(runs[c][p]<=z[p] for p in z); med=sorted(runs[c].values())[2]/sorted(z.values())[2]-1
        b=size[c]/size['zstd3']-1
        tot[c][0]+=fit and b<0; tot[c][1]+=fit
        row+=f"{100*b:+7.1f}/{100*med:+5.0f}{'*' if fit else ' '}".rjust(17)
    print(f"{os.path.basename(f)[:-6][:20]:20}"+row)
print('fit&smaller / fit:', {c:tuple(v) for c,v in tot.items()})
