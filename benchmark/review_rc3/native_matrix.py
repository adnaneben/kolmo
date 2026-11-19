#!/usr/bin/env python3
"""C5 audit: every mode, alias and -x level x every KOLMO_NATIVE setting, fresh process per encode, unique output
paths, decode with KOLMO_NATIVE unset, frame inspection (actual codecs), 1- vs 4-thread identity, and corruption /
truncation probes. Correctness only (no timing). Refuses an existing output directory.
usage: native_matrix.py KOLMO OUT_DIR [--quick]"""
import os, sys, json, random, struct, subprocess, hashlib, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frameinfo
K, OUT = os.path.abspath(sys.argv[1]), sys.argv[2]
os.mkdir(OUT)                                   # refuses an existing destination
QUICK = "--quick" in sys.argv
CLEAN = {k: v for k, v in os.environ.items() if not k.startswith(("KOLMO_", "OMP_"))}
rnd = random.Random(20261119)
words = ["kolmo", "block", "the", "of", "native", "coder", "match", "literal", "range", "huffman"]
def wav(n):
    pcm = b"".join(struct.pack("<hh", int(8000 * ((i % 200) / 100 - 1)) + rnd.randint(-40, 40), int(3000 * ((i % 77) / 38 - 1))) for i in range(n))
    return b"RIFF" + struct.pack("<I", 36 + len(pcm)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 2, 44100, 176400, 4, 16) + b"data" + struct.pack("<I", len(pcm)) + pcm
def y4m():
    s = b"YUV4MPEG2 W48 H32 F25:1 Ip A1:1 C420jpeg\n"
    for f in range(6):
        s += b"FRAME\n" + bytes(((x + 2 * f) * 3 + y * 5 + rnd.randint(0, 3)) & 255 for y in range(32) for x in range(48)) + bytes(128 + ((x + y + f) & 7) for _ in range(2) for y in range(16) for x in range(24))
    return s
FILES = {
    "text.txt": " ".join(rnd.choice(words) for _ in range(60000)).encode(),
    "table.csv": ("id,x,y\n" + "".join(f"{i},{rnd.randint(0, 999)},{rnd.random():.3f}\n" for i in range(20000))).encode(),
    "log.txt": "".join(f"2026-11-17T10:{i // 60 % 60:02d}:{i % 60:02d} host{i % 5} GET /a/{rnd.randint(1, 300)} 200\n" for i in range(12000)).encode(),
    "json.jsonl": "".join(json.dumps({"id": i, "v": rnd.random(), "k": rnd.choice(words)}) + "\n" for i in range(5000)).encode(),
    "ints.i32": b"".join(struct.pack("<i", 1000 + i * 3 + rnd.randint(-5, 5)) for i in range(60000)),
    "floats.f64": b"".join(struct.pack("<d", 20.0 + rnd.gauss(0, 1)) for _ in range(20000)),
    "image.u8": bytes((x * 3 + y * 5 + rnd.randint(0, 3)) & 255 for y in range(256) for x in range(256)),
    "audio.wav": wav(30000), "video.y4m": y4m(),
    "random.bin": bytes(rnd.getrandbits(8) for _ in range(70000)), "zeros.bin": bytes(100000), "tiny.txt": b"a", "empty.bin": b"",
}
MODES = {"speed": ["-m", "speed"], "balanced": ["-m", "balanced"], "eco": ["-m", "eco"], "hc": ["-m", "hc"], "ratio": ["-m", "ratio"],
         "x": ["-x"], "x-l1": ["-x", "-l", "1"], "x-l2": ["-x", "-l", "2"]}
ALIASES = {"rt": ("speed", ["-m", "rt"]), "fast": ("balanced", ["-m", "fast"]), "max": ("ratio", ["-m", "max"]), "ultra": ("x", ["-m", "ultra"])}
NATS = [None, "lz", "huf", "lzma", "strong", "all", "all,strong"]
if QUICK: NATS = [None, "all", "all,strong"]
rows = open(os.path.join(OUT, "rows.jsonl"), "x"); fails = []
def run(argv, env, timeout=600):
    t = time.time(); p = subprocess.run(argv, env=env, capture_output=True, timeout=timeout)
    return p.returncode, p.stderr.decode(errors="replace")[-300:], round(time.time() - t, 3)
def sha(p): return hashlib.sha256(open(p, "rb").read()).hexdigest()
def emit(r):
    rows.write(json.dumps(r) + "\n"); rows.flush()
    if r.get("status") != "ok": fails.append(r)
