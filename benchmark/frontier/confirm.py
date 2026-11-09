"""Freeze executables and run matched end-to-end trials on complete files."""
import argparse
import json
from pathlib import Path
import platform
import subprocess

from file_trials import run
from panel import sha, write_json

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]


def pair(encode, decode):
    return {"encode": list(map(str, encode)), "decode": list(map(str, decode))}


def freeze(work, reference):
    tools = reference/"build/comparison"
    upstream = reference/"build/holdout/frozen"
    return dict(
        candidate=work/"m1/kolmo", candidate_library=work/"m1/build/libkolmo.dylib",
        harness=work/"m1/lzbench", baseline=ROOT/"build/wide/candidate",
        release=upstream/"release", zstd=upstream/"zstd", zli=upstream/"zli",
        ennaf=tools/"naf/ennaf/ennaf", unnaf=tools/"naf/unnaf/unnaf",
        flac=work/"flac-build/src/flac/flac",
        ffmpeg=next((tools/"packages/imageio_ffmpeg/binaries").glob("ffmpeg-*")),
        cossmic_graph=work/"cossmic.zlc", portugal_graph=ROOT/"benchmark/wide/portugal.zlc",
        grid_graph=ROOT/"build/fast-memory/trained.zlc",
    )


def codecs(name, bins):
    src, frame, out = "{src}", "{frame}", "{out}"
    c = {}
    for k in ("candidate", "baseline", "release"):
        exe = bins[k]
        c[k] = pair([exe,"-T","1","-m","fast",src,frame], [exe,"-T","1","-d",frame,out])
    z = [bins["zstd"],"--single-thread","-q"]
    c["zstd"] = pair(z+["-3",src,"-o",frame], z+["-d",frame,"-o",out])
    if name in ("cossmic","portugal","grid"):
        zli=bins["zli"]
        c["openzl"] = pair([zli,"compress","--compressor",bins[name+"_graph"],src,"--output",frame],
                           [zli,"decompress",frame,"--output",out])
    if name in ("bsub","yeast","chr22","reads"):
        fmt = "--fastq" if name == "reads" else "--fasta"
        for level in (1,19):
            c["naf"+str(level)] = pair([bins["ennaf"],"-"+str(level),fmt,src,"-o",frame],
                                       [bins["unnaf"],fmt,frame,"-o",out])
    ff = [bins["ffmpeg"],"-nostdin","-loglevel","error","-threads","1","-filter_threads","1"]
    if name == "cellar":
        raw = ["--force-raw-format","--endian=little","--sign=signed"]
        flac = [bins["flac"],"-s"]
        for level in (0,5,8):
            c["flac"+str(level)] = pair(flac+["-"+str(level),*raw,"--channels=2","--bps=16",
                   "--sample-rate=44100","--no-padding","--no-seektable","-o",frame,src],
                   flac+["-d",*raw,"-o",out,frame])
        c["alac"] = pair(ff+["-f","s16le","-ar","44100","-ac","2","-i",src,"-threads","1",
                             "-c:a","alac","-f","ipod",frame],
                         ff+["-i",frame,"-threads","1","-c:a","pcm_s16le","-f","s16le",out])
    if name == "akiyo":
        raw = ["-f","rawvideo","-pixel_format","yuv420p","-video_size","176x144","-framerate","30000/1001"]
        for k,options in {
            "ffv1":["-c:v","ffv1","-level","3"],
            "x264-lossless":["-c:v","libx264","-preset","medium","-qp","0"],
            "av1-lossless":["-c:v","libaom-av1","-cpu-used","4","-crf","0","-b:v","0",
                            "-aom-params","lossless=1","-row-mt","0"]
        }.items():
            c[k] = pair(ff+raw+["-i",src]+options+["-threads","1","-f","matroska",frame],
                        ff+["-i",frame,"-threads","1","-pix_fmt","yuv420p","-f","rawvideo",out])
    return c


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("action",choices=["freeze","score"])
    p.add_argument("work",type=Path)
    p.add_argument("reference",type=Path)
    p.add_argument("cases",nargs="*")
    a=p.parse_args()
    work=a.work.resolve()
    bins=freeze(work,a.reference.resolve())
    hashes={k:sha(v) for k,v in bins.items()}
    receipt=HERE/"m1-freeze.json"
    if a.action=="freeze":
        if receipt.exists():
            raise FileExistsError(receipt)
        write_json(receipt,dict(binaries=hashes, candidate_source=subprocess.check_output(
            ["git","rev-parse","HEAD"],cwd=ROOT,text=True).strip(), system=platform.platform(),
            training=json.loads((work/"training.json").read_text()),protocol=sha(HERE/"protocol.md"),
            runners={p:sha(HERE/p) for p in ("confirm.py","file_trials.py")}))
        return
    frozen=json.loads(receipt.read_text())
    assert hashes == frozen["binaries"] and all(sha(HERE/k)==v for k,v in frozen["runners"].items())
    d=work/"holdouts"
    cases={name:d/(name+".fa") for name in ("bsub","yeast","chr22")}
    cases.update(cossmic=d/"cossmic/test.csv",portugal=ROOT/"build/wide/test.csv",
                 grid=ROOT/"build/fast-memory/test.csv",cellar=d/"cellar.pcm",akiyo=d/"akiyo.yuv",
                 reads=d/"reads.fastq",events=d/"events.jsonl",taxi=d/"taxi.parquet",
                 quic=d/"rfc9000.pdf",coffee=d/"coffee.rgb")
    pins=json.loads((HERE/"holdout-inputs.json").read_text())
    if (HERE/"additional-inputs.json").exists():
        pins.update(json.loads((HERE/"additional-inputs.json").read_text()))
    pins["cossmic"]={"target_sha256":pins["electricity"]["test_sha256"]}
    pins["portugal"]={"target_sha256":json.loads((HERE.parent/"wide/freeze.json").read_text())["inputs"]["test_sha256"]}
    pins["grid"]={"target_sha256":json.loads((HERE.parent/"electricity/freeze.json").read_text())["inputs"]["test_sha256"]}
    dest=work/"confirmation"
    dest.mkdir(exist_ok=True)
    for name in a.cases or cases:
        path=cases[name]
        assert sha(path)==pins[name]["target_sha256"],name
        run(path,codecs(name,bins),work,dest/(name+".json"))


if __name__=="__main__":
    main()
