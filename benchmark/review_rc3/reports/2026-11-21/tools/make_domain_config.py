#!/usr/bin/env python3
"""make_domain_config.py BASE_CONFIG OUT KOLMO_TAG=PATH [KOLMO_TAG=PATH ...]: domain suites vs specialists."""
import json, sys, hashlib, os
V = "/Users/adnanbenali/Kolmo-Reviews/review-20261119T083610Z.J62kQD"; D = V + "/corpus-domain"; P = V + "/rivals/prefix/bin"
base = json.load(open(sys.argv[1])); out = sys.argv[2]; kol = [a.split("=", 1) for a in sys.argv[3:]]
sha = lambda p: hashlib.sha256(open(p, "rb").read()).hexdigest()
cfg = {k: base[k] for k in ("seed", "repetitions", "warmups", "sample_ms", "preflight_sample_ms", "state_every_s", "quiet", "memory_scope")}
cfg["profile"] = "domain"; cfg["track"] = "cold-process CLI, file in -> file out; domain specialists; thread policy per codec"
tools = {}
for tag, path in kol: tools["kolmo-" + tag] = {"path": path, "sha256": sha(path), "version_argv": [], "license": "project"}
for name, path, va in [("flac", P + "/flac", ["--version"]), ("cjxl", P + "/cjxl", ["--version"]), ("djxl", P + "/djxl", ["--version"]),
                       ("ffmpeg6", V + "/rivals/prefix-ffmpeg6/bin/ffmpeg", ["-version"]), ("samtools", P + "/samtools", ["--version"]),
                       ("pcodec", P + "/pcodec", ["--version"])]:
    tools[name] = {"path": path, "sha256": sha(path), "version_argv": va, "source": "rivals/ (official source, run-local build; RIVALS_BUILT.md)"}
cfg["tools"] = tools
codecs = []
for tag, _ in kol:
    for m in ("speed", "balanced", "eco", "hc", "ratio"):
        codecs.append({"id": f"{tag}/{m}", "tool": "kolmo-" + tag, "encode": ["-m", m, "-T", "1", "{in}", "{out}"],
                       "decode": ["-d", "-T", "1", "{in}", "{out}"], "ext": ".kz", "timeout": 7200})
