"""Dense 6-bit layouts: a-z, 0-9, A-Z in order (code 36 = 'A', i.e. the 37th code), symbols absent from the file
pruned so the remaining ones pack densely from code 0; then variants for everything that is not a letter or digit.

  min36       a-z + 0-9 (case folded) + SHIFT, LOCK, ESC: exactly "36 + 3 controls", punctuation escaped
  dense       present a-z, 0-9, A-Z (no SHIFT needed) + ESC; punctuation escaped (ESC + 2 codes = 18 bits)
  dense+page  dense + PAGE: one code opens a second page of 64 bytes (punctuation then costs 12 bits)
  dense+fill  dense, then the slots left free (by pruning and the 26 unused ones) hold the most frequent other bytes
  dense+fill+page  dense+fill, with PAGE for the next 64 bytes
  freq        reference: the 63 most frequent bytes in any order (no letter/digit priority)
usage: python3 bench_dense.py K6_BINARY WORKDIR file...
"""
import collections, os, string, subprocess, sys

K6, WORK = sys.argv[1], sys.argv[2]
os.makedirs(WORK, exist_ok=True)
ORDER = [ord(c) for c in string.ascii_lowercase + string.digits + string.ascii_uppercase]
ALNUM = set(ORDER)


def write(path, entries, fold=0, page=()):
    with open(path, 'wb') as f:
        f.write(bytes([fold, len(entries)]))
        for e in entries:
            f.write(bytes([1, e]))
        f.write(bytes([len(page)]) + bytes(page))


def tables(data):
    freq = collections.Counter(data)
    present = [c for c in ORDER if freq[c]]
    others = [b for b, _ in freq.most_common() if b not in ALNUM]
    fill = present + others[:63 - len(present)]
    fillp = present + others[:62 - len(present)]
    return {
        'min36': (ORDER[:36], 1, ()),
        'dense': (present, 0, ()),
        'dense+page': (present, 0, others[:64]),
        'dense+fill': (fill, 0, ()),
        'dense+fill+page': (fillp, 0, others[62 - len(present):62 - len(present) + 64]),
        'freq': ([b for b, _ in freq.most_common(63)], 0, ()),
    }, present, others, freq


print('| file | alnum present (of 62) | other distinct bytes | non-alnum share of bytes | ' +
      ' | '.join(['min36', 'dense', 'dense+page', 'dense+fill', 'dense+fill+page', 'freq']) + ' |')
print('|---|---|---|---|' + '---|' * 6)
speeds = []
for path in sys.argv[3:]:
    data = open(path, 'rb').read(); n = len(data)
    tbls, present, others, freq = tables(data)
    share = sum(freq[b] for b in others) / n
    cells = []
    for name, (entries, fold, page) in tbls.items():
        tp = os.path.join(WORK, os.path.basename(path) + '.' + name + '.tbl'); write(tp, entries, fold, page)
        out = subprocess.run([K6, '-b', tp, path], capture_output=True, text=True).stdout.split()
        assert out[6] == 'OK', (path, name, out)
        cells.append(f'{float(out[3]):.3f}')
        speeds.append((os.path.basename(path), name, float(out[4]), float(out[5])))
    print(f'| {os.path.basename(path)} | {len(present)} | {len(others)} | {share:.1%} | ' + ' | '.join(cells) + ' |')
    print(f'|  ↳ top non-alnum bytes | ' + ' '.join(repr(chr(b))[1:-1] for b in others[:10]) + ' | | | | | | | | |')

print('\nencode / decode MB/s (scalar prototype):')
for f, name, e, d in speeds:
    if name in ('dense', 'dense+fill', 'dense+fill+page'):
        print(f'  {f:18} {name:16} {e:6.0f} / {d:5.0f}')
