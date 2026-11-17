#!/usr/bin/env python3
"""Compare frames of base and new kolmo for the non-ultra modes (and -x -l 1) on the benchmark files."""
import hashlib, os, subprocess, sys

SP = "/private/tmp/claude-501/-Users-adnanbenali-Projects-kolmo/150de77a-2a2f-5cf3-b144-4e505fb7f30d/scratchpad/gr6"
D = "/Users/adnanbenali/Downloads/kolmo-bench/data"
FILES = [D + "/" + f for f in ("owid-co2-data.csv", "owid_test.csv", "energy_test.csv", "wide_test.csv", "loghub_all.log", "dickens", "api.json")]
FILES.append(SP.rsplit("/gr6", 1)[0] + "/dl/covid/countries-aggregated.csv")
MODES = [["-m", "speed"], ["-m", "balanced"], ["-m", "eco"], ["-m", "hc"], ["-m", "ratio"], ["-x", "-l", "1"]]
W = SP + "/wf"
os.makedirs(W, exist_ok=True)
bad = 0
for f in FILES:
    for m in MODES:
        h = {}
        for b in ("base", "new"):
            o = "%s/%s.%s" % (W, os.path.basename(f), b)
            subprocess.run([SP + "/bin/kolmo-" + b] + m + ["-T", "1", f, o], check=True, stdout=subprocess.DEVNULL)
            with open(o, "rb") as fh: data = fh.read()
            h[b] = (len(data), hashlib.sha256(data).hexdigest()[:16])
            os.remove(o)
        same = h["base"] == h["new"]
        bad += not same
        print("%-28s %-14s %s %d %s" % (os.path.basename(f), " ".join(m), "same" if same else "DIFF", h["base"][0], "" if same else h["new"]), flush=True)
print("frames differing:", bad)
