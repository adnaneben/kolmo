"""Draw two paired eight-file codec-panel frontiers using their exact separate timers."""
import csv
import json
import math
import sys
from pathlib import Path
from reportlab.lib.colors import HexColor
from reportlab.graphics.shapes import Drawing, String, Line, Circle, Rect
from reportlab.graphics import renderPDF

root=Path(sys.argv[1]); out=Path(sys.argv[2])
figure=Drawing(1050,688); ink=HexColor("#172e3d"); grid=HexColor("#dbe5e7"); teal=HexColor("#007e81"); gray=HexColor("#87999e")
figure.add(Rect(0,0,1050,688,fillColor=HexColor("#ffffff"),strokeColor=None))
def txt(x,y,s,sz=11,b=False,anchor="start"):
    figure.add(String(x,y,str(s),fontName="Helvetica-Bold" if b else "Helvetica",fontSize=sz,fillColor=ink,textAnchor=anchor))
def ln(x1,y1,x2,y2,color=grid,w=.8): figure.add(Line(x1,y1,x2,y2,strokeColor=color,strokeWidth=w))
txt(28,650,"Codec presets from level 1 through Ultra",25,True)
txt(29,625,"Eight matched file-to-file inputs  /  29.74 MB  /  Apple M5  /  One requested thread  /  Separate paired experiments",12)
rows=[]
for j,(stage,before,after,caption) in enumerate([("wave1","main","wave1","Pinned main and Wave 1"),("wave2","wave1","wave2","Wave 1 and Wave 2")]):
    a=json.loads((root/(stage+"-score.json")).read_text())
    left=30+510*j; txt(left,580,caption,16,True); xl=left+42;xr=left+474; yl=402;yr=552
    def px(v):return xl+(math.log10(v)+.55)/3.4*(xr-xl)
    def py(v):return yl+(v-1.7)/3.25*(yr-yl)
    for v in (0.3,1,3,10,30,100,300):
        x=px(v); ln(x,yl,x,yr);txt(x,yl-14,v,9,anchor="middle")
    for v in (2,3,4):
        y=py(v); ln(xl,y,xr,y);txt(xl-8,y-3,v,10,anchor="end")
    txt(left,yr+6,"Ratio (×)",9);txt((xl+xr)/2,yl-31,"Combined throughput (MB/s, log scale)",10,anchor="middle")
    for arm in (before,after):
        for lev in a['levels']:
            r=a['groups'][arm][lev]; n=r['raw']; ratio=n/r['size']; sp=n/r['total_s']/1e6; x=px(sp);y=py(ratio)
            if arm==before: figure.add(Circle(x,y,5.6,fillColor=None,strokeColor=gray,strokeWidth=1.6))
            else: figure.add(Circle(x,y,3.7,fillColor=teal,strokeColor=None))
            rows.append([stage,arm,lev,n,r["size"],ratio,n/r["enc_s"]/1e6,n/r["dec_s"]/1e6,sp])
    for x,s in zip([left,left+100,left+220,left+355,left+473],["After preset","Ratio","Encode MB/s","Decode MB/s","Total MB/s"]):
        txt(x,330,s,10,True,"start" if x==left else "end")
    for i,lev in enumerate(a['levels']):
        vals=[row for row in rows if row[:3]==[stage,after,lev]][0]
        for x,s in zip([left,left+100,left+220,left+355,left+473],[lev.upper(),f"{vals[5]:.3f}",f"{vals[6]:.2f}",f"{vals[7]:.2f}",f"{vals[8]:.2f}"]):
            txt(x,309-18*i,s,11,False,"start" if x==left else "end")
txt(29,99,"Open circle = before   /   Filled dot = after. Identical compressed bytes across each paired configuration.",12,True)
txt(29,75,"Reported speeds aggregate raw bytes over phase time. Combined speed uses paired total time. The two Wave 1 references were measured separately.",10)
txt(29,54,"This is the historical codec-level panel. The current main backport changes archive operations and the Python result copy; those need separate measurements.",10)
txt(29,29,"Wave 1   f5822f30     /     Wave 2   dcbf4831     /     Complete primary rows and RSS in evidence PR #85.",10)
renderPDF.drawToFile(figure,str(out/"codec-waves.pdf"))
with (out/"codec-waves.csv").open("w",newline="") as f:
    w=csv.writer(f);w.writerow(["pair","arm","level","raw_bytes","compressed_bytes","ratio","encode_MBps","decode_MBps","combined_MBps"]);w.writerows(rows)
