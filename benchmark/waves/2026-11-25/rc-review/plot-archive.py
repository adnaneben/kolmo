"""Render the paired archive experiment as a publication figure; uses ReportLab."""
import argparse
import csv
from pathlib import Path
from reportlab.graphics.shapes import Drawing, Line, String, Rect, Circle, Polygon
from reportlab.graphics import renderPDF
from reportlab.lib.colors import HexColor, Color

C = HexColor
ink, muted, grid = C("#172e3d"), C("#486271"), C("#dbe5e7")
styles = {"main":("Main at baseline","#8a999f","box"),"direct-main":("Archive backport","#007e81","dot"),"tar+LZ4":("tar + LZ4","#b96a15","tri"),"tar+zstd":("tar + zstd","#784a89","cross")}
cases=[("Silesia 12 files","Silesia directory","12 original files / 211.94 MB"),("768 small members","Small-file directory","768 derived files / 6.29 MB"),("Silesia gzip member","Precompressed Silesia","One gzip member / 68.23 MB"),("enwik8 gzip member","Precompressed enwik8","One gzip member / 36.55 MB")]
def render(rows, threads, output):
    W,H=1070,790
    d=Drawing(W,H)
    d.add(Rect(0,0,W,H,fillColor=C("#ffffff"),strokeColor=None))
    def label(x,y,s,size=11,color=ink,bold=False,anchor="start"):
        d.add(String(x,y,str(s),fontName="Helvetica-Bold" if bold else "Helvetica",fontSize=size,fillColor=color,textAnchor=anchor))
    def line(x1,y1,x2,y2,c=grid,width=0.65):
        d.add(Line(x1,y1,x2,y2,strokeColor=c,strokeWidth=width))
    def marker(x,y,arm,r=4.7):
        _,hc,shape=styles[arm]; c=C(hc)
        if shape=="dot": d.add(Circle(x,y,r,fillColor=c,strokeColor=C("#ffffff"),strokeWidth=0.65))
        if shape=="box": d.add(Rect(x-r,y-r,2*r,2*r,fillColor=None,strokeColor=c,strokeWidth=1.6))
        if shape=="tri": d.add(Polygon([x,y+r,x+r,y-r,x-r,y-r],fillColor=c,strokeColor=None))
        if shape=="cross":
            line(x-r,y-r,x+r,y+r,c,2);line(x-r,y+r,x+r,y-r,c,2)
    label(30,749,"Archive compression ratio vs. full-cycle throughput",27,bold=True)
    label(31,724,f"Apple M5  /  {threads} requested thread{'s' if threads!=1 else ''}  /  Seven interleaved scored rounds per cell  /  Warm filesystem cache",13,muted)
    for i,(case,title,subtitle) in enumerate(cases):
        left=32+516*(i%2);top=680-307*(i//2)
        label(left,top,title,17,bold=True);label(left,top-17,subtitle,10,muted)
        data={r["arm"]:r for r in rows if r["workload"]==case and int(r["requested_threads"])==threads}
        assert set(data)==set(styles) and all(int(v["reps"])==7 for v in data.values())
        x0,x1=left+62,left+472;y0,y1=top-153,top-44
        xs=[float(v["combined_MBps"]) for v in data.values()];ys=[float(v["ratio"]) for v in data.values()]
        lo,hi=min(xs),max(xs); dx=(hi-lo)*.135;lo-=dx;hi+=dx
        low,high=min(ys),max(ys); dy=(high-low)*.19;low-=dy;high+=dy
        for j in range(4):
            f=j/3; xx=x0+f*(x1-x0); yy=y0+f*(y1-y0)
            line(xx,y0,xx,y1);line(x0,yy,x1,yy)
            label(xx,y0-13,f"{lo+f*(hi-lo):,.0f}",9,muted,anchor="middle")
            label(x0-6,yy-3,f"{low+f*(high-low):.{4 if high-low<.02 else 2}f}",9,muted,anchor="end")
        label(x0-6,y1+9,"Ratio (×)",9,muted,anchor="end")
        label((x0+x1)/2,y0-28,"Combined throughput (MB/s)",10,muted,anchor="middle")
        for arm,v in data.items():
            x=x0+(float(v["combined_MBps"])-lo)/(hi-lo)*(x1-x0)
            y=y0+(float(v["ratio"])-low)/(high-low)*(y1-y0)
            marker(x,y,arm,6.8 if arm=="main" else 4.7)
        tx=[left+180,left+272,left+363,left+471]
        for x,s in zip(tx,["Ratio","Create ms","Restore ms","MB/s"]):label(x,top-205,s,10,muted,anchor="end")
        line(left,top-211,left+474,top-211)
        for j,(arm,(name,col,shape)) in enumerate(styles.items()):
            v=data[arm]; y=top-226-17*j; marker(left+5,y+3,arm,3.7)
            label(left+17,y,name,10.6,bold=arm=="direct-main")
            for x,val in zip(tx,[f"{float(v['ratio']):.4f}",f"{float(v['encode_ms']):.2f}",f"{float(v['decode_ms']):.2f}",f"{float(v['combined_MBps']):,.1f}"]):
                label(x,y,val,10.6,bold=arm=="direct-main",anchor="end")
    line(31,75,W-31,75)
    label(31,54,"Higher and farther right is better. Each panel uses its own scales. Combined MB/s = input MB / median(create + full restore seconds).",11,ink)
    label(31,35,"Phase medians are independent. Each restored tree matched content, names and tested metadata. External tar and codec processes can overlap.",10,muted)
    label(31,17,"Baseline 4db0425b   /   Tested archive backport 1104d1c4   /   LZ4 1.11.1   /   zstd 1.5.7   /   25 November 2026",10,muted)
    renderPDF.drawToFile(d,str(output))
if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument("summary",type=Path);p.add_argument("threads",type=int);p.add_argument("output",type=Path);a=p.parse_args()
    with a.summary.open() as f: render(list(csv.DictReader(f)),a.threads,a.output)
