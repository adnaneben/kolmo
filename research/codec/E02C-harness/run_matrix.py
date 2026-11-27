#!/usr/bin/env python3
"""Run the E02C measurement matrix over the development corpus.

usage (repository root, after `make -f research/codec/E02C-harness/Makefile all check`):
  python3 -I research/codec/E02C-harness/run_matrix.py CORPUS_DIR OUT_DIR [--reps 7] [--seed 20261112] [--no-mem]

Writes OUT_DIR/raw.jsonl (every record, every repetition), OUT_DIR/env.json (identities, toolchain, hardware).
One thread throughout. Input order and configuration order are shuffled per repetition with a recorded seed.
"""
import argparse, hashlib, json, os, pathlib, platform, random, subprocess, sys, time

ROOT = pathlib.Path(__file__).resolve().parents[3]
HERE = pathlib.Path(__file__).resolve().parent
BIN = ROOT / 'build/e02c'

def sha(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def sh(*a): return subprocess.check_output(a, cwd=ROOT, text=True).strip()

def main():
    ap = argparse.ArgumentParser(); ap.add_argument('corpus'); ap.add_argument('out')
    ap.add_argument('--reps', type=int, default=7); ap.add_argument('--seed', type=int, default=20261112); ap.add_argument('--no-mem', action='store_true'); ap.add_argument('--manifest', default=str(HERE / 'corpus/manifest.json'))
    a = ap.parse_args(); corpus = pathlib.Path(a.corpus); out = pathlib.Path(a.out); out.mkdir(parents=True, exist_ok=True)
    man = json.loads(pathlib.Path(a.manifest).read_text())
    for it in man['inputs']:
        if sha(corpus / it['name']) != it['sha256']: raise SystemExit(f'corpus hash mismatch: {it["name"]}')
    env = dict(head=sh('git', 'rev-parse', 'HEAD'), kolmo_c_blob=sh('git', 'rev-parse', 'HEAD:src/kolmo.c'),
               kgraph_blob=sh('git', 'rev-parse', 'HEAD:src/kgraph.inc'), worktree_clean_src=sh('git', 'status', '--porcelain', '--', 'src') == '',
               harness_sha256={p.name: sha(p) for p in sorted((HERE / 'harness').glob('*')) if p.is_file()},
               binary_sha256=sha(BIN), libvendor_sha256=sha(ROOT / 'build/libvendor.a'),
               compiler=subprocess.check_output(['gcc', '--version'], text=True).splitlines()[0],
               harness_flags='-O3 -march=native -fno-plt -ffp-contract=off (core and harness, one translation unit)',
               vendor_flags='make ARCH= build/libvendor.a (-O3 -fno-plt)', cpu=next((l.split(':', 1)[1].strip() for l in open('/proc/cpuinfo') if l.startswith('model name')), ''),
               nproc=os.cpu_count(), kernel=platform.release(), python=sys.version.split()[0], threads=1, reps=a.reps, seed=a.seed,
               started=time.strftime('%Y-%m-%dT%H:%M:%S%z'))
    env['manifest'] = str(a.manifest); env['manifest_sha256'] = sha(a.manifest)
    rng = random.Random(a.seed); jobs = []
    for it in man['inputs']:
        jobs.append(('typed', it, None)); jobs.append(('planner', it, 'fast')); jobs.append(('planner', it, 'hc'))
    rng.shuffle(jobs)
    raw = (out / 'raw.jsonl').open('w')
    for k, (kind, it, mode) in enumerate(jobs):
        f = corpus / it['name']; s = str(rng.randrange(1 << 30)); t0 = time.time()
        cmd = [str(BIN), 'typed', str(f), it['name'], str(it['width']), str(it['head']), '3', s, str(a.reps)] if kind == 'typed' else \
              [str(BIN), 'planner', str(f), it['name'], mode, s, str(a.reps)]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode: raise SystemExit(f'FAILED {cmd}\n{res.stderr}')
        for line in res.stdout.splitlines():
            d = json.loads(line); d['job'] = k; d['regime'] = 'A_typed' if kind == 'typed' else 'B_planner'; raw.write(json.dumps(d, separators=(',', ':')) + '\n')
        print(f'[{k + 1}/{len(jobs)}] {kind} {mode or ""} {it["name"]} {time.time() - t0:.1f}s', flush=True)
    if not a.no_mem:
        confs = [l.split()[1] for l in subprocess.check_output([str(BIN), 'configs'], text=True).splitlines()]
        for it in man['inputs']:
            f = str(corpus / it['name'])
            for op in ('prod_compress', 'prod_decompress_api', 'prod_decode_seq'):
                d = json.loads(subprocess.check_output([str(BIN), 'mem', f, 'fast', op, '-1'], text=True)); d['input'] = it['name']; raw.write(json.dumps(d, separators=(',', ':')) + '\n')
            for c in range(len(confs)):
                for op in ('exp_residual_encode', 'exp_residual_decode', 'exp_decode_seq'):
                    d = json.loads(subprocess.check_output([str(BIN), 'mem', f, 'fast', op, str(c)], text=True)); d['input'] = it['name']; raw.write(json.dumps(d, separators=(',', ':')) + '\n')
            print(f'mem {it["name"]}', flush=True)
    raw.close(); env['finished'] = time.strftime('%Y-%m-%dT%H:%M:%S%z'); env['raw_sha256'] = sha(out / 'raw.jsonl')
    (out / 'env.json').write_text(json.dumps(env, indent=1) + '\n'); print('done', env['raw_sha256'])

if __name__ == '__main__': main()
