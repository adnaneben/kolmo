"""Produce the final head ratio/combined-throughput figure from the archived CSV."""
import csv, math
from pathlib import Path
from reportlab.graphics import renderSVG, renderPDF
from reportlab.graphics.shapes import Circle, Drawing, Line, Rect, String
from reportlab.lib.colors import HexColor, white
d=Path(__file__).resolve().parent
rows=list(csv.DictReader((d/'score.csv').open()))
drawing=Drawing(720,400)
def color(s): return HexColor(s)
ink=color('#20313e'); muted=color('#52606c'); grid=color('#d8dee2')
def label(x,y,s,size=10,fill=ink):
    drawing.add(String(x,y,s,fontName='Helvetica',fontSize=size,fillColor=fill))
x0,y0,w,h=74,72,580,240
def x(v): return x0 + math.log10(v)/3*w
def y(v): return y0 + v/7*h
drawing.add(Rect(0,0,720,400,fillColor=white,strokeColor=None))
label(28,374,'Kolmo combined candidate versus pinned main',18)
label(28,353,'Apple M5, complete CLI workloads, warm cache. Throughput uses input bytes / (encode + decode time).',10,muted)
for t in (1,3,10,30,100,300,1000):
    drawing.add(Line(x(t),y0,x(t),y0+h,strokeColor=grid))
    label(x(t)-6,56,str(t),9,muted)
for t in range(8):
    drawing.add(Line(x0,y(t),x0+w,y(t),strokeColor=grid)); label(x0-17,y(t)-3,str(t),9,muted)
label(75,324,'Original bytes / stored bytes',10,muted)
label(214,34,'Combined throughput (MB/s, logarithmic scale)',11,muted)
palette={'enwik8.zip':('#286d85','enwik8.zip archive  L1 / T4',-142,-20),
         'owid-co2.csv':('#a54329','OWID CO2  L1 / T1',-96,8),
         'prose-64k':('#6a5494','64 KiB prose  L3 / T1',8,8)}
for name,(hexcode,title,dx,dy) in palette.items():
    points={r['arm']:(x(float(r['combined_MBps'])),y(float(r['ratio']))) for r in rows if r['source']==name}
    a=points['main']; b=points['combined']; c=color(hexcode)
    drawing.add(Line(*a,*b,strokeColor=c,strokeWidth=1.3))
    drawing.add(Circle(*a,5,strokeColor=c,strokeWidth=1.7,fillColor=white))
    drawing.add(Circle(*b,3,strokeColor=c,fillColor=c))
    label(b[0]+dx,b[1]+dy,title,10,c)
label(28,10,'Open circle: main a1d3955. Filled circle: opt-in candidate 5af8c465. Eleven scored triplets per workload.',9,muted)
renderSVG.drawToFile(drawing,str(d/'final-frontier.svg'))
renderPDF.drawToFile(drawing,str(d/'final-frontier.pdf'))
