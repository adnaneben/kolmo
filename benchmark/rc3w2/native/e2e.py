"""End-to-end Kolmo, default against KOLMO_NATIVE (native lane, rc3 wave 2).

usage: python3 benchmark/rc3w2/native/e2e.py build/kolmo DATA_DIR THREADS OUT.tsv NATIVE_SPEC mode [mode ...]
For each mode: every file of DATA_DIR compressed and decompressed with -T THREADS, once with KOLMO_NATIVE unset and once
with KOLMO_NATIVE=NATIVE_SPEC; sums of output bytes and of process wall time (best of 2 runs per file and phase);
round trips checked."""
import os, subprocess, sys, tempfile, time
K, D, T, OUT, SPEC = sys.argv[1:6]
MODES = sys.argv[6:]
files = sorted(os.path.join(D, f) for f in os.listdir(D) if os.path.isfile(os.path.join(D, f)))
tmp = tempfile.mkdtemp()
env0 = {k: v for k, v in os.environ.items() if k != "KOLMO_NATIVE"}
def best(cmd, env, reps=2):
    b = 1e9
    for _ in range(reps):
        t = time.perf_counter(); subprocess.run(cmd, check=True, env=env); b = min(b, time.perf_counter() - t)
    return b
with open(OUT, "a") as o:
    for m in MODES:
        for label, env in (("library", env0), (SPEC, {**env0, "KOLMO_NATIVE": SPEC})):
            tc = td = 0.0; out = 0
            for f in files:
                z = os.path.join(tmp, "z"); r = os.path.join(tmp, "r")
                tc += best([K, "-m", m, "-T", T, f, z], env)
                td += best([K, "-d", "-T", T, z, r], env0)
                out += os.path.getsize(z)
                assert open(f, "rb").read() == open(r, "rb").read(), (m, label, f)
            o.write(f"{os.path.basename(D.rstrip('/'))}\t{m}\tT{T}\t{label}\t{out}\t{tc:.3f}\t{td:.3f}\n"); o.flush()
            print(m, label, out, f"{tc:.2f} {td:.2f}", file=sys.stderr)
