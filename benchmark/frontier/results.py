"""Build one offline comparison chart and a machine-readable index from verified receipts."""
import argparse
import csv
import gzip
import json
from pathlib import Path
from statistics import median

from board_chart_labels import LABELS
from panel import MARKET,OPTIONS
HERE=Path(__file__).resolve().parent
CATEGORIES={
    "dickens":"English prose","mozilla":"Executable and application data","mr":"Medical scan","nci":"Chemical structures",
    "ooffice":"Office binary","osdb":"Database","reymont":"Polish prose","samba":"Source and documentation",
    "sao":"Astronomical binary","webster":"Dictionary","xml":"XML","x-ray":"Medical image","rgb":"RGB photograph",
    "png":"Encoded PNG","pcm":"Speech PCM","yuv":"Video frames","fasta":"E. coli FASTA","integers":"Scientific uint64",
    "grid":"European electricity","co2":"Emissions CSV","entropy":"SHAKE256 random control"}
TITLE={"cossmic":"German household electricity","grid":"European grid electricity","portugal":"Portuguese meters",
       "bsub":"B. subtilis FASTA","yeast":"Yeast FASTA","chr22":"Human chromosome 22","reads":"E. coli FASTQ",
       "events":"GitHub event JSONL","taxi":"NYC taxi Parquet","quic":"RFC 9000 PDF","cellar":"Stereo audio PCM",
       "coffee":"Coffee photograph RGB","akiyo":"Akiyo lossless video","eia":"US balancing authorities",
       "docx":"Office Open XML document","sqlite":"Chinook SQLite","y4m":"Akiyo Y4M container",
       "flac":"Original FLAC container","png":"Original coffee PNG","reserve":"Human HiSeq FASTQ"}

def load(path):return json.loads(path.read_text())

def cli_metrics(doc):
    n=doc["source_bytes"]
    return {name:dict(bytes=row["size"],encode=n/(row["encode_ms"]*1000),decode=n/(row["decode_ms"]*1000),
                      encode_rss=row["encode_rss"],decode_rss=row["decode_rss"],repeats=doc["rounds"])
            for name,row in doc.get("metrics",{}).items()}

