#!/usr/bin/env python3
"""Kolmo 0.0.2 vs OpenZL 0.3.0 (zli, every applicable profile) vs zstd/xz on structured files.
Every tool runs as a CLI process on one thread, file to file, best of N (N=3, or 1 above 60 MB or for slow
settings), each round trip compared byte for byte. CPU time is recorded to confirm one thread.
usage: PANEL_DATA=DIR [KOLMO=kolmo ZLI=zli PANEL_WORK=DIR] openzl_panel.py OUT.json
(paths made configurable for docs/outreach/evidence; the measurement code is unchanged)"""
import filecmp, json, os, sys, time

K = os.environ.get("KOLMO", "kolmo")
Z = os.environ.get("ZLI", "zli")
D = os.environ["PANEL_DATA"].rstrip("/") + "/"
W = os.environ.get("PANEL_WORK", "/tmp/kolmo-panel-work").rstrip("/") + "/"
os.makedirs(W, exist_ok=True)
OUT = sys.argv[1]

panel = [  # (area, file, list of OpenZL profile arg lists)
    ("CSV", "owid-co2-data.csv", [["-p", "csv"]]),
    ("CSV", "energy_test.csv", [["-p", "csv"]]),
    ("CSV (wide, ';')", "wide_test.csv", [["-p", "csv", "--profile-arg", ";"]]),
    ("int64 array", "population.i64", [["-p", "le-i64"]]),
    ("int64 timestamps", "log_timestamps_ms.i64", [["-p", "le-i64"]]),
    ("int32 array", "year.i32", [["-p", "le-i32"]]),
    ("raw image u8", "img_astronaut.u8", [["-p", "u8"]]),
    ("raw image u8", "img_camera.u8", [["-p", "u8"]]),
    ("NumPy f64", "owid_energy_f64.npy", [["-p", "numpy"]]),
    ("NumPy u8 images", "lfw_subset.npy", [["-p", "numpy"]]),
    ("pandas pickle", "owid_energy.pkl", [["-p", "pickle"]]),
    ("ONNX model", "magika.onnx", [["-p", "onnx"]]),
    ("safetensors f32", "magika.safetensors", [["-p", "safetensors"]]),
    ("safetensors bf16 LLM", "llm_bf16_10mb.safetensors", [["-p", "safetensors"]]),
    ("GGUF model", "magika.gguf", [["-p", "gguf"]]),
    ("SAO star catalog", "sao", [["-p", "sao"], ["-p", "serial"]]),
    ("JSON", "api.json", [["-p", "serial"]]),
    ("logs", "loghub_all.log", [["-p", "serial"]]),
    ("genome FASTA", "chr22.fa", [["-p", "serial"]]),
    ("English text", "dickens", [["-p", "serial"]]),
]


def run(argv):
    t = time.perf_counter()
    pid = os.fork()
    if pid == 0:
        fd = os.open("/dev/null", os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2)
        os.execvp(argv[0], argv)
    _, st, ru = os.wait4(pid, 0)
    el = time.perf_counter() - t
    if st != 0:
        return None
    return el, ru.ru_utime + ru.ru_stime


def measure(name, c, d, src, reps):
    cf, bf = W + "p.c", W + "p.d"
    be = bd = 1e18; cpu = 0.0
    for _ in range(reps):
        for p in (cf, bf):
            if os.path.exists(p): os.remove(p)
        rc = run([a.format(i=src, o=cf) for a in c])
        if rc is None: return dict(tool=name, error="compress failed")
        rd = run([a.format(i=cf, o=bf) for a in d])
        if rd is None: return dict(tool=name, error="decompress failed")
        if not filecmp.cmp(src, bf, shallow=False): return dict(tool=name, error="MISMATCH")
        be = min(be, rc[0]); bd = min(bd, rd[0]); cpu = max(cpu, rc[1] / max(rc[0], 1e-9), rd[1] / max(rd[0], 1e-9))
    n = os.path.getsize(src); s = os.path.getsize(cf)
    return dict(tool=name, size=s, ratio=n / s, enc_mibs=n / be / 2**20, dec_mibs=n / bd / 2**20, cpu_per_wall=cpu)


res = json.load(open(OUT)) if os.path.exists(OUT) else {}
for area, f, profs in panel:
    src = D + f
    if f in res or not os.path.exists(src):
        continue
    n = os.path.getsize(src); reps = 3 if n < 60e6 else 1
    rows = []
    for lv in ["3", "5", "7", "9"] + (["U"] if n <= 12e6 else []):
        r = 1 if lv in ("U",) or (lv == "9" and n > 30e6) else reps
        rows.append(measure("kolmo -" + lv, [K, "-" + lv, "-T1", "-q", "-f", "{i}", "-o", "{o}"],
                            [K, "-d", "-T1", "-q", "-f", "{i}", "-o", "{o}"], src, r))
    for pa in profs:
        for lv in ["6", "9"]:
            rows.append(measure("openzl -l %s [%s]" % (lv, " ".join(pa)), [Z, "compress"] + pa + ["-l", lv, "-f", "{i}", "-o", "{o}"],
                                [Z, "decompress", "-f", "{i}", "-o", "{o}"], src, reps))
    for nm, c, d in [("zstd -3", ["zstd", "-3", "-T1", "-q", "-f", "{i}", "-o", "{o}"], ["zstd", "-d", "-q", "-f", "{i}", "-o", "{o}"]),
                     ("zstd -19", ["zstd", "-19", "-T1", "-q", "-f", "{i}", "-o", "{o}"], ["zstd", "-d", "-q", "-f", "{i}", "-o", "{o}"]),
                     ("xz -9", ["sh", "-c", "xz -9 -T1 -c '{i}' > '{o}'"], ["sh", "-c", "xz -d -T1 -c '{i}' > '{o}'"])]:
        rows.append(measure(nm, c, d, src, 1 if (nm != "zstd -3" and n > 30e6) else reps))
    res[f] = dict(area=area, bytes=n, rows=rows)
    json.dump(res, open(OUT + ".tmp", "w"), indent=1); os.replace(OUT + ".tmp", OUT)
    ok = [r for r in rows if "size" in r]
    bk = min((r for r in ok if r["tool"].startswith("kolmo")), key=lambda r: r["size"], default=None)
    bz = min((r for r in ok if r["tool"].startswith("openzl")), key=lambda r: r["size"], default=None)
    print(f"{f:28s} best kolmo {bk and bk['tool']} {bk and round(bk['ratio'],3)} | best openzl {bz and bz['tool']} "
          f"{bz and round(bz['ratio'],3)} | errors {[r['tool']+':'+r['error'] for r in rows if 'error' in r]}", flush=True)
