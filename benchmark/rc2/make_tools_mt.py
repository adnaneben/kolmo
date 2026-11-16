"""Write tools-mt.json for the multithreaded tiers (M1: one file with all cores; M2: many files, see set_parallel.py).

usage: make_tools_mt.py KOLMO_BUILD_DIR SCRIPT_DIR THREADS OUT_JSON [EXTRA_TOOLS_DIR]
Every setting uses every core it can: Kolmo and its research codecs with -T THREADS, zstd and xz with -T0, lz4 -T0,
bzip3 -j THREADS, 7-Zip -mmt=THREADS, zpaq -tTHREADS. Tools without threads (brotli) run as they are and are
labelled "(no threads)". EXTRA_TOOLS_DIR may hold kaud, kbin and krans builds from their branches; settings for
the ones present are added.
"""
import json
import os
import shutil
import sys

b, scripts, T, out = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
extra = sys.argv[5] if len(sys.argv) > 5 else None
k = lambda t: f"{b}/{t}"
w = lambda t: shutil.which(t) or t


def kolmo(label, *args, threads=T):
    return {"label": f"{label} -T {threads}", "c": [k("kolmo"), *args, "-T", str(threads), "{in}", "{out}"], "d": [k("kolmo"), "-d", "-T", str(threads), "{out}", "{back}"]}


KOLMO = [kolmo("kolmo -m speed", "-m", "speed"), kolmo("kolmo -m balanced", "-m", "balanced"), kolmo("kolmo -m fast -q", "-m", "fast", "-q"),
         kolmo("kolmo -m eco", "-m", "eco"), kolmo("kolmo -m hc", "-m", "hc"), kolmo("kolmo -m ratio", "-m", "ratio"),
         kolmo("kolmo -m max -l 1", "-m", "max", "-l", "1"), kolmo("kolmo -x", "-x"), kolmo("kolmo -m balanced --native-leaf", "-m", "balanced", "--native-leaf")]
