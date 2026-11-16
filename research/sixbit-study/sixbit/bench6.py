"""Benchmark the 6-bit (k6) codec against standard compressors.

usage: python3 bench6.py K6_BINARY KOLMO_BINARY WORKDIR file...
Prints one markdown table per file and writes results.json in WORKDIR.
"""
import collections, json, lzma, math, os, subprocess, sys, time
import brotli, lz4.frame, zstandard

K6, KOLMO, WORK = sys.argv[1], sys.argv[2], sys.argv[3]
FILES = sys.argv[4:]
os.makedirs(WORK, exist_ok=True)

LETTERS = [bytes([c]) for c in range(ord('a'), ord('z') + 1)]
DIGITS = [bytes([c]) for c in range(ord('0'), ord('9') + 1)]
FIXED_EXTRA = [bytes([c]) for c in b' ,\n.-":/_\'();\t+=\r[]@#%&*!']


def write_table(path, entries, fold):
    assert len(entries) <= 61, len(entries)
    with open(path, 'wb') as f:
        f.write(bytes([fold, len(entries)]))
        for e in entries:
            f.write(bytes([len(e)]) + e)


def table_fixed():
    return LETTERS + DIGITS + FIXED_EXTRA, 1


def table_adaptive(data):
    freq = collections.Counter(data)
    return [bytes([b]) for b, _ in freq.most_common(61)], 0


def table_tokens(data, sample=2_000_000):
    """letters+digits with case folding, then the 25 free slots go to whichever single bytes or
    2-8 byte strings save the most symbols (estimated from non-overlapping counts on a sample)."""
    s = data[:sample]
    low = s.lower()
    base = set(LETTERS + DIGITS)
    singles = collections.Counter(low)
    cands = {}
    for b, c in singles.items():
        e = bytes([b])
        if e not in base:
            cands[e] = c * 2          # a byte outside the table costs 3 symbols (ESC + 2)
    for L in range(2, 9):
        cnt = collections.Counter(s[i:i + L] for i in range(0, len(s) - L))
        for g, c in cnt.most_common(200):
            if c < 50:
                break
            nonover = s.count(g)       # bytes.count is non-overlapping
            cands[g] = max(cands.get(g, 0), nonover * (L - 1) - 0)
    chosen = [g for g, _ in sorted(cands.items(), key=lambda kv: -kv[1])[:25]]
    return LETTERS + DIGITS + chosen, 1


def timed(fn, data, reps=3):
    best, out = 1e9, None
    for _ in range(reps):
        t = time.perf_counter(); out = fn(data); best = min(best, time.perf_counter() - t)
    return out, best


CODECS = {
    'lz4': (lambda d: lz4.frame.compress(d), lz4.frame.decompress),
    'zstd -1': (lambda d: zstandard.ZstdCompressor(level=1).compress(d), lambda c: zstandard.ZstdDecompressor().decompress(c)),
    'zstd -3': (lambda d: zstandard.ZstdCompressor(level=3).compress(d), lambda c: zstandard.ZstdDecompressor().decompress(c)),
    'zstd -19': (lambda d: zstandard.ZstdCompressor(level=19).compress(d), lambda c: zstandard.ZstdDecompressor().decompress(c)),
    'brotli -11': (lambda d: brotli.compress(d, quality=11), brotli.decompress),
    'xz -9e': (lambda d: lzma.compress(d, preset=9 | lzma.PRESET_EXTREME), lzma.decompress),
}
SLOW = {'zstd -19', 'brotli -11', 'xz -9e'}


def run_codecs(data, slow_ok=True):
    res = {}
    for name, (c, d) in CODECS.items():
        reps = 1 if name in SLOW else 3
        comp, tc = timed(c, data, reps)
        dec, td = timed(d, comp, 3)
        assert dec == data
        res[name] = dict(size=len(comp), cmb=len(data) / tc / 1e6, dmb=len(data) / td / 1e6)
    return res