inp = os.path.join(OUT, "inputs"); os.mkdir(inp)
for name, data in FILES.items(): open(os.path.join(inp, name), "xb").write(data)
enc_dir = os.path.join(OUT, "frames"); os.mkdir(enc_dir)
frames = {}
for name, data in FILES.items():
    src = os.path.join(inp, name)
    for mk, margs in list(MODES.items()) + [(a, v[1]) for a, v in ALIASES.items()]:
        nats = NATS if mk in MODES else [None]
        for nat in nats:
            for T in ("1", "4"):
                tag = f"{name}.{mk}.n-{(nat or 'unset').replace(',', '+')}.T{T}"
                z = os.path.join(enc_dir, tag + ".k"); back = z + ".out"
                env = dict(CLEAN); env.update({"KOLMO_NATIVE": nat} if nat else {})
                r = {"input": name, "mode": mk, "native": nat or "unset", "T": T, "frame": os.path.basename(z)}
                rc, err, s = run([K, *margs, "-T", T, src, z], env); r["enc_rc"], r["enc_s"] = rc, s
                if rc: r.update(status="encode-failed", err=err); emit(r); continue
                rc, err, s = run([K, "-d", "-T", T, z, back], CLEAN); r["dec_rc"], r["dec_s"] = rc, s
                r["bytes"] = os.path.getsize(z); r["frame_sha"] = sha(z)
                try: r["info"] = frameinfo.summary(frameinfo.inspect(open(z, "rb").read()))
                except Exception as e: r["info"] = {"error": repr(e)}
                ok = rc == 0 and open(back, "rb").read() == data
                r["status"] = "ok" if ok else ("decode-failed" if rc else "MISMATCH"); frames[(name, mk, nat, T)] = r
                emit(r)
# derived checks
for (name, mk, nat, T), r in list(frames.items()):
    if T == "4" and (name, mk, nat, "1") in frames and frames[(name, mk, nat, "1")]["frame_sha"] != r["frame_sha"]:
        emit({"check": "threads-differ", "input": name, "mode": mk, "native": nat or "unset", "status": "THREADS_DIFFER"})
    if nat and T == "1" and (name, mk, None, "1") in frames:
        base = frames[(name, mk, None, "1")]
        emit({"check": "native-effect", "input": name, "mode": mk, "native": nat, "status": "ok",
              "identical_to_unset": base["frame_sha"] == r["frame_sha"], "same_size_as_unset": base["bytes"] == r["bytes"],
              "native_used": r["info"].get("native_used"), "version_unset": base["info"].get("versions"), "version": r["info"].get("versions"),
              "bytes_unset": base["bytes"], "bytes": r["bytes"]})
for a, (canon, _) in ALIASES.items():
    for name in FILES:
        x, y = frames.get((name, a, None, "1")), frames.get((name, canon, None, "1"))
        if x and y: emit({"check": "alias", "alias": a, "canonical": canon, "input": name, "status": "ok", "identical": x["frame_sha"] == y["frame_sha"], "bytes_alias": x["bytes"], "bytes_canon": y["bytes"]})
# corruption / truncation probes: decoder must fail cleanly or return the exact input; never exit 0 with other bytes
cdir = os.path.join(OUT, "corrupt"); os.mkdir(cdir)
for (name, mk, nat, T), r in frames.items():
    if T != "1" or nat not in (None, "all,strong" if not QUICK else "all,strong") or not FILES[name]: continue
    z = os.path.join(enc_dir, r["frame"]); b = bytearray(open(z, "rb").read()); n = len(b)
    probes = [("trunc", k) for k in sorted({0, 4, 31, 32, n // 2, n - 1}) if k < n]
    probes += [("flip", k) for k in sorted({4, 11, 20, 33, 40, n // 3, n // 2, n - 1}) if 0 <= k < n]
    for kind, k in probes:
        bad = bytes(b[:k]) if kind == "trunc" else bytes(b[:k]) + bytes([b[k] ^ 0x5A]) + bytes(b[k + 1:])
        cz = os.path.join(cdir, f"{r['frame']}.{kind}{k}"); open(cz, "xb").write(bad)
        rc, err, s = run([K, "-d", "-T", "1", cz, cz + ".out"], CLEAN, timeout=120)
        out = open(cz + ".out", "rb").read() if os.path.exists(cz + ".out") else None
        st = "ok" if (rc > 0 or (rc == 0 and out == FILES[name])) else ("CRASH" if rc < 0 else "SILENT_MISDECODE")
        emit({"check": "corrupt", "frame": r["frame"], "probe": f"{kind}{k}", "dec_rc": rc, "status": st, "err": err[-120:]})
rows.close()
summ = {"kolmo": K, "kolmo_sha256": sha(K), "rows": sum(1 for _ in open(os.path.join(OUT, "rows.jsonl"))), "failures": len(fails),
        "fail_kinds": sorted({f["status"] for f in fails})}
json.dump(summ, open(os.path.join(OUT, "summary.json"), "x"), indent=1); print(json.dumps(summ))
sys.exit(1 if fails else 0)
