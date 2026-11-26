"""Run one encoder or decoder process and measure it.

Adapted from benchmark/perfgraph/cell.py (PR #114): wall time around fork/exec/wait, so process start and file I/O
count; peak RSS and CPU time from wait4. On macOS it also reads instructions retired and cycles with
proc_pid_rusage(RUSAGE_INFO_V4), taken from the exited child before it is reaped (waitid WNOWAIT). That is the
counter PR #37's `icount` used. Instructions do not depend on machine load, so they serve as the cost measure
in sizes-only mode."""
import ctypes
import ctypes.util
import hashlib
import os
import subprocess
import sys
import time

_DARWIN = sys.platform == "darwin"
if _DARWIN:
    _libproc = ctypes.CDLL("/usr/lib/libproc.dylib")
    _libc = ctypes.CDLL(None, use_errno=True)
    _P_PID, _WEXITED, _WNOWAIT, _RUSAGE_INFO_V4 = 1, 0x04, 0x20, 4
    # rusage_info_v4: 16-byte uuid, then uint64 fields; index 28 lifetime_max_phys_footprint, 29 instructions,
    # 30 cycles (counted from the first field after the uuid)
    _I_FOOT, _I_INSTR, _I_CYC = 28, 29, 30


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def clean_env(extra=None):
    """The parent's environment without any KOLMO_* variable, plus `extra`."""
    env = {k: v for k, v in os.environ.items() if not k.startswith("KOLMO_")}
    env.update(extra or {})
    return env


def run(argv, stdout_path, env, cwd=None):
    """-> dict(ns, rc, rss, cpu, instr, cycles, err). rss is ru_maxrss in bytes (macOS); instr/cycles None off macOS."""
    fout = open(stdout_path, "wb") if stdout_path else subprocess.DEVNULL
    try:
        t0 = time.perf_counter_ns()
        p = subprocess.Popen(argv, stdin=subprocess.DEVNULL, stdout=fout, stderr=subprocess.PIPE, cwd=cwd, env=env)
        instr = cycles = None
        if _DARWIN:
            si = ctypes.create_string_buffer(256)
            if _libc.waitid(_P_PID, p.pid, si, _WEXITED | _WNOWAIT) == 0:
                t1 = time.perf_counter_ns()
                buf = (ctypes.c_uint64 * 64)()
                if _libproc.proc_pid_rusage(p.pid, _RUSAGE_INFO_V4, ctypes.byref(buf)) == 0:
                    f = list(buf)[2:]
                    instr, cycles = f[_I_INSTR], f[_I_CYC]
        _, status, ru = os.wait4(p.pid, 0)
        if not _DARWIN:
            t1 = time.perf_counter_ns()
        err = p.stderr.read().decode("utf-8", "replace")[-400:]
        p.stderr.close()
        rc = os.waitstatus_to_exitcode(status)
    finally:
        if stdout_path:
            fout.close()
    rss = ru.ru_maxrss if _DARWIN else ru.ru_maxrss * 1024
    return dict(ns=t1 - t0, rc=rc, rss=rss, cpu=ru.ru_utime + ru.ru_stime, instr=instr, cycles=cycles, err=err)


def load1():
    return os.getloadavg()[0]
