"""Prepare exact targets from the preregistered public media and electricity files."""
import argparse
import gzip
import json
from pathlib import Path
import shutil
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"electricity"))
from run import prepare
from panel import sha, write_json

URLS = dict(bsub="https://ftp.ncbi.nlm.nih.gov/genomes/all/GCF/000/009/045/GCF_000009045.1_ASM904v1/GCF_000009045.1_ASM904v1_genomic.fna.gz",
            yeast="https://ftp.ncbi.nlm.nih.gov/genomes/all/GCF/000/146/045/GCF_000146045.2_R64/GCF_000146045.2_R64_genomic.fna.gz",
            chr22="https://hgdownload.soe.ucsc.edu/goldenPath/hg38/chromosomes/chr22.fa.gz",
            coffee="https://raw.githubusercontent.com/scikit-image/scikit-image/v0.24.0/skimage/data/coffee.png",
            akiyo="https://media.xiph.org/video/derf/y4m/akiyo_qcif.y4m",
            cellar="https://raw.githubusercontent.com/ietf-wg-cellar/flac-test-files/main/subset/01%20-%20blocksize%204096.flac",
            electricity="https://data.open-power-system-data.org/household_data/2020-04-15/household_data_15min_singleindex.csv")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("work",type=Path)
    p.add_argument("ffmpeg",type=Path)
    a=p.parse_args()
    d=a.work/"holdouts"
    rows={}
    def note(name,source,target,**metadata):
        rows[name]=dict(url=URLS[name],artifact_sha256=sha(source),target_sha256=sha(target),
                        source_bytes=source.stat().st_size,target_bytes=target.stat().st_size,**metadata)
    for name in ("bsub","yeast","chr22"):
        source=d/(name+".gz"); target=d/(name+".fa")
        with gzip.open(source,"rb") as f,target.open("wb") as out:
            shutil.copyfileobj(f,out)
        note(name,source,target)
    src=d/"akiyo.y4m"; target=d/"akiyo.yuv"
    with src.open("rb") as f,target.open("wb") as out:
        header=f.readline()
        assert header.startswith(b"YUV4MPEG2 W176 H144 F30000:1001 ")
        count=0
        while marker:=f.readline():
            assert marker==b"FRAME\n"
            frame=f.read(176*144*3//2)
            assert len(frame)==176*144*3//2
            out.write(frame); count+=1
        assert count==300
    note("akiyo",src,target,width=176,height=144,layout="yuv420p",frames=count,rate="30000/1001")
    src=d/"coffee.png"; target=d/"coffee.rgb"
    header=src.read_bytes()[:33]
    assert header[:8]==b"\x89PNG\r\n\x1a\n" and header[12:16]==b"IHDR"
    w,h=int.from_bytes(header[16:20],"big"),int.from_bytes(header[20:24],"big")
    subprocess.run([a.ffmpeg,"-nostdin","-loglevel","error","-y","-threads","1","-i",src,
                    "-threads","1","-pix_fmt","rgb24","-f","rawvideo",target],check=True)
    assert target.stat().st_size==w*h*3
    note("coffee",src,target,width=w,height=h,layout="rgb24")
    src=d/"cellar-01.flac"; target=d/"cellar.pcm"
    header=src.read_bytes()[:42]
    assert header[:4]==b"fLaC" and (header[4]&127)==0
    spec=int.from_bytes(header[18:26],"big")
    rate,channels,bits=spec>>44,1+((spec>>41)&7),1+((spec>>36)&31)
    samples=spec&((1<<36)-1)
    assert bits in (16,24)
    fmt=f"s{bits}le"
    subprocess.run([a.ffmpeg,"-nostdin","-loglevel","error","-y","-threads","1","-i",src,
                    "-threads","1","-c:a","pcm_"+fmt,"-f",fmt,target],check=True)
    assert target.stat().st_size==samples*channels*(bits//8)
    note("cellar",src,target,rate=rate,channels=channels,bits=bits,samples=samples,layout=fmt)
    electricity=d/"cossmic"; electricity.mkdir(exist_ok=True)
    row=prepare(a.work/"cossmic.csv",electricity)
    row["url"]=URLS["electricity"];rows["electricity"]=row
    write_json(a.work/"holdout-inputs.json",rows)


if __name__=="__main__":
    main()
