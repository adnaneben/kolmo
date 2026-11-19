#!/usr/bin/env python3
"""Generate the frozen harness config for one campaign (paths are machine-specific, so the generated JSON lives in the
review evidence directory, not in the repository). Refuses an existing output file.

usage: make_config.py --corpus MANIFEST.json --kolmo NAME=PATH [--kolmo NAME=PATH ...] --out CONFIG.json
                      [--profile screening|holdout]

Thread policy per codec (verified again by `audit.py preflight`, which samples the real thread counts):
  kolmo     -T 1 for encode and decode
  zstd      --single-thread (not -T1: -T1 may run a worker beside I/O) and --no-asyncio, decode --no-asyncio
  lz4       -T1 (1.11 defaults to -T0 = auto)
  xz        -T1, stdin -> stdout (the measured process is xz itself)
  7zz       -mmt=1 for encode and decode; the .7z container bytes count
  bzip3     -j 1
  bsc       no thread switch in this build's help: preflight must observe 1 thread or the codec is labelled
  brotli    single-threaded CLI
  zpaq      -t1; archive bytes count (it stores the path)
Quiet-machine rule (preregistered, independent of results): other_cores <= 0.5 during every encode and decode
process, a 0.3 s pre-window at <= 0.3 other cores, AC power, no thermal limit; rejected trials are kept and retried.
"""
import argparse, hashlib, json, os, shutil, subprocess

def sha(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""): h.update(c)
    return h.hexdigest()

def which(t):
    p = shutil.which(t)
    return os.path.realpath(p) if p else None

KOLMO_PRESETS = {"speed": ["-m", "speed"], "rt": ["-m", "rt"], "balanced": ["-m", "balanced"], "eco": ["-m", "eco"],
                 "hc": ["-m", "hc"], "ratio": ["-m", "ratio"], "x": ["-x"], "x-l1": ["-x", "-l", "1"], "x-l2": ["-x", "-l", "2"]}

def rivals():
    r = []
    for lv in (1, 3, 9, 19):
        r.append({"id": f"zstd-{lv}", "tool": "zstd", "encode": [f"-{lv}", "--single-thread", "--no-asyncio", "-q", "{in}", "-o", "{out}"],
                  "decode": ["-d", "--no-asyncio", "-q", "{in}", "-o", "{out}"], "ext": ".zst"})
    r.append({"id": "zstd-22", "tool": "zstd", "encode": ["--ultra", "-22", "--single-thread", "--no-asyncio", "-q", "{in}", "-o", "{out}"],
              "decode": ["-d", "--no-asyncio", "-q", "{in}", "-o", "{out}"], "ext": ".zst"})
    r.append({"id": "lz4-1", "tool": "lz4", "encode": ["-1", "-T1", "-q", "{in}", "{out}"], "decode": ["-d", "-T1", "-q", "{in}", "{out}"], "ext": ".lz4"})
    r.append({"id": "lz4-9", "tool": "lz4", "encode": ["-9", "-T1", "-q", "{in}", "{out}"], "decode": ["-d", "-T1", "-q", "{in}", "{out}"], "ext": ".lz4"})
    r.append({"id": "brotli-11", "tool": "brotli", "encode": ["-q", "11", "-w", "24", "-o", "{out}", "{in}"], "decode": ["-d", "-o", "{out}", "{in}"], "ext": ".br"})
    r.append({"id": "xz-9", "tool": "xz", "encode": ["-9", "-T1", "-c"], "encode_stdin": True, "encode_stdout": True,
              "decode": ["-d", "-T1", "-c"], "decode_stdin": True, "decode_stdout": True, "ext": ".xz"})
    r.append({"id": "7zz-9", "tool": "7zz", "encode": ["a", "-t7z", "-mx=9", "-mmt=1", "-bd", "-bso0", "-bsp0", "{out}", "{in}"],
              "decode": ["e", "-so", "-mmt=1", "-bd", "{in}"], "decode_stdout": True, "ext": ".7z"})
    r.append({"id": "bzip3", "tool": "bzip3", "encode": ["-e", "-j", "1", "-c", "{in}"], "encode_stdout": True,
              "decode": ["-d", "-j", "1", "-c", "{in}"], "decode_stdout": True, "ext": ".bz3"})
    r.append({"id": "bsc-default", "tool": "bsc", "encode": ["e", "{in}", "{out}"], "decode": ["d", "{in}", "{out}"], "ext": ".bsc",
              "thread_policy": "strict-T1"})
    r.append({"id": "zpaq-m5", "tool": "zpaq", "encode": ["a", "{out}", "{in}", "-m5", "-t1"], "decode": ["x", "{in}", "{orig}", "-to", "{out}", "-t1"],
              "ext": ".zpaq"})
    return r

