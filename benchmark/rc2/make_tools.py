"""Write tools.json for run_cli2.py: every setting of tier C, each on one thread.

usage: make_tools.py KOLMO_BUILD_DIR OUT_JSON
KOLMO_BUILD_DIR holds kolmo, kolmo-fastq, kvid, krc-cpu, kbc and kbx of this commit. The rivals are the
command-line tools on PATH (zstd, xz, bsc, bzip3, brotli).
"""
import json
import shutil
import sys

b, out = sys.argv[1], sys.argv[2]
k = lambda t: f"{b}/{t}"
w = lambda t: shutil.which(t) or t


def kolmo(label, *args):
    return {"label": label, "c": [k("kolmo"), *args, "-T", "1", "{in}", "{out}"], "d": [k("kolmo"), "-d", "-T", "1", "{out}", "{back}"]}


GENERAL = [
    {"label": "zstd -3", "c": [w("zstd"), "-q", "-f", "-3", "-T1", "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "zstd -19", "c": [w("zstd"), "-q", "-f", "-19", "-T1", "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "xz -9", "c": [w("xz"), "-9", "-T1", "-c", "{in}", ">", "{out}"], "d": [w("xz"), "-d", "-T1", "-c", "{out}", ">", "{back}"]},
    {"label": "brotli -11", "c": [w("brotli"), "-q", "11", "-f", "{in}", "-o", "{out}"], "d": [w("brotli"), "-d", "-f", "{out}", "-o", "{back}"]},
    {"label": "bsc -m0 -e2", "c": [w("bsc"), "e", "{in}", "{out}", "-m0", "-e2"], "d": [w("bsc"), "d", "{out}", "{back}"]},
    {"label": "bzip3 -b 16", "c": [w("bzip3"), "-e", "-j", "1", "-b", "16", "-c", "{in}", ">", "{out}"], "d": [w("bzip3"), "-d", "-j", "1", "-c", "{out}", ">", "{back}"]},
]
KOLMO = [kolmo("kolmo -m balanced", "-m", "balanced"), kolmo("kolmo -m hc", "-m", "hc"),
         kolmo("kolmo -m ratio", "-m", "ratio"), kolmo("kolmo -x", "-x")]
RESEARCH = [
    {"label": "krc (KRC1, cpu)", "c": [k("krc-cpu"), "-b", "cpu", "-T", "1", "{in}", "{out}"], "d": [k("krc-cpu"), "-d", "-T", "1", "{out}", "{back}"]},
    {"label": "kbc (KBC1)", "c": [k("kbc"), "-T", "1", "{in}", "{out}"], "d": [k("kbc"), "-d", "-T", "1", "{out}", "{back}"]},
    {"label": "kbx -l 0", "c": [k("kbx"), "-l", "0", "-T", "1", "{in}", "{out}"], "d": [k("kbx"), "-d", "-T", "1", "{out}", "{back}"]},
    {"label": "kbx -l 1", "c": [k("kbx"), "-l", "1", "-T", "1", "{in}", "{out}"], "d": [k("kbx"), "-d", "-T", "1", "{out}", "{back}"]},
    {"label": "kbx -l 2", "c": [k("kbx"), "-l", "2", "-T", "1", "{in}", "{out}"], "d": [k("kbx"), "-d", "-T", "1", "{out}", "{back}"]},
]
FASTQ = [{"label": "kolmo-fastq (prototype)", "c": [k("kolmo-fastq"), "{in}", "{out}"], "d": [k("kolmo-fastq"), "-d", "{out}", "{back}"]}]
VIDEO = [{"label": f"kvid -m {m}", "c": [k("kvid"), "-m", m, "{in}", "{out}"], "d": [k("kvid"), "-d", "{out}", "{back}"]}
         for m in ("balanced", "hc", "ratio", "x")]
# The research codecs at several thread counts and on the GPU: not one-thread rows, reported apart.
SCALING = [{"label": f"krc {b} -T {t}", "c": [k("krc-auto"), "-b", b, "-T", str(t), "{in}", "{out}"], "d": [k("krc-auto"), "-d", "-T", str(t), "{out}", "{back}"]}
           for b, t in (("cpu", 1), ("cpu", 4), ("cpu", 10), ("metal", 10), ("hybrid", 10))]
SCALING += [{"label": f"kbx -l 1 -T {t}", "c": [k("kbx"), "-l", "1", "-T", str(t), "{in}", "{out}"], "d": [k("kbx"), "-d", "-T", str(t), "{out}", "{back}"]} for t in (1, 4, 10)]
SCALING += [{"label": f"kolmo -m balanced -T {t}", "c": [k("kolmo"), "-m", "balanced", "-T", str(t), "{in}", "{out}"], "d": [k("kolmo"), "-d", "-T", str(t), "{out}", "{back}"]} for t in (1, 4, 10)]

json.dump({"fastq": FASTQ + KOLMO + GENERAL + RESEARCH, "video": VIDEO + KOLMO + GENERAL + RESEARCH,
           "research": RESEARCH + KOLMO[:3] + GENERAL, "scaling": SCALING}, open(out, "w"), indent=1)
print({g: len(v) for g, v in json.load(open(out)).items()})
