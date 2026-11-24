#!/usr/bin/env python3
"""ru.py label -- cmd...: run once, print wall, user, sys, minor faults, maxrss (rusage of the child)."""
import os, sys, time, resource, subprocess
lab = sys.argv[1]; cmd = sys.argv[3:]
t = time.perf_counter(); r0 = resource.getrusage(resource.RUSAGE_CHILDREN)
rc = subprocess.call(cmd, stdout=subprocess.DEVNULL)
r1 = resource.getrusage(resource.RUSAGE_CHILDREN)
print(f"{lab}: wall {time.perf_counter()-t:.3f}s user {r1.ru_utime-r0.ru_utime:.3f}s sys {r1.ru_stime-r0.ru_stime:.3f}s minflt {r1.ru_minflt-r0.ru_minflt} maxrss {r1.ru_maxrss/1024:.1f} MiB exit {rc}")