def run_kolmo(path, mode):
    out = os.path.join(WORK, 'k.kz'); back = os.path.join(WORK, 'k.back')
    args = [KOLMO, '-T', '1'] + (['-m', mode] if mode else []) + [path, out]
    best_c = best_d = 1e9
    for _ in range(2):
        t = time.perf_counter(); subprocess.run(args, check=True); best_c = min(best_c, time.perf_counter() - t)
        t = time.perf_counter(); subprocess.run([KOLMO, '-d', out, back], check=True); best_d = min(best_d, time.perf_counter() - t)
    assert open(back, 'rb').read() == open(path, 'rb').read()
    n = os.path.getsize(path)
    return dict(size=os.path.getsize(out), cmb=n / best_c / 1e6, dmb=n / best_d / 1e6)


def h0(data):
    n = len(data); c = collections.Counter(data)
    return -sum(v / n * math.log2(v / n) for v in c.values())


results = {}
for path in FILES:
    name = os.path.basename(path)
    data = open(path, 'rb').read()
    n = len(data)
    r = dict(n=n, h0=h0(data), k6={}, raw={}, packed={}, sym={})
    for tname, (entries, fold) in {'K6-fixed': table_fixed(), 'K6-adaptive': table_adaptive(data),
                                    'K6-tokens': table_tokens(data)}.items():
        tp = os.path.join(WORK, f'{name}.{tname}.tbl'); write_table(tp, entries, fold)
        line = subprocess.run([K6, '-b', tp, path], capture_output=True, text=True, check=True).stdout.split()
        _, k, pn, ratio, emb, dmb, ok, esc = line
        assert ok == 'OK'
        r['k6'][tname] = dict(symbols=int(k), size=int(pn), ratio=float(ratio), emb=float(emb), dmb=float(dmb),
                              esc=int(esc.split('=')[1]), table=[e.decode('latin1') for e in entries])
        kp = os.path.join(WORK, f'{name}.{tname}.k6')
        subprocess.run([K6, tp, path, kp], check=True)
        packed = open(kp, 'rb').read()
        sym = open(kp + '.sym', 'rb').read()
        # what a strong back end makes of the 6-bit output: packed (bit-misaligned) and one symbol per byte
        r['packed'][tname] = {c: len(CODECS[c][0](packed)) for c in ('zstd -19', 'xz -9e', 'brotli -11')}
        r['sym'][tname] = {c: len(CODECS[c][0](sym)) for c in ('zstd -19', 'xz -9e')}
    r['raw'] = run_codecs(data)
    for mode in (None, 'speed', 'ratio'):
        r['raw']['kolmo ' + (mode or 'balanced')] = run_kolmo(path, mode)
    results[name] = r

    print(f'\n### {name}  ({n:,} bytes, order-0 entropy {r["h0"]:.3f} bits/byte -> ideal static Huffman-class ratio {8 / r["h0"]:.2f})')
    print('| codec | ratio | enc MB/s | dec MB/s |\n|---|---|---|---|')
    for t, v in r['k6'].items():
        print(f'| {t} (alone) | {v["ratio"]:.3f} | {v["emb"]:.0f} | {v["dmb"]:.0f} |')
    for c, v in r['raw'].items():
        print(f'| {c} | {n / v["size"]:.2f} | {v["cmb"]:.1f} | {v["dmb"]:.0f} |')
    print('| back end on top of K6 | ' + ' | '.join(f'{t}: ' + ', '.join(f'{c} {n / s:.2f}' for c, s in v.items()) for t, v in r['packed'].items()) + ' |')
    print('| back end on K6 symbols (1/byte) | ' + ' | '.join(f'{t}: ' + ', '.join(f'{c} {n / s:.2f}' for c, s in v.items()) for t, v in r['sym'].items()) + ' |')
    sys.stdout.flush()

json.dump(results, open(os.path.join(WORK, 'results.json'), 'w'), indent=1)
