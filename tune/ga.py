#!/usr/bin/env python3
"""Genetic search over cz build-time settings (thresholds, coder levels, context-mixing parameters).

Each genome is compiled into its own czn binary (src/cli.c + src/czn.c against the prebuilt vendor library), run on a
corpus in the chosen modes, and scored by total compressed size relative to the defaults. Genomes that compress or
decompress more than --slack slower than the defaults are penalised, so the search cannot buy ratio with speed.
Every result is verified by a full round trip. Runs until --hours have passed, then writes tune/best.json and a log.

  python3 tune/ga.py --corpus data --modes fast,hc,max --hours 8          (overnight on the Mac: wrap in caffeinate)
  python3 tune/ga.py --show                                               (print the best settings found so far)
"""
import argparse, hashlib, json, os, platform, random, shutil, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PARAMS = [  # name, low, high, default
    ("CZ_TBL_GAIN_PCT", 85, 99, 97), ("CZ_LOG_GAIN_PCT", 80, 99, 92), ("CZ_WORD_GAIN_PCT", 85, 99, 99),
    ("CZ_STRUCT_GAIN_PCT", 85, 99, 97), ("CZ_HC_TEXT_LEVEL", 19, 22, 22), ("CZ_ECO_LEVEL", 5, 9, 8),
    ("CZ_FAST_LEVEL", 1, 5, 3), ("LOG_MIN", 2, 16, 4), ("CM_LR", 10, 14, 12), ("CM_LIM", 1, 4, 2),
]
DEFAULT = {p[0]: p[3] for p in PARAMS}


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True, **kw)


def flags():
    if platform.system() == "Darwin":
        brew = subprocess.run("brew --prefix", shell=True, capture_output=True, text=True).stdout.strip()
        return f"-O3 -mcpu=native -I{brew}/include", f"-L{brew}/lib -lc++"
    return "-O3 -march=native -fno-plt", "-lstdc++"


def build(genome, bindir):
    key = hashlib.sha1(json.dumps(genome, sort_keys=True).encode()).hexdigest()[:12]
    exe = os.path.join(bindir, "czn_" + key)
    if os.path.exists(exe):
        return exe
    cf, lf = flags()
    defs = " ".join(f"-D{k}={v}" for k, v in sorted(genome.items()))
    sh(f"cc {cf} -ffp-contract=off -DXXH_NAMESPACE=ZSTD_ -DZSTD_MULTITHREAD -DZSTD_LEGACY_SUPPORT=0 {defs} "
       f"-Isrc -Ivendor/zstd -Ivendor/lz4 -Ivendor/libbsc -Ivendor/brotli/include src/cli.c src/czn.c build/libvendor.a {lf} "
       f"-llzma -lpthread -lm -o {exe}", cwd=ROOT)
    return exe


def run(exe, files, modes, tmp):
    size = ctime = dtime = 0.0
    for m in modes:
        for f in files:
            c, d = os.path.join(tmp, "x.czn"), os.path.join(tmp, "x.out")
            t0 = time.perf_counter(); sh(f'"{exe}" -m {m} "{f}" "{c}"'); t1 = time.perf_counter()
            sh(f'"{exe}" -d "{c}" "{d}"'); t2 = time.perf_counter()
            if not filecmp(f, d):
                raise RuntimeError(f"round trip failed: {f} mode {m}")
            size += os.path.getsize(c); ctime += t1 - t0; dtime += t2 - t1
    return size, ctime, dtime


def filecmp(a, b):
    with open(a, "rb") as x, open(b, "rb") as y:
        while True:
            p, q = x.read(1 << 20), y.read(1 << 20)
            if p != q:
                return False
            if not p:
                return True


def mutate(g, rate):
    g = dict(g)
    for name, lo, hi, _ in PARAMS:
        if random.random() < rate:
            span = max(1, (hi - lo) // 4)
            g[name] = min(hi, max(lo, g[name] + random.randint(-span, span)))
    return g


def crossover(a, b):
    return {k: (a[k] if random.random() < 0.5 else b[k]) for k in a}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", default="data"); ap.add_argument("--modes", default="fast,hc")
    ap.add_argument("--hours", type=float, default=8); ap.add_argument("--pop", type=int, default=12)
    ap.add_argument("--slack", type=float, default=0.10); ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--show", action="store_true")
    a = ap.parse_args()
    state_path = os.path.join(ROOT, "tune", "best.json")
    if a.show:
        print(open(state_path).read() if os.path.exists(state_path) else "no results yet"); return
    random.seed(a.seed)
    corpus = os.path.join(ROOT, a.corpus) if not os.path.isabs(a.corpus) else a.corpus
    files = sorted(os.path.join(corpus, f) for f in os.listdir(corpus)
                   if os.path.isfile(os.path.join(corpus, f)) and not f.endswith((".zip", ".xz", ".czn")))
    modes = a.modes.split(",")
    bindir = os.path.join(ROOT, "tune", "bin"); os.makedirs(bindir, exist_ok=True)
    if not os.path.exists(os.path.join(ROOT, "build", "libvendor.a")):
        sh("make build/libvendor.a", cwd=ROOT)
    tmp = tempfile.mkdtemp(prefix="cztune")
    log = open(os.path.join(ROOT, "tune", "log.tsv"), "a")
    cache = {}

    def fitness(g):
        k = json.dumps(g, sort_keys=True)
        if k not in cache:
            try:
                s, c, d = run(build(g, bindir), files, modes, tmp)
                cache[k] = (s, c, d)
            except Exception as e:
                print("genome failed:", e, file=sys.stderr); cache[k] = None
        return cache[k]

    base = fitness(DEFAULT)
    if base is None:
        sys.exit("defaults failed to build or run")
    bs, bc, bd = base
    print(f"defaults: {bs:,.0f} bytes, compress {bc:.1f} s, decompress {bd:.1f} s ({len(files)} files x {len(modes)} modes)")

    def score(g):
        r = fitness(g)
        if r is None:
            return 1e9
        s, c, d = r
        pen = max(0.0, c / bc - 1 - a.slack) + max(0.0, d / bd - 1 - a.slack)
        return s / bs * (1 + 2 * pen)

    pop = [DEFAULT] + [mutate(DEFAULT, 0.6) for _ in range(a.pop - 1)]
    end, gen, best = time.time() + a.hours * 3600, 0, (score(DEFAULT), DEFAULT)
    while time.time() < end:
        scored = sorted(((score(g), g) for g in pop), key=lambda t: t[0])
        if scored[0][0] < best[0]:
            best = scored[0]
            s, c, d = fitness(best[1])
            json.dump({"relative_size": best[0], "bytes": s, "default_bytes": bs, "compress_s": c, "decompress_s": d,
                       "settings": best[1], "modes": modes, "files": [os.path.basename(f) for f in files]},
                      open(state_path, "w"), indent=2)
        log.write(f"{gen}\t{time.strftime('%H:%M:%S')}\t{scored[0][0]:.5f}\t{json.dumps(scored[0][1], sort_keys=True)}\n"); log.flush()
        print(f"generation {gen}: best {best[0]:.4f} of default size", flush=True)
        elite = [g for _, g in scored[:2]]
        def pick():
            return min(random.sample(scored, min(3, len(scored))), key=lambda t: t[0])[1]
        pop = elite + [mutate(crossover(pick(), pick()), 0.3) for _ in range(a.pop - 2)]
        gen += 1
    shutil.rmtree(tmp, ignore_errors=True)
    print(f"done after {gen} generations; best {best[0]:.4f} of default size -> {state_path}")


if __name__ == "__main__":
    main()
