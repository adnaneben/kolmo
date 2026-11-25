#!/usr/bin/env python3
"""Build the impact-matrix corpus: real files cut to at most CAP bytes (text at a line end, y4m at a frame end),
by data kind, dev and holdout kept apart. Writes OUT/{dev,holdout}/<kind>__<name> and OUT/manifest.jsonl.
usage: mkcorpus.py V OUT     (V = the review workspace)"""
import hashlib, io, json, os, sys, tarfile, glob, gzip
V, OUT = sys.argv[1], sys.argv[2]
CAP = 4 << 20
def p(*a): return os.path.join(V, *a)
CLT = "/Library/Developer/CommandLineTools/Library/Frameworks/Python3.framework/Versions/3.9/lib/python3.9"
# (set, kind, name, source spec)
S = [
 ("dev", "text", "dickens", p("corpus/dickens")),
 ("holdout", "text", "reymont", p("corpus-holdout/reymont")),
 ("holdout", "text", "ld30", p("corpus-holdout/ld30.txt")),
 ("dev", "code", "flac-c", ("cat", p("rivals/src/flac-1.5.0/src"), (".c", ".h"))),
 ("holdout", "code", "jxl-cc", ("cat", p("rivals/src/libjxl-0.13.0/lib/jxl"), (".cc", ".h"))),
 ("holdout", "code", "py-stdlib", ("cat", CLT, (".py",))),
 ("dev", "json", "api", p("corpus/api.json")),
 ("holdout", "json", "gharchive", p("review40b/work/R21/data/gh_test.ndjson")),
 ("dev", "xml", "silesia-xml", p("corpus/xml")),
 ("holdout", "html", "bashref", "/usr/share/doc/bash/bashref.html"),
 ("holdout", "xml", "enwik8", p("mkt/data/enwik8")),
 ("dev", "csv", "energy-130col", p("corpus/energy_test.csv")),
 ("dev", "csv", "ghcn", p("review40b/work/R21/data/ghcn_train.csv")),
 ("holdout", "csv", "owid-co2", p("corpus-holdout/owid-co2-data.csv")),
 ("holdout", "csv", "backblaze", p("outreach-data/backblaze/2025-12-31.csv")),
 ("holdout", "csv", "gaia", p("outreach-data/gaia/GaiaSource_000000-003111.csv")),
 ("dev", "tsv", "ghcn-rows", p("review40b/work/R34/sqs/ghcn.rows.tsv")),
 ("holdout", "tsv", "bb-rows", p("review40b/work/R34/sqs/bb.rows.tsv")),
 ("dev", "log", "loghub", p("corpus/loghub_all.log")),
 ("holdout", "log", "bgl", p("outreach-data/loghub/BGL.log")),
 ("holdout", "log", "nasa", p("review40b/work/R21/data/nasa_test.log")),
 ("dev", "numeric", "population-i64", p("corpus/population.i64")),
 ("dev", "numeric", "owid-f64", p("corpus/owid_energy_f64.npy")),
 ("holdout", "numeric", "sao", p("corpus-holdout/sao")),
 ("holdout", "numeric", "ncep-nc", ("glob", p("bench-page/data/dl/ncep-air-2022*.nc"))),
 ("dev", "weights", "bf16-10mb", p("corpus/llm_bf16_10mb.safetensors")),
 ("dev", "weights", "magika-onnx", p("corpus/magika.onnx")),
 ("holdout", "weights", "gpt2-f32", p("bench-page/data/weights/gpt2-f32.safetensors")),
 ("holdout", "weights", "smollm2-bf16", p("bench-page/data/weights/smollm2-135m-bf16.safetensors")),
 ("dev", "img8", "retina", p("corpus/retina.ppm")),
 ("dev", "img8", "hubble", p("corpus/hubble_deep_field.ppm")),
 ("holdout", "img8", "coffee", p("corpus-holdout/coffee.ppm")),
 ("holdout", "img8", "astronaut", p("corpus-holdout/astronaut.ppm")),
 ("dev", "img16", "x-ray", p("corpus/x-ray")),
 ("holdout", "img16", "mr", p("corpus-holdout/mr")),
 ("dev", "audio", "esc50-1-100032", p("corpus/esc50-1-100032-A-0.wav")),
 ("dev", "audio", "esc50-1-28808", p("corpus/esc50-1-28808-A-43.wav")),
 ("holdout", "audio", "esc50-3-100018", p("corpus-holdout/esc50-3-100018-A-18.wav")),
 ("holdout", "audio", "esc50-5-103415", p("corpus-holdout/esc50-5-103415-A-2.wav")),
 ("dev", "video", "akiyo", ("y4m", p("corpus/akiyo_cif.y4m"))),
 ("holdout", "video", "foreman", ("y4m", p("corpus-holdout/foreman_cif.y4m"))),
 ("dev", "exe", "ooffice-x86", p("corpus/ooffice")),
 ("holdout", "exe", "wasmtime-arm64", p("review40b/work/R28/venv/lib/python3.9/site-packages/wasmtime/darwin-aarch64/_libwasmtime.dylib")),
 ("holdout", "exe", "osdb", p("corpus-holdout/osdb")),
 ("dev", "container", "kennedy-xls", ("tar", p("mkt/data/canterbury.tar"), "canterbury/kennedy.xls")),
 ("holdout", "container", "bash-pdf", "/usr/share/doc/bash/bash.pdf"),
 ("holdout", "container", "highway-pdf", p("rivals/src/libjxl-0.13.0/third_party/highway/g3doc/highway_intro.pdf")),
 ("holdout", "container", "comm-jar", p("review40b/work/R18/moz/mozilla/chrome/comm.jar")),
 ("dev", "genomics", "srr835775", p("corpus/SRR835775.fastq")),
 ("holdout", "genomics", "srr2584863", p("corpus-holdout/SRR2584863.fastq")),
 ("dev", "tar", "canterbury", p("mkt/data/canterbury.tar")),
 ("holdout", "tar", "samba", p("corpus-holdout/samba")),
 ("holdout", "tar", "mozilla", p("corpus-holdout/mozilla")),
]
TEXT = {"text", "code", "json", "xml", "html", "csv", "tsv", "log", "genomics"}
def read(spec):
    if isinstance(spec, str):
        with open(spec, "rb") as f: return f.read(CAP + (1 << 20)), spec
    k = spec[0]
    if k == "glob": f = sorted(glob.glob(spec[1]))[0]; return read(f)
    if k == "tar":
        with tarfile.open(spec[1]) as t: return t.extractfile(spec[2]).read(), spec[1] + ":" + spec[2]
    if k == "cat":
        b = io.BytesIO()
        for root, ds, fs in sorted(os.walk(spec[1])):
            ds.sort()
            for fn in sorted(fs):
                if fn.endswith(spec[2]) and b.tell() < CAP + (1 << 20):
                    with open(os.path.join(root, fn), "rb") as f: b.write(f.read())
        return b.getvalue(), spec[1] + "/**" + "|".join(spec[2])
    if k == "y4m":
        with open(spec[1], "rb") as f: d = f.read(CAP + (2 << 20))
        h = d.index(b"\n") + 1; fr = d.index(b"FRAME", h); fl = d.index(b"FRAME", fr + 6) - fr
        n = (CAP - h) // fl; return d[: h + n * fl], spec[1]
def main():
    rows = []
    for st, kind, name, spec in S:
        d, src = read(spec)
        if len(d) > CAP:
            d = d[:CAP]
            if kind in TEXT and b"\n" in d: d = d[: d.rindex(b"\n") + 1]
        os.makedirs(os.path.join(OUT, st), exist_ok=True)
        fn = os.path.join(OUT, st, f"{kind}__{name}")
        with open(fn, "wb") as f: f.write(d)
        rows.append({"set": st, "kind": kind, "name": name, "file": os.path.relpath(fn, OUT), "bytes": len(d),
                     "sha256": hashlib.sha256(d).hexdigest(), "source": os.path.relpath(src, V) if src.startswith(V) else src})
    with open(os.path.join(OUT, "manifest.jsonl"), "w") as f:
        for r in rows: f.write(json.dumps(r) + "\n")
    print(len(rows), "files,", sum(r["bytes"] for r in rows) >> 20, "MiB")
main()
