"""Recompute the decision and render its single chart from the raw trials."""
import csv
import json
from pathlib import Path
from statistics import median
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.patches import Rectangle
from common import OUT

def summarize(trials,codec):
    rows=[t for t in trials if t['codec']==codec and t['repeat']>0]
    assert sorted(t['repeat'] for t in rows)==list(range(1,8))
    return dict(bytes=rows[0]['compressed'],**{
        stage+'_'+metric:op(t[stage][metric] for t in rows)
        for stage in ('encode','decode') for metric,op in [('seconds',median),('peak_rss_bytes',max)]})

def scoreboard():
    records=json.loads((OUT/'holdout-runs.json').read_text())
    inputs=json.loads((OUT/'inputs.json').read_text())
    results={}
    for name,r in records.items():
        assert len(r['trials'])==32 and all(t['decoded_sha256']==inputs[name]['test_sha256'] for t in r['trials'])
        s={c:summarize(r['trials'],c) for c in ('kolmo','release','zstd','openzl')}
        gates={}
        for rival in ['zstd','openzl']:
            metrics={k:s['kolmo'][k]/v for k,v in s[rival].items()}
            flags={k:s['kolmo'][k]*5<=v*4 if k=='bytes' else s['kolmo'][k]<=v for k,v in s[rival].items()}
            gates[rival]=dict(ratios=metrics,flags=flags,passed=all(flags.values()))
        results[name]=dict(metrics=s,gates=gates,passed=all(v['passed'] for v in gates.values()))
    return results

def draw(results):
    ink='#172f3d';muted='#4e6572';green='#157161';red='#a24529'
    fig,ax=plt.subplots(figsize=(14,6.6))
    fig.subplots_adjust(0,0,1,1)
    ax.set(xlim=(0,1),ylim=(0,1));ax.axis('off')
    def txt(x,y,s,**opts):
        opts.setdefault('color',ink);opts.setdefault('fontsize',11);ax.text(x,y,s,va='center',**opts)
    count=sum(r['passed'] for r in results.values())
    txt(.035,.928,'Can Kolmo fast save 20% at the same time and memory cost?',fontsize=23,weight='bold')
    txt(.035,.862,f'{count} of 3 datasets passed   •   {"Target met" if count>=2 else "Target missed"} (2 required)',fontsize=16,weight='bold',color=green if count>=2 else red)
    txt(.035,.800,'Each row compares Kolmo with one incumbent. A dataset must pass all five checks against both.',color=muted)
    xs=[.30,.462,.581,.704,.808]
    for x,label in zip(xs,['Size saved','Encode time','Decode time','Encode RSS','Decode RSS']):
        txt(x,.712,label,fontsize=10,fontweight='bold',ha='center')
    txt(.035,.712,'Dataset / comparator',fontsize=10,weight='bold');txt(.938,.712,'Result',fontsize=10,ha='center',weight='bold')
    ds={'power':'Household electricity','rides':'Chicago bike trips','quakes':'USGS earthquakes'}
    ax.plot([.035,.97],[.681,.681],color=muted,lw=.6)
    for j,name in enumerate(ds):
        r=results[name]
        y=.62-j*.162
        txt(.035,y,ds[name],fontsize=11,weight='bold')
        for idx,(codec,label) in enumerate([('zstd','Zstd −3'),('openzl','Trained OpenZL')]):
            yy=y-.067*idx
            txt(.212,yy,label,fontsize=9,ha='right',color=muted)
            gate=r['gates'][codec]
            for x,key in zip(xs,('bytes','encode_seconds','decode_seconds','encode_peak_rss_bytes','decode_peak_rss_bytes')):
                val=gate['ratios'][key]
                s=f'{100*(1-val):+.1f}%' if key=='bytes' else f'{val:.2f}×'
                ok=gate['flags'][key]
                ax.add_patch(Rectangle((x-.046,yy-.025),.092,.05,color='#e5f2ee' if ok else '#faeae4',lw=0))
                txt(x,yy,s,ha='center',weight='bold',color=green if ok else red)
            if idx==0: txt(.938,yy-.032,'PASS' if r['passed'] else 'MISS',ha='center',weight='bold',color=green if r['passed'] else red)
        ax.plot([.035,.97],[y-.107]*2,color='#d9e1e4',lw=.6)
    txt(.035,.128,'Size saved must reach +20%. Each time and memory ratio must be ≤1.00×. Green meets the limit. Decisions use unrounded values.',fontsize=10)
    txt(.035,.077,'Apple M5   •   1 codec worker   •   7 timed file-to-file runs after warm-up   •   median wall time and highest process peak RSS in each direction',fontsize=10,color=muted)
    fig.savefig(OUT/'decision.png',dpi=155)
    plt.close(fig)

if __name__=='__main__':
    result=scoreboard()
    (OUT/'decision.json').write_text(json.dumps(result,indent=2)+'\n')
    draw(result)
