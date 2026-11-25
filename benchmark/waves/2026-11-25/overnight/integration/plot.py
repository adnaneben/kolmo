"""Render the combined product comparison using vector primitives and measured CSV values."""
import csv,math
from pathlib import Path
from html import escape
d=Path(__file__).resolve().parent
rows=list(csv.DictReader((d/'score.csv').open()))
out=['<svg xmlns="http://www.w3.org/2000/svg" width="1120" height="800" viewBox="0 0 1120 800" role="img"><title>Ratio and combined speed across three builds</title><style>text{font-family:system-ui,-apple-system,sans-serif;fill:#172f37}.tick{font-size:12px;fill:#455c61}.grid{stroke:#dbe4e3}.head{font-weight:650;font-size:16px}</style><rect width="100%" height="100%" fill="#fff"/>',
'<text x="35" y="42" style="font-size:28px;font-weight:700">Compression ratio and complete round-trip throughput</text><text x="35" y="69" font-size="16">Apple M5 · relative position within each workload · higher and farther right are better.</text>',
'<text x="35" y="97" font-size="15">○ pre-wave main (4db0425b)　 ● qualified main (a1d39559)　 ◆ combined experiment (38d512f5)</text>']
colors={'pre':'#54717c','main':'#047f7a','combined':'#d55a31'}
names={'enwik8.zip':'enwik8 ZIP archive · L1','owid-co2.csv':'Whole OWID CSV · L1 and L2','prose-64k':'Literary prefix · L3, L9 and Ultra'}
for i,(source,name) in enumerate(names.items()):
 selected=[r for r in rows if r['source']==source and r['arm']!='main-AA']
 vals=[math.log10(float(r['combined_MBps'])) for r in selected]
 ratios=[float(r['ratio']) for r in selected]
 xmin,xmax=min(vals),max(vals)
 xpad=max((xmax-xmin)*.15,.02); xmin-=xpad;xmax+=xpad
 ymin,ymax=min(ratios),max(ratios);ypad=max(.01,(ymax-ymin)*.16);ymin-=ypad;ymax+=ypad
 for j,t in enumerate(['1','4']):
  ax,ay=70+552*j,151+i*199;w,h=459,115
  out.append(f'<text class="head" x="{ax}" y="{ay-15}">{name} · T{t}</text>')
  for tick in range(4):
   a=tick/3;xp=ax+w*a;yp=ay+h-h*a
   out.extend([f'<path class="grid" d="M{xp},{ay}v{h}M{ax},{yp}h{w}"/>',f'<text class="tick" x="{xp}" y="{ay+h+18}" text-anchor="middle">{10**(xmin+(xmax-xmin)*a):.2g}</text>',
               f'<text class="tick" x="{ax-7}" y="{yp+4}" text-anchor="end">{ymin+(ymax-ymin)*a:.2f}×</text>'])
  for r in selected:
   if r['threads']!=t:continue
   x=ax+w*(math.log10(float(r['combined_MBps']))-xmin)/(xmax-xmin);y=ay+h-h*(float(r['ratio'])-ymin)/(ymax-ymin)
   c=colors[r['arm']]
   tip=escape(f"{r['arm']}, level {r['level']}, {float(r['combined_MBps']):.2f} MB/s, ratio {float(r['ratio']):.3f}")
   if r['arm']=='combined':out.append(f'<path fill="{c}" d="M{x},{y-5}l5,5 -5,5 -5,-5z"><title>{tip}</title></path>')
   else:out.append(f'<circle cx="{x}" cy="{y}" r="{5 if r["arm"]=="pre" else 3.5}" fill="{c if r["arm"]=="main" else "white"}" stroke="{c}" stroke-width="1.8"><title>{tip}</title></circle>')
out.append('<text x="35" y="748" font-size="15">X: decimal MB/s, log scale. Y: original ÷ stored bytes. All four builds/controls restored the source; each panel uses workload-specific axes.</text><text x="35" y="772" font-size="14">One warmup + eleven randomized scored repetitions. Archive and whole-file results have separate timing boundaries. Hover for values.</text></svg>')
(d/'frontier.svg').write_text('\n'.join(out)+'\n')
