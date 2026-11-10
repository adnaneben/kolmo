"""Export the opening local Silesia chart with each measured phase and exact frame size."""
import matplotlib
matplotlib.use("Agg")
from matplotlib import pyplot as plt
from matplotlib.patches import Rectangle

from explorer import HERE, UPSTREAM_LZBENCH, local_archive


def display(row):
    name = row["label"]
    if name.startswith("kolmo 1.1.0 "):
        return "Kolmo " + name[len("kolmo 1.1.0 "):]
    return name.replace("openzl", "OpenZL").replace("'generic'", "generic").replace("zstd", "Zstd").replace("lz4", "LZ4")


def render(local, output):
    sequence = ["rt", "fast", "eco", "hc", "max", "ultra"]
    def order(row):
        label = row["label"]
        if label.startswith("kolmo 1.1.0 "):
            return (0, sequence.index(label.split()[2]))
        return (1, label)

    rows = sorted(local["rows"], key=order)
    ink, dim, teal = "#18323e", "#536d76", "#007569"
    other, orange, blue = "#758994", "#bb602d", "#87b9d1"
    width = 12.4
    gap = .385
    height = 3.12 + gap * len(rows)
    fig = plt.figure(figsize=(width, height), facecolor="white")
    ax = fig.add_axes([0, 0, 1, 1], xlim=(0, width), ylim=(height, 0))
    ax.axis("off")
    def text(x, y, s, *, size=9, bold=False, color=ink, align="left"):
        ax.text(x, y, s, fontsize=size, fontweight="bold" if bold else "normal",
                color=color, ha=align, va="center", family="DejaVu Sans")
    def rect(x, y, w, color, h=.125):
        ax.add_patch(Rectangle((x, y-h/2), w, h, facecolor=color, linewidth=0))

    left, ratio_x, bytes_x, speed_x, enc_x, dec_x = .26, 2.08, 5.62, 6.01, 10.94, 12.12
    text(left, .37, "Kolmo v1.1.0 on the Silesia archive", size=21, bold=True)
    text(left, .79, "Apple M5   ·   One CPU worker   ·   In-memory lzbench   ·   Complete 211,947,520-byte tar", size=10, color=dim)
    h = 1.28
    text(left,h,"Setting",bold=True)
    text(ratio_x,h,"Compression ratio",bold=True)
    text(bytes_x,h,"Frame bytes",bold=True,align="right")
    rect(speed_x,h,.12,orange);text(speed_x+.19,h,"Encode")
    rect(speed_x+1.01,h,.12,blue);text(speed_x+1.2,h,"Decode  ·  MB/s")
    text(enc_x,h,"Encode",bold=True,align="right");text(dec_x,h,"Decode",bold=True,align="right")
    ax.plot([left,dec_x],[h+.18,h+.18],lw=.7,color="#b3c4c8")
    max_ratio = max(r["ratio"] for r in rows)
    max_stack = max(r["enc"] + r["dec"] for r in rows)
    y = h + .43
    for idx, r in enumerate(rows):
        is_kolmo = r["label"].startswith("kolmo ")
        if idx and not is_kolmo and rows[idx-1]["label"].startswith("kolmo "):
            ax.plot([left,dec_x],[y-gap/2,y-gap/2],lw=.6,color="#d2dddf")
        name = display(r)
        text(left,y,name,bold=is_kolmo,color=teal if is_kolmo else ink,size=9)
        rw = 1.72*r["ratio"]/max_ratio
        rect(ratio_x,y,rw,teal if is_kolmo else other);text(ratio_x+rw+.07,y,f"{r['ratio']:.2f}×",size=8)
        text(bytes_x,y,f"{r['size']:,}",align="right")
        ew = 3.61*r["enc"]/max_stack
        dw = 3.61*r["dec"]/max_stack
        rect(speed_x,y,ew,orange);rect(speed_x+ew,y,dw,blue)
        for x, k in ((enc_x,"enc"),(dec_x,"dec")):
            rate = r[k]
            text(x,y,format(rate,",.2f" if rate < 1 else ",.1f"),align="right")
        y += gap
    ax.plot([left,dec_x],[y-.12,y-.12],lw=.7,color="#b3c4c8")
    text(left,y+.12,"Timing",bold=True)
    text(left+1.48,y+.12,"Medians of three encode and five decode iterations. A row marked “1 pass” timed each phase once.",color=dim)
    text(left,y+.43,"Upstream size",bold=True)
    text(left+1.48,y+.43,local["upstream_size_comparison"],color=dim)
    text(left,y+.75,"Raw bytes divided by frame bytes gives the compression ratio. Both speed segments use the same MB/s scale.",
         size=8,color=dim)
    fig.savefig(output,dpi=155,facecolor="white",metadata={"Source":UPSTREAM_LZBENCH})
    plt.close(fig)


if __name__ == "__main__":
    local=local_archive(HERE.parent/"v110-lz/replay.json.gz")["groups"][0]
    render(local,HERE/"board.png")
