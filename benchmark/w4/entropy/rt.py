#!/usr/bin/env python3
"""rt.py LABEL CMD...: run CMD, print 'LABEL wall_s peak_rss_kb' (peak RSS of the child via wait4)."""
import os, sys, time
label, cmd = sys.argv[1], sys.argv[2:]
t = time.perf_counter(); pid = os.fork()
if pid == 0:
    fd = os.open(os.devnull, os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2); os.execvp(cmd[0], cmd)
_, st, ru = os.wait4(pid, 0); w = time.perf_counter() - t
print(f"{label} {w:.3f} {ru.ru_maxrss} rc={os.waitstatus_to_exitcode(st)}")
