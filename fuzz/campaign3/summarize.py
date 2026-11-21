#!/usr/bin/env python3
"""Summarise libFuzzer fork-mode logs: last status line of each log (executions, cov, ft, corpus, wall seconds,
oom/timeout/crash counts). usage: summarize.py LOG..."""
import re, sys
for p in sys.argv[1:]:
    last = None
    for line in open(p, errors="replace"):
        if re.match(r"#\d+:", line): last = line
    if not last: print(p, "no status"); continue
    g = lambda k: re.search(k + r": (\S+)", last).group(1)
    n = int(last[1:last.index(":")]); t = int(g("time").rstrip("s"))
    print("%-34s %10d execs %6.0f/s cov %6s ft %6s corpus %5s wall %5ds oom/timeout/crash %s" %
          (p.split("/")[-2] + "/" + p.split("/")[-1][:-4], n, n / max(t, 1), g("cov"), g("ft"), g("corp"), t, g("oom/timeout/crash")))