KOLMO_X2 = [kolmo("kolmo -x -l 2", "-x", "-l", "2")]
TYPED = [kolmo("kolmo -m fast --typed-huf", "-m", "fast", "--typed-huf", threads=1)]   # one thread by design
RIVALS = [
    {"label": "zstd -1 -T0", "c": [w("zstd"), "-q", "-f", "-1", "-T0", "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "zstd -3 -T0", "c": [w("zstd"), "-q", "-f", "-3", "-T0", "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "zstd -19 -T0", "c": [w("zstd"), "-q", "-f", "-19", "-T0", "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "{out}", "-o", "{back}"]},
    {"label": "zstd -19 --long=27 -T0", "c": [w("zstd"), "-q", "-f", "-19", "--long=27", "-T0", "{in}", "-o", "{out}"], "d": [w("zstd"), "-q", "-f", "-d", "--long=27", "{out}", "-o", "{back}"]},
    {"label": "lz4 -1 -T0", "c": [w("lz4"), "-q", "-f", "-1", "-T0", "{in}", "{out}"], "d": [w("lz4"), "-q", "-f", "-d", "{out}", "{back}"]},
    {"label": "xz -9 -T0", "c": [w("xz"), "-9", "-T0", "-c", "{in}", ">", "{out}"], "d": [w("xz"), "-d", "-T0", "-c", "{out}", ">", "{back}"]},
    {"label": f"bzip3 -b 16 -j {T}", "c": [w("bzip3"), "-e", "-j", T, "-b", "16", "-c", "{in}", ">", "{out}"], "d": [w("bzip3"), "-d", "-j", T, "-c", "{out}", ">", "{back}"]},
    {"label": "bsc -m0 -e2 (its own threads)", "c": [w("bsc"), "e", "{in}", "{out}", "-m0", "-e2"], "d": [w("bsc"), "d", "{out}", "{back}"]},
    {"label": f"7-Zip LZMA2 -mx9 -mmt={T}", "c": ["sh", f"{scripts}/7z_c.sh", T, "{in}", "{out}"], "d": ["sh", f"{scripts}/7z_d.sh", T, "{out}", "{back}"]},
    {"label": f"zpaq -m5 -t{T}", "c": ["sh", f"{scripts}/zpaq_c.sh", T, "{in}", "{out}"], "d": ["sh", f"{scripts}/zpaq_d.sh", T, "{out}", "{back}"]},
    {"label": "brotli -11 (no threads)", "c": [w("brotli"), "-q", "11", "-f", "{in}", "-o", "{out}"], "d": [w("brotli"), "-d", "-f", "{out}", "-o", "{back}"]},
]
RESEARCH = [
    {"label": f"krc cpu -T {T}", "c": [k("krc-auto"), "-b", "cpu", "-T", T, "{in}", "{out}"], "d": [k("krc-auto"), "-d", "-T", T, "{out}", "{back}"]},
    {"label": f"krc hybrid (GPU + CPU) -T {T}", "c": [k("krc-auto"), "-b", "hybrid", "-T", T, "{in}", "{out}"], "d": [k("krc-auto"), "-d", "-T", T, "{out}", "{back}"]},
    {"label": f"kbc -T {T}", "c": [k("kbc"), "-T", T, "{in}", "{out}"], "d": [k("kbc"), "-d", "-T", T, "{out}", "{back}"]},
    {"label": f"kbx -l 1 -T {T}", "c": [k("kbx"), "-l", "1", "-T", T, "{in}", "{out}"], "d": [k("kbx"), "-d", "-T", T, "{out}", "{back}"]},
]
KFAM = [{"label": f"kfam -T {T}", "c": [k("kfam"), "-T", T, "{in}", "{out}"], "d": [k("kfam"), "-d", "-T", T, "{out}", "{back}"]}]
KVID = [{"label": f"kvid -m {m} -T {T}", "c": [k("kvid"), "-m", m, "-T", T, "{in}", "{out}"], "d": [k("kvid"), "-d", "-T", T, "{out}", "{back}"]} for m in ("balanced", "hc", "ratio", "x")]
FASTQ1 = [{"label": "kolmo-fastq (one thread)", "c": [k("kolmo-fastq"), "{in}", "{out}"], "d": [k("kolmo-fastq"), "-d", "{out}", "{back}"]}]
KBIN = [{"label": f"kbin -T {T}", "c": [k("kbin"), "-T", T, "{in}", "{out}"], "d": [k("kbin"), "-d", "-T", T, "{out}", "{back}"]},
        {"label": f"kbin -C -T {T}", "c": [k("kbin"), "-C", "-T", T, "{in}", "{out}"], "d": [k("kbin"), "-d", "-T", T, "{out}", "{back}"]}]
EXTRA = []
if extra and os.path.exists(f"{extra}/krans"):
    EXTRA.append({"label": f"krans order 0/1 auto -T {T} (entropy coding only)", "c": [f"{extra}/krans", "-o", "a", "-T", T, "{in}", "{out}"], "d": [f"{extra}/krans", "-d", "-T", T, "{out}", "{back}"]})
AUDIO_SET = [kolmo("kolmo -m balanced", "-m", "balanced", threads=1), kolmo("kolmo -m ratio", "-m", "ratio", threads=1),
             kolmo("kolmo -x -l 2", "-x", "-l", "2", threads=1), kolmo("kolmo -m fast --typed-huf", "-m", "fast", "--typed-huf", threads=1),
             {"label": "FLAC level 5 (libsndfile)", "py": "flac5"}, {"label": "FLAC level 8 (libsndfile)", "py": "flac8"}]
if extra and os.path.exists(f"{extra}/kaud"):
    AUDIO_SET += [{"label": f"kaud profile {p}", "c": [f"{extra}/kaud", f"-{p}", "{in}", "{out}"], "d": [f"{extra}/kaud", "-d", "{out}", "{back}"]} for p in ("0", "1")]

json.dump({"general": KOLMO + RIVALS + RESEARCH + EXTRA, "numeric_npy": KOLMO + KOLMO_X2 + TYPED + KBIN + RIVALS[:3] + RIVALS[5:6], "structured": KOLMO + KOLMO_X2 + KFAM + RIVALS + RESEARCH[2:],
           "genomics": KOLMO + KFAM + FASTQ1 + RIVALS + RESEARCH[2:], "numeric": KOLMO + KOLMO_X2 + RIVALS + RESEARCH[2:],
           "video": KVID + KOLMO + RIVALS + RESEARCH[2:], "weights": KOLMO + RIVALS, "audio_set": AUDIO_SET},
          open(out, "w"), indent=1)
print({g: len(v) for g, v in json.load(open(out)).items()})