def assemble(work):
    panel=load(HERE/"panel-manifest.json")
    groups={}
    def group(key,title,method,n,expected=None,note=""):
        groups[key]=dict(id=key,title=title,method=method,original=n,expected=expected,note=note,rows={},failed=[])
        return groups[key]
    def add(g,name,doc):
        if doc.get("valid"):
            g["rows"][name]=dict(bytes=doc["bytes"],encode=doc["encode"],decode=doc["decode"])
        else:g["failed"].append(name)
    tar=group("archive","Silesia · whole archive","lzbench · one worker",211947520,52,
              "All 12 Silesia members and tar headers. Three encode and five decode iterations per setting, medians.")
    for f in (work/"board").glob("*.json"):add(tar,f.stem,load(f))
    for k,v in panel["samples"].items():
        group("slice-"+k,CATEGORIES[k]+" · diagnostic sample","lzbench · one worker",v["size"],52,
              "First 4 MiB or the complete input when shorter. Compare only within this workload.")
    for f in (work/"observations").glob("*.json"):
        r=load(f);add(groups["slice-"+r["input"]],r["codec"],r)
    for f in (work/"m1-panel").glob("*.json"):
        r=load(f);g=tar if r["input"]=="whole-silesia" else groups["slice-"+r["input"]]
        add(g,"m1_fast",r)
    for f in (work/"full-inputs").glob("*.json"):
        r=load(f);k="full-"+r["input"]
        g=groups.get(k) or group(k,CATEGORIES[r["input"]]+" · complete file","lzbench · one worker",r["original_bytes"],
                                 note="Rivals selected by the three metric leaders on the diagnostic sample, plus Zstd -3.")
        add(g,r["codec"],r)
    for folder in ("confirmation","transfer-results"):
        for f in (work/folder).glob("*.json"):
            d=load(f);name=f.stem
            g=group("cli-"+name,TITLE.get(name,name),"File-to-file · one worker",d["source_bytes"],
                    note="Warm-up and seven randomized paired runs. SHA-256 verifies each reconstruction. Memory is maximum process RSS. OpenZL graphs were trained on disjoint rows.")
            g["rows"].update(cli_metrics(d))
            g["failed"]=[k for k in d["commands"] if "metrics" in d and k not in d["metrics"]]
            g["pending"]=[k for k in d["commands"] if "metrics" not in d]
    for f in (work/"mode-trials").glob("*.json"):
        d=load(f);k="cossmic" if f.stem=="electricity" else f.stem
        g=groups.get("cli-"+k)
        if not g:continue
        for name,r in cli_metrics(d).items():
            if name=="kolmo_fast" and "candidate" in g["rows"]:continue
            g["rows"][name]=r
        if "metrics" in d:
            g["note"]+=" The additional six-mode replay uses three timed pairs. Per-row repeat counts appear in the chart."
    f=work/"rgb-confirm.json"
    if f.exists():
        d=load(f);g=group("rgb-api","Coffee photograph · native image codecs","Python native APIs · one worker",d["source_bytes"],
                          note="Identical RGB samples. Image codecs receive pixel dimensions. Seven randomized trials include native wrapper and output conversion.")
        g["rows"].update(d.get("metrics",{}))
    for f in (work/"multicore").glob("*.json"):
        d=load(f);name=d["input"];regime=d["regime"];k=regime+"-"+name
        g=groups.get(k) or group(k,TITLE.get(name,{"archive":"Silesia tar","video":"Foreman YUV","integers":"Scientific uint64"}.get(name,name)),
            "lzbench · 10-worker pool" if regime=="pool10" else "lzbench · up to 10 internal workers",d["original_bytes"],
            note="1 MiB independent chunks, 10 outer workers, one worker per codec." if regime=="pool10" else
                 "One complete stream with 10 internal workers requested. Each codec determines supported internal parallelism.")
        add(g,d["codec"],d)
    for f in (work/"gpu-next").glob("*.csv"):
        with f.open() as stream:rows=list(csv.DictReader(stream))
        if len(rows)!=64:continue
        name=f.stem;n=int(rows[0]["input_bytes"])
        g=group("gpu-"+name,{"archive":"Silesia tar","grid":"European grid","video":"Foreman YUV","integers":"Scientific uint64",
                "cossmic":"German household","reads":"E. coli reads"}.get(name,name),"KRC1 · CPU and Metal",n,
                note="One warm-up and seven shuffled repetitions per method. Kernel, transfer, packing and checksums are timed. Device setup is separate. All plans, frames and restored bytes match the original CPU encoder.")
        for codec in {r["codec"] for r in rows}:
            trials=[r for r in rows if r["codec"]==codec and r["repeat"]!="0"]
            g["rows"][codec]=dict(bytes=int(trials[0]["frame_bytes"]),encode=n/median(float(t["encode_s"]) for t in trials)/1e6,
                decode=n/median(float(t["decode_s"]) for t in trials)/1e6,repeats=7)
    return [g for g in groups.values() if g["rows"] or g["failed"]]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("work",type=Path);p.add_argument("out",type=Path)
    a=p.parse_args();data=assemble(a.work)
    a.out.mkdir(exist_ok=True,parents=True)
    payload=dict(groups=data,labels=LABELS)
    text=json.dumps(payload,separators=(",",":"),ensure_ascii=False)
    (a.out/"comparison.html").write_text((HERE/"chart.template.html").read_text().replace("/*DATA*/",text.replace("<","\\u003c")))
    (a.out/"comparison.json.gz").write_bytes(gzip.compress((text+"\n").encode(),mtime=0))
if __name__=="__main__":main()
