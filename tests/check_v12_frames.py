#!/usr/bin/env python3
"""The frozen version-12 corpus through the CLI (COMPATIBILITY.md).

usage: python3 tests/check_v12_frames.py build/kolmo [tests/v12]

For every release directory tests/v12/r*/ with a frames.json: each frame file is still the frame that release wrote
(SHA-256 unchanged: the corpus is never regenerated), no frame file is unlisted, and the kolmo binary given decodes
each frame (one-shot CZN1 frames and CZNS streams) and tests it with -t, to the original's SHA-256 and length.
test-v12-corpus covers the same CZN1 frames through the library (whole buffer, 1 and 4 threads, range read).
"""
import glob, hashlib, json, os, subprocess, sys, tempfile

kolmo = sys.argv[1]
root = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(os.path.abspath(__file__)), "v12")
bad, count = [], 0
with tempfile.TemporaryDirectory() as tmp:
    out = os.path.join(tmp, "out")
    for jpath in sorted(glob.glob(os.path.join(root, "r*", "frames.json"))):
        d = os.path.dirname(jpath); man = json.load(open(jpath))
        listed = {e["file"] for e in man["frames"]}
        for f in sorted(os.listdir(d)):
            if f.endswith(".kolmo") and f not in listed: bad.append("%s: not in frames.json" % f)
        for e in man["frames"]:
            p = os.path.join(d, e["file"]); count += 1
            if not os.path.exists(p): bad.append("%s: missing" % e["file"]); continue
            if hashlib.sha256(open(p, "rb").read()).hexdigest() != e["frame_sha256"]:
                bad.append("%s: frame changed (frozen frames are never rewritten)" % e["file"]); continue
            r = subprocess.run([kolmo, "-d", "-q", "-f", p, "-o", out], capture_output=True, text=True)
            got = open(out, "rb").read() if r.returncode == 0 and os.path.exists(out) else None
            if got is None or len(got) != e["decoded_bytes"] or hashlib.sha256(got).hexdigest() != e["decoded_sha256"]:
                bad.append("%s: does not decode to its original (%s)" % (e["file"], r.stderr.strip()[:120]))
            if os.path.exists(out): os.remove(out)
            if subprocess.run([kolmo, "-t", "-q", p], capture_output=True).returncode:
                bad.append("%s: kolmo -t fails" % e["file"])
for b in bad: print(b)
if bad or not count:
    sys.exit("V12 FRAMES (CLI) FAILED (%d problems, %d frames)" % (len(bad), count))
print("V12 FRAMES (CLI) PASSED (%d frozen frames, streams included)" % count)
