"""Write tools.json for run_cli2.py: rc3 against the leading general-purpose tools, every tool on the same thread count.

usage: make_tools.py BIN_DIR SCRIPT_DIR THREADS OUT_JSON
BIN_DIR holds kolmo, kvid, kfam and kbin of this commit; the rivals are the command-line tools on PATH (lz4, zstd, xz,
brotli, bsc, bzip3, 7zz, zpaq). THREADS is given to every tool that has threads (1 = the one-thread tier). The
Homebrew bsc build and brotli have no threads.
"""
import json
import shutil
import sys

b, scripts, T, out = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
k = lambda t: f"{b}/{t}"
w = lambda t: shutil.which(t) or t
one = T == "1"


def kolmo(name, *args):
    return {"label": f"kolmo {name} -T {T}", "c": [k("kolmo"), *args, "-T", T, "{in}", "{out}"], "d": [k("kolmo"), "-d", "-T", T, "{out}", "{back}"]}


KOLMO = [kolmo("speed", "-m", "speed"), kolmo("balanced", "-m", "balanced"), kolmo("eco", "-m", "eco"), kolmo("hc", "-m", "hc"),
         kolmo("ratio", "-m", "ratio"), kolmo("-x", "-x")]
zt = "-T1" if one else "-T0"
RIVALS = [
    {"label": "lz4 -1", "c": [w("lz4"), "-q", "-f", "-1", zt, "{in}", "{out}"], "d": [w("lz4"), "-q", "-f", "-d", "{out}", "{back}"]},
    {"label": "zstd -3", "c": [w("zstd"), "-q", "-f", "-3", zt, "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "zstd -19", "c": [w("zstd"), "-q", "-f", "-19", zt, "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "zstd -19 --long=27", "c": [w("zstd"), "-q", "-f", "-19", "--long=27", zt, "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "--long=27", "{out}", "-o", "{back}"]},
    {"label": "brotli -11", "c": [w("brotli"), "-q", "11", "-f", "{in}", "-o", "{out}"], "d": [w("brotli"), "-d", "-f", "{out}", "-o", "{back}"]},
    {"label": "xz -9", "c": [w("xz"), "-9", zt, "-c", "{in}", ">", "{out}"], "d": [w("xz"), "-d", zt, "-c", "{out}", ">", "{back}"]},
    {"label": "7-Zip LZMA2 -mx9", "c": ["sh", f"{scripts}/7z_c.sh", T, "{in}", "{out}"], "d": ["sh", f"{scripts}/7z_d.sh", T, "{out}", "{back}"]},
    {"label": "bsc -m0 -e2 (one thread: this build has no threads)", "c": [w("bsc"), "e", "{in}", "{out}", "-m0", "-e2"], "d": [w("bsc"), "d", "{out}", "{back}"]},
    {"label": "bzip3 -b 16", "c": [w("bzip3"), "-e", "-j", T, "-b", "16", "-c", "{in}", ">", "{out}"], "d": [w("bzip3"), "-d", "-j", T, "-c", "{out}", ">", "{back}"]},
    {"label": "zpaq -m5", "c": ["sh", f"{scripts}/zpaq_c.sh", T, "{in}", "{out}"], "d": ["sh", f"{scripts}/zpaq_d.sh", T, "{out}", "{back}"]},
]
KVID = [{"label": f"kvid -m {m} -T {T}", "c": [k("kvid"), "-m", m, "-T", T, "{in}", "{out}"], "d": [k("kvid"), "-d", "-T", T, "{out}", "{back}"]} for m in ("balanced", "ratio", "x")]
KFAM = [{"label": f"kfam -T {T}", "c": [k("kfam"), "-T", T, "{in}", "{out}"], "d": [k("kfam"), "-d", "-T", T, "{out}", "{back}"]}]
KBIN = [{"label": f"kbin -C -T {T}", "c": [k("kbin"), "-C", "-T", T, "{in}", "{out}"], "d": [k("kbin"), "-d", "-T", T, "{out}", "{back}"]}]
groups = {g: KOLMO + RIVALS for g in ("text", "general", "structured", "numeric", "weights", "audio", "img8", "img16")}
groups["numeric_npy"] = KOLMO + KBIN + RIVALS
groups["genomics"] = KOLMO + KFAM + RIVALS
groups["video"] = KOLMO + KVID + RIVALS
json.dump(groups, open(out, "w"), indent=1)
print({g: len(v) for g, v in groups.items()})
