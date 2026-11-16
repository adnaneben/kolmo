"""Map-reduce emulation for GR6+ on CPU cores (no GPU in the test machine).

map    : the file is cut at line boundaries into P blocks; each block is coded by its own GR6+ model in parallel
reduce : the coded blocks are concatenated (with an index); decoding runs the blocks in parallel and concatenates
warm   : every block i >= 1 starts from the model trained on block 0. Estimated as size(block0 + block i) - size(block0),
         which is what a coder whose state is copied after block 0 would produce.
usage: python3 mapreduce.py GR6P WORKDIR THREADS file...
"""
import concurrent.futures as cf, os, subprocess, sys, time

G, WORK, TH = sys.argv[1], sys.argv[2], int(sys.argv[3])
os.makedirs(WORK, exist_ok=True)


def split(data, p):
    n = len(data); cuts = [0]
    for i in range(1, p):
        c = data.find(b'\n', i * n // p)
        if c < 0 or c + 1 <= cuts[-1]:
            continue
        cuts.append(c + 1)
    cuts.append(n)
    return [data[a:b] for a, b in zip(cuts, cuts[1:]) if b > a]


def run(args):
    subprocess.run(args, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def blocks_run(name, parts, tag):
    paths = []
    for i, b in enumerate(parts):
        p = os.path.join(WORK, f'{name}.{tag}.{i}'); open(p, 'wb').write(b); paths.append(p)
    t = time.perf_counter()
    with cf.ThreadPoolExecutor(TH) as ex:
        list(ex.map(run, [[G, 'c', '2', p, p + '.g'] for p in paths]))
    tc = time.perf_counter() - t
    t = time.perf_counter()
    with cf.ThreadPoolExecutor(TH) as ex:
        list(ex.map(run, [[G, 'd', p + '.g', p + '.back'] for p in paths]))
    td = time.perf_counter() - t
    assert b''.join(open(p + '.back', 'rb').read() for p in paths) == b''.join(parts)
    return sum(os.path.getsize(p + '.g') for p in paths) + 8 * len(parts), tc, td, paths


for path in sys.argv[4:]:
    name = os.path.basename(path); data = open(path, 'rb').read(); n = len(data)
    print(f'\n{name} ({n:,} B), {TH} worker threads')
    print('| blocks | size | ratio | vs 1 block | compress MB/s (wall) | decompress MB/s (wall) |\n|---|---|---|---|---|---|')
    base = None
    for P in (1, 4, 16, 64):
        size, tc, td, paths = blocks_run(name, split(data, P), f'p{P}')
        base = base or size
        print(f'| {P} | {size:,} | {n / size:.2f} | {100 * (size - base) / base:+.1f}% | {n / tc / 1e6:.2f} | {n / td / 1e6:.2f} |')
        if P == 16:
            parts = split(data, P)
            s0 = os.path.getsize(paths[0] + '.g')
            joined = [parts[0] + b for b in parts[1:]]
            jp = []
            for i, b in enumerate(joined):
                p = os.path.join(WORK, f'{name}.warm.{i}'); open(p, 'wb').write(b); jp.append(p)
            with cf.ThreadPoolExecutor(TH) as ex:
                list(ex.map(run, [[G, 'c', '2', p, p + '.g'] for p in jp]))
            warm = s0 + sum(os.path.getsize(p + '.g') - s0 for p in jp) + 8 * P
            print(f'| 16 warm-start | {warm:,} | {n / warm:.2f} | {100 * (warm - base) / base:+.1f}% | (est.) | |')
        sys.stdout.flush()
