"""Replay bounded same-cell checks chosen by the predeclared main/AA adverse signal."""
import argparse,csv,json,os,random,shutil,tempfile
from pathlib import Path
from collections import defaultdict
from statistics import median
from replay import run,digest,SOURCES,ARMS
from score import score

def main():
    ap=argparse.ArgumentParser()
    for k in ['before','after','corpus','out']:ap.add_argument('--'+k,type=Path,required=True)
    args=ap.parse_args()
    summary=score(args.out/'rounds.csv',3,args.out/'score.csv')
    # Strongest doubly-negative cells, without repeating the same input/level. Include the largest
    # positively-moving cell to remeasure the effect of the PCM gate in the same shuffled run.
    adverse=[r for r in summary if max(r['gain_paired_pct'],r['gain_vs_AA_paired_pct']) < -5
             and max(r['gain_wins'],r['gain_vs_AA_wins'])<=1]
    adverse.sort(key=lambda r:max(r['gain_paired_pct'],r['gain_vs_AA_paired_pct']))
    chosen=[]
    for r in adverse:
        if all((r['input'],r['level'])!=(s['input'],s['level']) for s in chosen):chosen.append(r)
        if len(chosen)==3:break
    chosen.append(max(summary,key=lambda r:min(r['gain_paired_pct'],r['gain_vs_AA_paired_pct'])))
    selection=[(r['input'],r['level'],r['threads']) for r in chosen]
    (args.out/'confirmation-selection.json').write_text(json.dumps({'selection':selection,'seed':1156651,'scored_repetitions':11,'selection_method': 'Up to three distinct input/level cells >=5% slower against both control arms and at most one win in each three-round comparison, by worst common slowdown. One strongest positive control.'},indent=2)+'\n')
    bins={'main':args.before.resolve(),'main-AA':args.before.resolve(),'candidate':args.after.resolve()}
    corpus={s:(args.corpus/file).read_bytes()[:limit] for file,limit,s in SOURCES}
    rng=random.Random(1156651)
    with tempfile.TemporaryDirectory(dir=args.out) as td,(args.out/'confirmation-rounds.csv').open('x',newline='',buffering=1) as of:
        td=Path(td)
        for k,v in corpus.items(): (td/k).write_bytes(v)
        w=None
        for rep in range(12):
            cells=list(selection);rng.shuffle(cells)
            for sample,level,threads in cells:
                for order,arm in enumerate(rng.sample(ARMS,3)):
                    frame=td/'frame';restored=td/'restored';n=len(corpus[sample]);sig=digest(corpus[sample]);load=os.getloadavg()[0]
                    enc=run([bins[arm],'--level',level,'-T',threads,'-q','-f',td/sample,'-o',frame])
                    dec=run([bins[arm],'-d','-T',threads,'-q','-f',frame,'-o',restored])
                    assert restored.read_bytes()==corpus[sample]
                    framed=frame.read_bytes()
                    r={'input':sample,'level':level,'threads':threads,'rep':rep,'order':order,'arm':arm,'input_sha256':sig,'bytes_in':n,'bytes_out':len(framed),'frame_sha256':digest(framed),'restored_sha256':sig,'encode_ns':enc[0],'decode_ns':dec[0],'encode_cpu_s':enc[1],'decode_cpu_s':dec[1],'encode_rss_B':enc[2],'decode_rss_B':dec[2],'load1':load}
                    if w is None:w=csv.DictWriter(of,fieldnames=r);w.writeheader()
                    w.writerow(r)
    score(args.out/'confirmation-rounds.csv',11,args.out/'confirmation-score.csv')
if __name__=='__main__':main()
