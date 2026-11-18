"""Many kolmo instances at once, sharing the machine-wide pool (docs/CONCURRENCY.md), some killed at random.

usage: python3 tests/stress_jobs.py KOLMO [--quick] [--rounds N]
Each round starts 24 instances (16 with --quick) on a private pool directory with 4 tokens, compressing and
decompressing generated inputs at speed, balanced, ratio and -x with -T 4, and sends SIGKILL or SIGTERM to about a
third of them at random moments. Checks:
  - every instance that finishes writes exactly the reference frame (made beforehand with --jobs-shared=off), and the
    decoded file is the input;
  - a killed instance never leaves a partial destination (absent, or the complete reference), and SIGTERM leaves no
    staged file;
  - afterwards the pool holds no token and lists no live process (kill -9 leaves nothing stale);
  - nothing hangs (each round has a deadline).
"""
import os
import random
import shutil
import signal
import subprocess
import sys
import tempfile
import time

KOLMO = os.path.abspath(sys.argv[1])
QUICK = "--quick" in sys.argv
ROUNDS = int(sys.argv[sys.argv.index("--rounds") + 1]) if "--rounds" in sys.argv else (2 if QUICK else 5)
W = tempfile.mkdtemp(prefix="kolmo-stress-")
POOL = os.path.join(W, "pool")
os.makedirs(POOL, mode=0o700)
ENV = dict(os.environ, KOLMO_JOBS_DIR=POOL, KOLMO_JOBS="4")
ENV.pop("KOLMO_JOBS_SHARED", None)


def make_inputs():
    rnd = random.Random(1118)
    words = [("tok%d" % i) * (1 + i % 3) for i in range(2000)]
    files = {}
    scale = 1 if QUICK else 3
    text = " ".join(words[min(int(rnd.paretovariate(1.2)) - 1, 1999)] + ("\n" if rnd.random() < 0.1 else "") for _ in range(250000 * scale)).encode()
    files["text"] = text
    files["csv"] = ("\n".join(f"{i},{rnd.randint(0, 99999)},{rnd.random():.5f},host{i % 13}" for i in range(80000 * scale)) + "\n").encode()
    files["small"] = text[: 300 << 10]
    for k, v in files.items():
        with open(os.path.join(W, k), "wb") as f:
            f.write(v)
    return files


SETTINGS = [["-m", "speed"], ["-m", "balanced"], ["-m", "ratio", "-B", "1"], ["-x", "-l", "1", "-B", "0.125"]]


def run(args, env=ENV, check=True):
    return subprocess.run([KOLMO] + args, env=env, check=check, stdout=subprocess.PIPE, stderr=subprocess.PIPE)


def status():
    return run(["--jobs-status"]).stdout.decode()


def main():
    files = make_inputs()
    jobs = []
    for name in files:
        for s in SETTINGS:
            if s[0] == "-x" and name != "small":
                continue
            jobs.append((name, s))
    ref = {}
    off = dict(ENV, KOLMO_JOBS_SHARED="off")
    for name, s in jobs:
        out = os.path.join(W, "ref-%s-%s.k" % (name, "".join(s)))
        run(s + ["-T", "4", os.path.join(W, name), out], env=off)
        ref[(name, tuple(s))] = open(out, "rb").read()
    rnd = random.Random(7)
    n = 16 if QUICK else 24
    for r in range(ROUNDS):
        rd = os.path.join(W, "r%d" % r)
        os.makedirs(rd)
        procs = []
        for i in range(n):
            name, s = jobs[rnd.randrange(len(jobs))]
            out = os.path.join(rd, "o%d.k" % i)
            dec = rnd.random() < 0.25
            if dec:        # decode a reference frame
                src = os.path.join(rd, "in%d.k" % i)
                open(src, "wb").write(ref[(name, tuple(s))])
                args = ["-d", "-T", "4", src, out]
            else:
                args = s + ["-T", "4", os.path.join(W, name), out]
            p = subprocess.Popen([KOLMO] + args, env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
            kill = rnd.choice([signal.SIGKILL, signal.SIGTERM]) if rnd.random() < 0.4 else None
            procs.append(dict(p=p, name=name, s=s, out=out, dec=dec, kill=kill, at=time.time() + rnd.uniform(0.002, 0.12)))
        deadline = time.time() + (240 if QUICK else 600)
        while any(x["p"].poll() is None for x in procs):
            now = time.time()
            if now > deadline:
                for x in procs:
                    if x["p"].poll() is None:
                        x["p"].kill()
                sys.exit("FAIL: round %d did not finish in time (hang?)" % r)
            for x in procs:
                if x["kill"] and now >= x["at"] and x["p"].poll() is None:
                    x["p"].send_signal(x["kill"])
                    x["sent"] = True
            time.sleep(0.01)
        done = killed = 0
        for x in procs:
            rc = x["p"].returncode
            want = files[x["name"]] if x["dec"] else ref[(x["name"], tuple(x["s"]))]
            have = open(x["out"], "rb").read() if os.path.exists(x["out"]) else None
            if rc == 0:
                if have != want:
                    sys.exit("FAIL: %s %s: output differs from the reference" % (x["name"], x["s"]))
                done += 1
            else:
                if not x.get("sent"):
                    sys.exit("FAIL: %s %s exited %d: %s" % (x["name"], x["s"], rc, x["p"].stderr.read().decode()))
                if have is not None and have != want:
                    sys.exit("FAIL: killed instance left a partial destination %s" % x["out"])
                killed += 1
            x["p"].stderr.close()
        staged = [f for f in os.listdir(rd) if ".kolmo-" in f]
        sigterm_left = [f for f in staged if any(x["kill"] == signal.SIGTERM and x.get("sent") and x["p"].returncode != 0 for x in procs)]
        nkill9 = sum(1 for x in procs if x["kill"] == signal.SIGKILL and x.get("sent") and x["p"].returncode != 0)
        if len(staged) > nkill9:
            sys.exit("FAIL: %d staged files left, more than the %d instances killed with SIGKILL: %s" % (len(staged), nkill9, sigterm_left))
        st = status()
        if "0 busy of 4" not in st or "processes: 0" not in st:
            sys.exit("FAIL: the pool is not empty after the round:\n" + st)
        print("round %d: %d instances, %d finished with the reference bytes, %d killed (%d staged files from kill -9), pool empty"
              % (r, n, done, killed, len(staged)))
    shutil.rmtree(W)
    print("stress_jobs: all passed")


main()
