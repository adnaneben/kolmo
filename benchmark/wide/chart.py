"""Render the held-out, single-worker decision from measured metrics."""
import json
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

HERE = Path(__file__).resolve().parent
CODECS = ["candidate", "baseline", "zstd", "openzl"]
LABELS = ["Kolmo fast · wide tables", "Kolmo fast · PR #2", "Zstd -3", "Trained OpenZL"]


def draw():
    decision = json.loads((HERE / "decision.json").read_text())
    freeze = json.loads((HERE / "freeze.json").read_text())
    size = freeze["inputs"]["test_bytes"]
    m = decision["metrics"]
    ink, muted, bg = "#15303a", "#506773", "#fafbf9"
    enc, dec = "#306c81", "#a6cbd5"
    fig, axes = plt.subplots(1, 3, figsize=(13.8, 5.1), gridspec_kw={"width_ratios":[1, 1.3, 1.25]})
    fig.subplots_adjust(left=.165, right=.97, top=.70, bottom=.25, wspace=.28)
    fig.patch.set_facecolor(bg)
    fig.text(.035,.90,"Kolmo fast vs Zstd on Portuguese meter readings",size=21,weight="bold",color=ink)
    fig.text(.035,.825,f"{size/1e6:,.1f} MB held out   ·   370 meters   ·   One Apple M5 CPU worker   ·   Seven timed rounds",
             size=11,color=muted)
    for ax, title in zip(axes, ["Compression ratio · higher wins", "Time · ms, lower wins", "Peak memory · MiB, lower wins"]):
        ax.set_facecolor(bg)
        ax.set_title(title,loc="left",fontsize=11,weight="bold",color=ink,pad=10)
        ax.set_ylim(3.54,-.54)
        ax.spines[:].set_visible(False)
        ax.grid(axis="x",linewidth=.7,color="#dce3e1")
        ax.set_axisbelow(True)
        ax.tick_params(length=0,labelsize=8,colors=muted)
        ax.set_yticks(range(4), labels=[""]*4)
        ax.locator_params(axis="x",nbins=4)
    axes[0].set_yticklabels(LABELS,fontsize=10,color=ink)
    axes[0].get_yticklabels()[0].set_weight("bold")
    def label(ax,x,y,text):
        ax.annotate(text,(x,y),xytext=(4,0),textcoords="offset points",va="center",fontsize=9,color=ink)
    for i,k in enumerate(CODECS):
        r = m[k]
        ratio = size / r["size"]
        axes[0].barh(i,ratio,height=.38,color=["#137968","#899ba1","#4c7f9f","#ac7649"][i])
        label(axes[0], ratio,i,f"{ratio:.2f}×")
        e,d = [r["seconds_"+s]*1000 for s in ("encode","decode")]
        axes[1].barh(i,e,height=.38,color=enc)
        axes[1].barh(i,d,left=e,height=.38,color=dec)
        label(axes[1],e+d,i,f"{e:,.0f} + {d:,.0f}")
        for dy,s,col in [(-.15,"encode",enc),(.15,"decode",dec)]:
            v=r["peak_rss_bytes_"+s]/2**20
            axes[2].barh(i+dy,v,height=.23,color=col)
            label(axes[2],v,i+dy,f"{v:,.1f}")
    for ax, factor in zip(axes,[1.28,1.58,1.30]):
        ax.set_xlim(0,ax.dataLim.xmax*factor)
    fig.legend(handles=[Patch(color=enc,label="Encode"),Patch(color=dec,label="Decode")],
               loc="center",bbox_to_anchor=(.53,.16),frameon=False,ncol=2,fontsize=9)
    win=decision["zstd_pass"]
    fig.text(.035,.088,("PASS" if win else "MISS")+"   20% smaller than Zstd with no increase in encode/decode time or peak memory.",
             color="#166d5e" if win else "#95472f",size=11,weight="bold")
    fig.text(.035,.03,"Ratio = original bytes ÷ complete frame bytes. Time includes file I/O. OpenZL training is excluded. Peaks are maxima across scored runs.",
             size=8.8,color=muted)
    fig.savefig(HERE/"decision.png",dpi=160,facecolor=bg)
    plt.close(fig)


if __name__ == "__main__":
    draw()
