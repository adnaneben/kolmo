#!/usr/bin/env python3
"""Summarize item 5's results.tsv into markdown tables (stdout)."""
import collections, os, re, sys

D = os.path.dirname(os.path.abspath(__file__))
rows = [l.rstrip("\n").split("\t") for l in open(os.path.join(D, "results.tsv"))]

def kind(path):
    n = os.path.basename(path).lower()
    for pat, k in [(r"\.tar$", "tar"), (r"fastq", "fastq"), (r"\.y4m", "video"), (r"\.wav", "audio"), (r"\.(ppm|pgm)$|x-ray|^mr$|retina|hubble|astronaut|chelsea|coffee|immuno", "image"),
                   (r"safetensors|onnx|npy|f64_|i64|population|owid_energy_f64", "numeric/weights"), (r"\.csv|sao|nci|osdb", "table/db"),
                   (r"\.log|loghub", "log"), (r"\.json|xml", "markup"), (r"ooffice|mozilla|samba", "binary/archive"),
                   (r"dickens|webster|enwik|reymont|ld30|\.txt", "text")]:
        if re.search(pat, n): return k
    return "other"

R = {}   # (phase, input, mode, level, t, bin) -> (size, sha, ok)
bad = []
for f in rows:
    if len(f) < 9: continue
    ph, inp, mode, L, t, b, size, sha, ok = f[:9]
    R[(ph, inp, mode, int(L), int(t), b)] = (int(size), sha, ok)
    if ok not in ("ok", "-") or int(size) < 0: bad.append(f)

def pct(a, b): return (b - a) * 100.0 / a if a else 0.0

out = []
out.append(f"rows: {len(R)}; new outputs decoded by 0.0.2: {sum(1 for k,v in R.items() if k[5]=='new' and v[2]=='ok')}; failures: {len(bad)}")
for f in bad: out.append("FAIL: " + " ".join(f))
# T1 vs T10 frame identity for new
neq = [k for k in R if k[5] == "new" and k[4] == 10 and (k[0], k[1], k[2], k[3], 1, "new") in R and R[(k[0], k[1], k[2], k[3], 1, "new")][1] != R[k][1]]
out.append(f"new -T 1 vs -T 10 frames compared: {sum(1 for k in R if k[5]=='new' and k[4]==10 and (k[0],k[1],k[2],k[3],1,'new') in R)}, different: {len(neq)}")
for k in neq: out.append("  T1!=T10: " + " ".join(map(str, k)))

def table(phase, modes, title):
    keys = sorted({(k[1], k[2]) for k in R if k[0] == phase and k[2] in modes})
    if not keys: return
    out.append(f"\n### {title}\n")
    levels = range(1, 10)
    for t in (1, 10):
        hdr = "| input | mode | " + " | ".join(f"-{L}" for L in levels) + " |"
        cells = []
        for inp, mode in keys:
            c = []
            for L in levels:
                b = R.get((phase, inp, mode, L, t, "base")); n = R.get((phase, inp, mode, L, t, "new"))
                if t == 10 and not b: b = R.get((phase, inp, mode, L, 1, "base"))   # base -T 10 = base -T 1 (thread-independent frames)
                c.append("" if not (b and n) else ("=" if b[1] == n[1] else f"{pct(b[0], n[0]):+.2f}%"))
            if any(x and x != "=" for x in c): cells.append(f"| {os.path.basename(inp)} | {mode} | " + " | ".join(c) + " |")
        if cells:
            out.append(f"-T {t} (change of new against base; `=` identical frame):\n")
            out.append(hdr); out.append("|---|---|" + "---:|" * 9); out.extend(cells); out.append("")

def kinds_table():
    agg = collections.defaultdict(lambda: [0, 0, 0, 0])   # (kind, level) -> base, new, files, changed
    for k, v in R.items():
        if k[0] != "files" or k[5] != "new" or k[4] != (1 if k[3] >= 8 else 10): continue
        b = R.get((k[0], k[1], k[2], k[3], k[4], "base"))
        if not b: continue
        a = agg[(kind(k[1]), k[3])]; a[0] += b[0]; a[1] += v[0]; a[2] += 1; a[3] += b[1] != v[1]
    ks = sorted({k for k, _ in agg})
    out.append("\n### Corpus files by kind, one-shot (levels 8-9 `-T 1`, levels 1-7 `-T 10`; sum of sizes, change, files changed / files)\n")
    out.append("| kind | " + " | ".join(f"-{L}" for L in range(1, 10)) + " |"); out.append("|---|" + "---:|" * 9)
    for kd in ks:
        c = []
        for L in range(1, 10):
            a = agg.get((kd, L))
            c.append("" if not a else (f"= ({a[2]})" if not a[3] else f"{pct(a[0], a[1]):+.2f}% ({a[3]}/{a[2]})"))
        out.append(f"| {kd} | " + " | ".join(c) + " |")

kinds_table()
table("files", {"oneshot"}, "Corpus files, one-shot: per file (only files where some level changed)")
table("tars", {"stream64", "pipe", "oneshot"}, "Real tars: `--stream=64` and `cat tar | kolmo` (pipe: 256 MiB segments above 256 MiB)")
table("archive", {"archive"}, "kolmo archive (#105)")
# absolute sizes for the key rows
out.append("\n### Absolute sizes (bytes, -T 1 unless noted)\n")
out.append("| input | mode | level | base | new | change |"); out.append("|---|---|---:|---:|---:|---:|")
for k in sorted(R):
    if k[5] != "new": continue
    b = R.get((k[0], k[1], k[2], k[3], k[4], "base"))
    if not b or b[1] == R[k][1]: continue
    if k[0] == "files" and k[3] < 8: continue
    out.append(f"| {os.path.basename(k[1])} | {k[2]}{'' if k[4]==1 else ' T'+str(k[4])} | -{k[3]} | {b[0]:,} | {R[k][0]:,} | {pct(b[0], R[k][0]):+.2f}% |")
print("\n".join(out))
