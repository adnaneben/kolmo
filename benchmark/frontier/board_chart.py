"""Plot completed whole-Silesia lzbench measurements using separate ratio and throughput axes."""
import argparse
import json
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MaxNLocator,FuncFormatter
from matplotlib.patches import Patch

from board_chart_labels import LABELS

def render(work,out):
    board={f.stem:json.loads(f.read_text()) for f in (work/"board").glob("*.json")}
    rows={k:r for k,r in board.items() if r.get("valid")}
    m=work/"m1-panel/whole-silesia.m1_fast.json"
    if m.exists():
        r=json.loads(m.read_text())
        if r.get("valid"):rows["m1_fast"]=r
    leaders=set()
    for key,func in (("bytes",min),("encode",max),("decode",max)):
        leaders.add(func((k for k in rows if k in board and not k.startswith("kolmo") and k not in ("kbc","krc")),key=lambda k:rows[k][key]))
    order=[k for k in LABELS if k in rows and (k in leaders or k.startswith("kolmo") or k in
           ("m1_fast","kbc","krc","lz4","zstd3","zstd19","openzl","brotli11","zpaq","bsc","kanzi9"))]
    order+=sorted(leaders-set(order))
    names=[LABELS.get(k,k) for k in order]
    n=len(order);ink="#172f43";muted="#466174";ratio_color="#435f74";accent="#006e62";enc="#d8753b";dec="#a0c6da"
    fig,(ax,sp,tab)=plt.subplots(1,3,figsize=(12,2.4+0.39*n),gridspec_kw={"width_ratios":[1.04,1.95,.78],"wspace":.10})
    fig.subplots_adjust(left=.205,right=.98,top=1-1.54/fig.get_figheight(),bottom=.63/fig.get_figheight())
    fig.text(.035,.958,"Whole Silesia archive",weight="bold",fontsize=21,color=ink,va="top")
    fig.text(.036,.958-.44/fig.get_figheight(),
             f"211.95 MB  ·  Apple M5  ·  one worker  ·  {len(board)}/52 baseline settings measured",fontsize=11,color=muted,va="top")
    ratios=[211947520/rows[k]["bytes"] for k in order]
    ax.barh(range(n),ratios,color=[accent if k.startswith(("m1","kolmo")) or k in ("kbc","krc") else ratio_color for k in order],height=.58)
    es=[rows[k]["encode"] for k in order];ds=[rows[k]["decode"] for k in order]
    sp.barh(range(n),es,height=.58,color=enc);sp.barh(range(n),ds,left=es,height=.58,color=dec)
    for i,v in enumerate(ratios):ax.text(v+.06,i,f"{v:.2f}×",va="center",color=ink,fontsize=8)
    for i,(e,d) in enumerate(zip(es,ds)):
        tab.text(.47,i,f"{e:,.1f}",ha="right",va="center",fontsize=8.5,color=ink)
        tab.text(1.,i,f"{d:,.1f}",ha="right",va="center",fontsize=8.5,color=ink)
    ax.set_xlim(0,max(ratios)*1.25);sp.set_xlim(0,max(e+d for e,d in zip(es,ds))*1.04)
    for a,title in ((ax,"Compression ratio"),(sp,"Encode and decode speed · MB/s"),(tab,"Encode       Decode")):
        a.set_ylim(n-.45,-.55);a.set_title(title,loc="left",fontsize=10,pad=12,color=ink,weight="bold")
        a.set_axisbelow(True);a.tick_params(length=0,labelsize=8,colors=muted)
        for s in a.spines.values():s.set_visible(False)
        a.set_yticks(range(n));a.set_yticklabels([])
    ax.set_yticklabels(names,fontsize=9,color=ink);tab.set_xlim(0,1);tab.set_xticks([])
    for a in (ax,sp):
        a.xaxis.set_major_locator(MaxNLocator(4));a.grid(axis="x",color="#d7e0e4",lw=.65)
    sp.xaxis.set_major_formatter(FuncFormatter(lambda x,p:f"{x:,.0f}"))
    fig.legend(handles=[Patch(color=enc,label="Encode MB/s"),Patch(color=dec,label="Decode MB/s")],frameon=False,ncol=2,
               bbox_to_anchor=(.975,.98),loc="upper right",fontsize=9)
    fig.text(.036,.04,"Higher is better within each metric. Each colored segment measures its own phase. Round-trip time equals encode time plus decode time.",
             fontsize=9,color=muted)
    fig.savefig(out,dpi=155,facecolor="white")
    plt.close(fig)

if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);p.add_argument("output",type=Path)
    a=p.parse_args();render(a.work,a.output)
