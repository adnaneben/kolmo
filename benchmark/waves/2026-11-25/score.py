import collections
import json
import pathlib
import statistics
import sys

PANEL = ('text__reymont', 'csv__owid-co2', 'log__bgl', 'weights__smollm2-bf16',
         'code__jxl-cc', 'img16__mr', 'exe__wasmtime-arm64', 'audio__esc50-3-100018')
LEVELS = [str(i) for i in range(1,10)] + ['ultra']

def summarize(files):
    receipts=collections.defaultdict(list)
    for f in files:
        for line in pathlib.Path(f).read_text().splitlines():
            r=json.loads(line)
            receipts[r['arm'],r['input'],r['level'],r['threads']].append(r)
    cells=[]
    for (arm,name,level,threads),rs in sorted(receipts.items()):
        if len({(r['input_sha256'],r['frame_sha256'],r['stored_bytes']) for r in rs})!=1:
            raise ValueError(f'inconsistent frame: {arm}, {name}, {level}, {threads}')
        raw=rs[0]['raw_bytes']
        size=rs[0]['stored_bytes']
        if len({r['repeat'] for r in rs})!=len(rs):
            raise ValueError('repeated trial id')
        if len(rs)<3:
            continue
        enc=statistics.median(r['enc']['wall_s'] for r in rs)
        dec=statistics.median(r['dec']['wall_s'] for r in rs)
        encode_cpu=statistics.median(r['enc']['cpu_s'] for r in rs)
        decode_cpu=statistics.median(r['dec']['cpu_s'] for r in rs)
        total=statistics.median(r['enc']['wall_s']+r['dec']['wall_s'] for r in rs)
        cells.append(dict(arm=arm,name=name,domain=rs[0]['domain'],level=level,threads=threads,
                          raw=raw,size=size,frame_sha256=rs[0]['frame_sha256'],trials=len(rs),
                          enc_s=enc,dec_s=dec,total_s=total,enc_cpu_s=encode_cpu,dec_cpu_s=decode_cpu,
                          enc_rss=max(r['enc']['rss_bytes'] for r in rs),
                          dec_rss=max(r['dec']['rss_bytes'] for r in rs)))
    pos={(r['arm'],r['name'],r['level'],r['threads']):r for r in cells}
    grouped={}
    for arm in sorted({r['arm'] for r in cells}):
        selected={}
        for lv in LEVELS:
            rs=[pos.get((arm,n,lv,1)) for n in PANEL]
            if any(r is None for r in rs):
                continue
            selected[lv]={key:sum(r[key] for r in rs)
                          for key in ['raw','size','enc_s','dec_s','total_s','enc_cpu_s','dec_cpu_s']}
            selected[lv]['peak_rss']=max(max(r['enc_rss'],r['dec_rss']) for r in rs)
        grouped[arm]=selected
    return dict(panel=PANEL,levels=LEVELS,groups=grouped,details=cells,n_rows=sum(map(len,receipts.values())))

if __name__=='__main__':
    print(json.dumps(summarize(sys.argv[1:]),indent=2))
