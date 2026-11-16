"""Alphabet variants and back ends for the 6-bit study.

  radix      the file's N distinct bytes numbered 0..N-1 and written as one big base-N number:
             exactly log2(N) bits per character (the "42 then 43 rolls over" idea; DEC's RADIX-50 did this with
             40 characters, 3 per 16-bit word)
  bpe63      the file's distinct bytes get codes 0..N-1, codes N..62 get the most frequent pairs/strings
             (byte-pair merges), ESC = 63; measured with the k6 codec (greedy longest match)
  back ends  on GR6 units (repeat + graph output, one 6-bit unit per byte): fixed 6-bit, static Huffman (huff0),
             xz, and the bit-level neural coder in 6-bit mode
usage: python3 variants.py K6 HUFFBENCH NNCM WORKDIR GR6_UNITS_DIR file...
"""
import collections, lzma, math, os, subprocess, sys

K6, HUFF, NN, WORK, GRD = sys.argv[1:6]
os.makedirs(WORK, exist_ok=True)


def bpe_table(data, sample=1_000_000):
    seq = [bytes([b]) for b in data[:sample]]
    distinct = sorted(set(data), key=lambda b: -data.count(bytes([b])))
    entries = [bytes([b]) for b in distinct][:63]
    while len(entries) < 63:
        pairs = collections.Counter(zip(seq, seq[1:]))
        (a, b), c = pairs.most_common(1)[0]
        if c < 20 or len(a) + len(b) > 8:
            break
        m = a + b; entries.append(m)
        out = []; i = 0
        while i < len(seq):
            if i + 1 < len(seq) and seq[i] == a and seq[i + 1] == b:
                out.append(m); i += 2
            else:
                out.append(seq[i]); i += 1
        seq = out
    return entries


def k6_ratio(path, entries):
    tp = os.path.join(WORK, 'bpe.tbl')
    with open(tp, 'wb') as f:
        f.write(bytes([0, len(entries)]))
        for e in entries:
            f.write(bytes([len(e)]) + e)
        f.write(b'\0')
    o = subprocess.run([K6, '-b', tp, path], capture_output=True, text=True).stdout.split()
    assert o[6] == 'OK'
    return float(o[3]), float(o[4]), float(o[5])


print('| file | distinct bytes | plain 6-bit | radix (log2 N bits/char) | bpe63 | GR6 + fixed 6-bit | GR6 + Huffman | GR6 + xz | GR6 + neural | xz on original | Huffman on original |')
print('|---|---|---|---|---|---|---|---|---|---|---|')
for path in sys.argv[6:]:
    name = os.path.basename(path); data = open(path, 'rb').read(); n = len(data)
    N = len(set(data))
    radix = n * math.log2(N) / 8 + N
    plain = n * 6 / 8 if N <= 64 else None
    r_bpe, e_bpe, d_bpe = k6_ratio(path, bpe_table(data))
    units = os.path.join(GRD, name + '.u'); u = open(units, 'rb').read()
    fixed = len(u) * 6 / 8
    hf = subprocess.run([HUFF, units], capture_output=True, text=True).stdout.split()
    huff = len(u) / float(hf[hf.index('ratio') + 1])
    xzu = len(lzma.compress(u, preset=9 | lzma.PRESET_EXTREME))
    nn = subprocess.run([NN, 'c', '2', '6', units, os.path.join(WORK, 'nn.o')], capture_output=True, text=True).stderr
    nnsize = os.path.getsize(os.path.join(WORK, 'nn.o'))
    xzr = len(lzma.compress(data, preset=9 | lzma.PRESET_EXTREME))
    hr = subprocess.run([HUFF, path], capture_output=True, text=True).stdout.split()
    hraw = n / float(hr[hr.index('ratio') + 1])

    def cell(size):
        return f'{100 * size / n:.1f}% ({n / size:.2f})'
    print(f'| {name} | {N} | ' + (cell(plain) if plain else 'n/a (>64)') + f' | {cell(radix)} ({math.log2(N):.2f} b) | '
          f'{100 / r_bpe:.1f}% ({r_bpe:.2f}) | {cell(fixed)} | {cell(huff)} | {cell(xzu)} | {cell(nnsize)} | {cell(xzr)} | {cell(hraw)} |')
    sys.stdout.flush()
