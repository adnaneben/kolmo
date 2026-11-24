#!/usr/bin/env python3
"""Unpack the rc3w3 prior lane's per-kind CM priors (src/codecs/cm_kprior.inc on claude/rc3w3-prior-20261118: eight raw
LZMA2 blobs) into one file of 8 x CM_KP_RAW bytes, which build/kolmo-impact reads from KOLMO_CM_KPRIOR_FILE.
usage: kprior.py OUT   (run inside the repository; reads the branch with git show)"""
import lzma, re, subprocess, sys
src = subprocess.run(["git", "show", "origin/claude/rc3w3-prior-20261118:src/codecs/cm_kprior.inc"], capture_output=True, check=True, text=True).stdout
raw_n = int(re.search(r"#define CM_KP_RAW (\d+)", src).group(1))
order = re.search(r"cm_kp_blob\[8\] = \{(.*?)\};", src, re.S).group(1)
names = re.findall(r"\{(cm_kp_\w+), (\d+)\}", order)
filt = [{"id": lzma.FILTER_LZMA2, "preset": 6, "dict_size": 1 << 20, "lc": 3, "lp": 0, "pb": 2}]
with open(sys.argv[1], "wb") as out:
    for name, zn in names:
        body = re.search(r"static const uint8_t %s\[%s\] = \{(.*?)\};" % (name, zn), src, re.S).group(1)
        z = bytes(int(x) for x in re.findall(r"\d+", body))
        assert len(z) == int(zn)
        d = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filt).decompress(z, max_length=raw_n)
        assert len(d) == raw_n, (name, len(d))
        out.write(d)
        print(name, zn, "->", len(d))
