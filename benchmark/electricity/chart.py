"""Render the single decision figure from the frozen trials."""
import json
from pathlib import Path
from statistics import median
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

HERE = Path(__file__).resolve().parent
COLORS = ["#116C60", "#86989F", "#4179A3", "#B57844"]
LABELS = ["Kolmo fast · candidate", "Kolmo fast · merged", "Zstd -3", "Trained OpenZL"]
CODECS = ["candidate", "merged", "zstd", "openzl"]


def summarize(rows):
    results = {}
    for name in CODECS:
        scored = [r for r in rows if r["repeat"] and r["codec"] == name]
        assert sorted(r["repeat"] for r in scored) == list(range(1, 8))
        results[name] = dict(size=scored[0]["compressed"], **{
            key + "_" + stage: op(r[stage][key] for r in scored)
            for key, op in (("seconds", median), ("peak_rss_bytes", max)) for stage in ("encode", "decode")})
    return results


def draw():
    freeze = json.loads((HERE / "freeze.json").read_text())
    data = json.loads((HERE / "trials.json").read_text())
    stats = summarize(data["trials"])
    candidate = stats["candidate"]
    gates = {name: {k: candidate[k] * 5 <= v * 4 if k == "size" else candidate[k] <= v
                    for k, v in stats[name].items()} for name in ["zstd", "openzl"]}
    passed = all(all(flags.values()) for flags in gates.values())
    (HERE / "decision.json").write_text(json.dumps(dict(passed=passed, tests=gates, metrics=stats), indent=2) + "\n")
    fig, axes = plt.subplots(1, 3, figsize=(13.2, 4.55), gridspec_kw={"width_ratios":[1.05, 1.30, 1.35]})
    fig.subplots_adjust(left=.155, right=.98, top=.72, bottom=.235, wspace=.27)
    fig.patch.set_facecolor("#FAFBF9")
    fig.text(.034,.90,"Kolmo fast on European electricity data",fontsize=22,weight="bold",color="#172D34")
    size_mb = freeze["inputs"]["test_bytes"] / 1e6
    fig.text(.035,.815,f"{size_mb:.1f} MB held out   ·   One Apple M5 CPU worker   ·   Seven measured rounds   ·   Lower is better",
             fontsize=10.8,color="#486069")
    for ax, title in zip(axes, ["Compressed file · MB", "Time · ms", "Peak memory · MiB"]):
        ax.set_facecolor("#FAFBF9")
        ax.set_title(title,fontsize=12,weight="bold",loc="left",pad=9,color="#253E45")
        ax.set_ylim(3.55,-.55)
        ax.spines[:].set_visible(False)
        ax.set_axisbelow(True)
        ax.grid(axis="x",color="#DDE4E1",lw=.65)
        ax.tick_params(length=0,labelsize=8,colors="#607179")
        ax.locator_params(axis="x",nbins=4)
        ax.set_yticks(range(4))
        ax.set_yticklabels([])
    axes[0].set_yticklabels(LABELS,fontsize=10,color="#253E45")
    axes[0].get_yticklabels()[0].set_weight("bold")
    for i, code in enumerate(CODECS):
        r = stats[code]
        size = r["size"] / 1e6
        axes[0].barh(i, size, height=.38, color=COLORS[i])
        axes[0].annotate(f"{size:.2f}", (size,i), xytext=(4,0), textcoords="offset points",va="center",fontsize=9)
        e, d = (r["seconds_" + s]*1e3 for s in ("encode","decode"))
        axes[1].barh(i,e,height=.38,color="#396F83")
        axes[1].barh(i,d,left=e,height=.38,color="#A6C9D2")
        axes[1].annotate(f"{e:.0f} + {d:.0f}", (e+d,i), xytext=(4,0), textcoords="offset points",va="center",fontsize=9)
        for offset,stage,color in [(-.15,"encode","#396F83"),(.15,"decode","#A6C9D2")]:
            rss = r["peak_rss_bytes_" + stage]/2**20
            axes[2].barh(i+offset,rss,height=.24,color=color)
            axes[2].annotate(f"{rss:.1f}",(rss,i+offset),xytext=(4,0),textcoords="offset points",va="center",fontsize=8)
    for ax,extra in zip(axes,[1.23,1.47,1.23]):
        ax.set_xlim(0, ax.dataLim.xmax * extra)
    fig.legend(handles=[Patch(color="#396F83",label="Encode"),Patch(color="#A6C9D2",label="Decode")],
               loc="upper left",bbox_to_anchor=(.40,.225),ncol=2,frameon=False,fontsize=9)
    fig.text(.035,.095,("PASS" if passed else "MISS")+"   ≥20% smaller than both rivals, within each rival’s encode/decode time and peak memory.",
             fontsize=11,color="#116C60" if passed else "#93472F",weight="bold")
    fig.text(.035,.037,"Times are medians including file I/O. Memory is the largest process peak in each direction. OpenZL training is excluded.",
             fontsize=9,color="#556B73")
    fig.savefig(HERE / "decision.png",dpi=170,facecolor=fig.get_facecolor())
    plt.close(fig)


if __name__ == "__main__":
    draw()
