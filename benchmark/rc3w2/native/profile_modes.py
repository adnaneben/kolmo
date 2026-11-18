"""Time and compressed bytes in each dependency, per mode (native lane, rc3 wave 2; from the deps lane's script).

usage: python3 benchmark/rc3w2/native/profile_modes.py build/kolmo-libprof DATA_DIR OUT.tsv [mode ...]
For each mode and each file of DATA_DIR: compress with -T 1, then decompress, under the --wrap profiler
(benchmark/rc3w2/native/libprof.c). Writes one row per (mode, phase, entry point) with calls, bytes, ms, and the whole
process wall time of the phase, summed over the files, and the compressed bytes (column zbytes: written by encoder
calls, read by decoder calls; the decoder figure is the part of the output files coded through that library)."""
import os, subprocess, sys, time, collections, tempfile
K, D, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
MODES = sys.argv[4:] or ["speed", "balanced", "eco", "hc", "ratio"]
files = sorted(f for f in os.listdir(D) if os.path.isfile(os.path.join(D, f)))
tmp = tempfile.mkdtemp()
rows = []
for m in MODES:
    args = ["-x"] if m == "x" else ["-m", m]
    acc = {"c": collections.defaultdict(lambda: [0, 0, 0.0, 0]), "d": collections.defaultdict(lambda: [0, 0, 0.0, 0])}
    wall = {"c": 0.0, "d": 0.0}; insz = outsz = 0
    for f in files:
        src = os.path.join(D, f); z = os.path.join(tmp, "z"); r = os.path.join(tmp, "r")
        for ph, cmd in (("c", [K, *args, "-T", "1", src, z]), ("d", [K, "-d", "-T", "1", z, r])):
            prof = os.path.join(tmp, "p.tsv")
            if os.path.exists(prof): os.remove(prof)
            t = time.perf_counter(); subprocess.run(cmd, check=True, env={**os.environ, "LIBPROF_OUT": prof}); wall[ph] += time.perf_counter() - t
            for line in open(prof):
                k, c, b, ms, zb = line.rstrip("\n").split("\t"); a = acc[ph][k]; a[0] += int(c); a[1] += int(b); a[2] += float(ms); a[3] += int(zb)
        insz += os.path.getsize(src); outsz += os.path.getsize(z)
        assert open(src, "rb").read() == open(r, "rb").read(), f
    for ph in "cd":
        rows.append((m, ph, "_total_", len(files), insz, wall[ph] * 1000, outsz, ""))
        for k, (c, b, ms, zb) in sorted(acc[ph].items(), key=lambda kv: -kv[1][2]):
            rows.append((m, ph, k, c, b, ms, "", zb))
    print(m, "in", insz, "out", outsz, "c %.1fs d %.1fs" % (wall["c"], wall["d"]), file=sys.stderr)
with open(OUT, "w") as o:
    o.write("mode\tphase\tentry\tcalls\tbytes\tms\tout_bytes\tzbytes\n")
    for r in rows: o.write("\t".join(str(round(x, 2)) if isinstance(x, float) else str(x) for x in r) + "\n")