MISSING = [
    {"name": "flac (CLI)", "domain": "audio", "source": "https://github.com/xiph/flac", "build": "cmake -B build -DCMAKE_INSTALL_PREFIX=$RUN/prefix && cmake --build build && cmake --install build", "cost": "minutes; small"},
    {"name": "ffmpeg (FFV1)", "domain": "video", "source": "https://ffmpeg.org (source tree present in kolmo-bench/tools/FFmpeg, unbuilt)", "build": "./configure --prefix=$RUN/prefix --disable-everything --enable-encoder=ffv1 --enable-decoder=ffv1 --enable-muxer=matroska --enable-demuxer=matroska,yuv4mpegpipe --enable-protocol=file && make -j4", "cost": "~10 min"},
    {"name": "cjxl/djxl (JPEG XL)", "domain": "image", "source": "https://github.com/libjxl/libjxl", "build": "cmake with -DJPEGXL_ENABLE_TOOLS=ON into $RUN/prefix; set --num_threads=0 / djxl --num_threads=0", "cost": "~15 min"},
    {"name": "pcodec CLI", "domain": "numeric", "source": "https://github.com/pcodec/pcodec (cargo install pcodec_cli --root $RUN/prefix)", "build": "cargo install --root $RUN/prefix pcodec_cli", "cost": "~3 min"},
    {"name": "OpenZL", "domain": "numeric/table", "source": "https://github.com/facebook/openzl", "build": "a source tree exists under the original checkout's _claude_scratch/fresh/openzl (not reused: not a pinned release build)", "cost": "~10 min"},
    {"name": "Kanzi", "domain": "general", "source": "https://github.com/flanglet/kanzi-cpp", "build": "make into $RUN/prefix; -j 1", "cost": "~2 min"},
    {"name": "fqzcomp5 / CRAM (samtools)", "domain": "genomics", "source": "https://github.com/jkbonfield/fqzcomp5 (tree in kolmo-bench/tools, unbuilt binary)", "build": "make in a copy; -t 1", "cost": "~2 min"},
    {"name": "cmix", "domain": "max-ratio", "source": "https://github.com/byronknoll/cmix", "build": "make; bounded lane only (very slow)", "cost": "hours per MB"},
]

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--corpus", required=True); ap.add_argument("--kolmo", action="append", required=True)
    ap.add_argument("--out", required=True); ap.add_argument("--profile", default="screening")
    a = ap.parse_args()
    if os.path.lexists(a.out): raise SystemExit("refusing existing " + a.out)
    man = json.load(open(a.corpus))
    tools, codecs = {}, []
    for kv in a.kolmo:
        name, path = kv.split("=", 1)
        tools[f"kolmo-{name}"] = {"path": path, "sha256": sha(path), "version_argv": [], "license": "project"}
        for pid, args in KOLMO_PRESETS.items():
            codecs.append({"id": f"{name}/{pid}", "tool": f"kolmo-{name}", "encode": [*args, "-T", "1", "{in}", "{out}"],
                           "decode": ["-d", "-T", "1", "{in}", "{out}"], "ext": ".kz", "timeout": 7200})
        for nat in ("all", "all,strong"):
            for pid in ("speed", "balanced", "hc"):
                codecs.append({"id": f"{name}/{pid}+native-{nat.replace(',', '+')}", "tool": f"kolmo-{name}",
                               "encode": [*KOLMO_PRESETS[pid], "-T", "1", "{in}", "{out}"], "decode": ["-d", "-T", "1", "{in}", "{out}"],
                               "env": {"KOLMO_NATIVE": nat}, "ext": ".kz"})
    for t, va in (("zstd", ["--version"]), ("lz4", ["--version"]), ("brotli", ["--version"]), ("xz", ["--version"]),
                  ("7zz", []), ("bzip3", ["--version"]), ("bsc", []), ("zpaq", [])):
        p = which(t)
        tools[t] = {"path": p, "sha256": sha(p) if p else None, "version_argv": va} if p else {"path": None}
    codecs += rivals()
    cfg = {"profile": a.profile, "seed": 20261119, "repetitions": 5, "warmups": 1, "sample_ms": 5, "preflight_sample_ms": 2,
           "state_every_s": 300,
           "quiet": {"max_other_cores": 0.5, "pre_window_s": 0.3, "pre_wait_tries": 120, "pre_backoff_s": 5, "max_retries": 4, "require_ac": True},
           "memory_scope": "direct codec process (no adapters); kernel ru_maxrss (Darwin bytes) and sampled resident_size",
           "track": "cold-process CLI, file in -> file out, strict single thread unless thread_policy says otherwise",
           "tools": tools, "codecs": codecs,
           "inputs": [{"id": m["id"], "path": m["path"], "sha256": m["sha256"], "bytes": m["bytes"], "domain": m["domain"],
                       "provenance": m.get("provenance"), "split": a.profile} for m in man],
           "missing_tools": MISSING, "suites": {}}
    json.dump(cfg, open(a.out, "x"), indent=1)
    print(f"{len(codecs)} codecs, {len(cfg['inputs'])} inputs -> {a.out} (add suites with add_suites)")

if __name__ == "__main__":
    main()