codecs += [
 {"id": "flac-8", "tool": "flac", "encode": ["-8", "--silent", "-f", "-o", "{out}", "{in}"], "decode": ["-d", "--silent", "-f", "-o", "{out}", "{in}"], "ext": ".flac", "thread_policy": "strict-T1"},
 {"id": "flac-5", "tool": "flac", "encode": ["-5", "--silent", "-f", "-o", "{out}", "{in}"], "decode": ["-d", "--silent", "-f", "-o", "{out}", "{in}"], "ext": ".flac", "thread_policy": "strict-T1"},
 {"id": "cjxl-e7", "tool": "cjxl", "encode": ["{in}", "{out}", "-d", "0", "-e", "7", "--num_threads=0", "--quiet"], "decode_tool": "djxl", "decode": ["{in}", "{out}", "--num_threads=0", "--quiet"], "ext": ".jxl", "thread_policy": "strict-T1"},
 {"id": "cjxl-e9", "tool": "cjxl", "encode": ["{in}", "{out}", "-d", "0", "-e", "9", "--num_threads=0", "--quiet"], "decode_tool": "djxl", "decode": ["{in}", "{out}", "--num_threads=0", "--quiet"], "ext": ".jxl", "thread_policy": "strict-T1"},
 {"id": "ffv1-l3", "tool": "ffmpeg6", "encode": ["-nostdin", "-loglevel", "error", "-threads", "1", "-i", "{in}", "-threads", "1", "-c:v", "ffv1", "-level", "3", "-g", "1", "-slices", "4", "-slicecrc", "1", "-y", "{out}"],
  "decode": ["-nostdin", "-loglevel", "error", "-threads", "1", "-i", "{in}", "-threads", "1", "-c:v", "rawvideo", "-f", "yuv4mpegpipe", "-y", "{out}"], "ext": ".mkv", "thread_policy": "worker+async-io (ffmpeg 6 CLI; 4 threads exist)"},
 {"id": "ffv1-l3-ctx1", "tool": "ffmpeg6", "encode": ["-nostdin", "-loglevel", "error", "-threads", "1", "-i", "{in}", "-threads", "1", "-c:v", "ffv1", "-level", "3", "-g", "1", "-slices", "4", "-slicecrc", "1", "-context", "1", "-coder", "1", "-y", "{out}"],
  "decode": ["-nostdin", "-loglevel", "error", "-threads", "1", "-i", "{in}", "-threads", "1", "-c:v", "rawvideo", "-f", "yuv4mpegpipe", "-y", "{out}"], "ext": ".mkv", "thread_policy": "worker+async-io (ffmpeg 6 CLI; 4 threads exist)"},
 {"id": "cram31", "tool": "samtools", "encode": ["import", "-@0", "-0", "{in}", "-o", "{out}", "-O", "cram,version=3.1"], "decode": ["fastq", "-@0", "{in}"], "decode_stdout": True, "ext": ".cram", "thread_policy": "strict-T1"},
 {"id": "cram31-archive", "tool": "samtools", "encode": ["import", "-@0", "-0", "{in}", "-o", "{out}", "-O", "cram,version=3.1,archive"], "decode": ["fastq", "-@0", "{in}"], "decode_stdout": True, "ext": ".cram", "thread_policy": "strict-T1"},
 {"id": "pcodec-8", "tool": "pcodec", "encode": ["compress", "--input-format", "binary", "-i", "{in}", "{out}"], "decode": ["decompress", "-o", "binary", "{in}"], "decode_stdout": True, "ext": ".pco", "thread_policy": "unverified (rayon in binary)"},
]
cfg["codecs"] = codecs
dom = {".wav": "audio", ".ppm": "image", ".y4m": "video", ".fastq": "genomics"}
inputs = []
for f in sorted(os.listdir(D)):
    p = os.path.join(D, f)
    if not os.path.isfile(p) or f == "MANIFEST.sha256": continue
    ext = os.path.splitext(f)[1]; d = dom.get(ext, "numeric")
    inputs.append({"id": f, "path": p, "sha256": sha(p), "bytes": os.path.getsize(p), "domain": d, "ext": ext, "provenance": "corpus-domain", "split": "domain"})
cfg["inputs"] = inputs
K = [f"{t}/{m}" for t, _ in kol for m in ("speed", "balanced", "eco", "hc", "ratio")]
ids = lambda d: [i["id"] for i in inputs if i["domain"] == d]
cfg["suites"] = {
 "dom-audio": {"inputs": ids("audio"), "codecs": K + ["flac-8", "flac-5"], "purpose": "audio vs FLAC"},
 "dom-image": {"inputs": ids("image"), "codecs": K + ["cjxl-e7", "cjxl-e9"], "purpose": "images vs JPEG XL lossless"},
 "dom-video": {"inputs": ids("video"), "codecs": K + ["ffv1-l3", "ffv1-l3-ctx1"], "purpose": "video vs FFV1 (ffmpeg-normalised y4m)"},
 "dom-genomics": {"inputs": ids("genomics"), "codecs": K + ["cram31", "cram31-archive"], "purpose": "FASTQ vs CRAM 3.1 (samtools-normalised FASTQ)"},
 "dom-numeric": {"inputs": ids("numeric"), "codecs": K + ["pcodec-8"], "purpose": "raw numeric arrays vs Pcodec"},
}
cfg["missing_tools"] = base.get("missing_tools", [])
json.dump(cfg, open(out, "x"), indent=1); print(out, len(inputs), "inputs", len(codecs), "codecs", {k: len(v["inputs"]) for k, v in cfg["suites"].items()})
