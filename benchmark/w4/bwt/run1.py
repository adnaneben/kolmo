#!/usr/bin/env python3
"""Run one command, print wall seconds and peak RSS (MiB) of the child (os.wait4). Usage: run1.py CMD..."""
import os, sys, time
t = time.perf_counter()
pid = os.fork()
if pid == 0:
    fd = os.open(os.devnull, os.O_WRONLY); os.dup2(fd, 1); os.dup2(fd, 2)
    os.execvp(sys.argv[1], sys.argv[1:])
_, st, ru = os.wait4(pid, 0)
print("%.3f %.1f %d" % (time.perf_counter() - t, ru.ru_maxrss / 1024, os.waitstatus_to_exitcode(st)))
