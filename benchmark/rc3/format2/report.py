# report.py DIR : markdown tables from fin_<set>_kolmo-rc2.csv and fin_<set>_final.csv
import csv, sys, os, collections
D = sys.argv[1]
def load(f):
    return {(r[0], r[1]): (int(r[2]), float(r[3]), float(r[4]), r[5]) for r in csv.reader(open(os.path.join(D, f)))}
def table(name, title, per_file=True, group=None):
    a, b = load('fin_%s_kolmo-rc2.csv' % name), load('fin_%s_final.csv' % name)
    modes = []
    for k in a:
        if k[1] not in modes: modes.append(k[1])
    out = ['### ' + title, '']
    out.append('| mode | files | rc2 bytes | rc3 format2 bytes | change | smaller / same / larger | compress s rc2 → rc3 | decompress s rc2 → rc3 |')
    out.append('|---|---:|---:|---:|---:|---|---|---|')
    bad = [k for k in b if b[k][3] != 'ok'] + [k for k in a if a[k][3] != 'ok']
    for m in modes:
        ks = [k for k in a if k[1] == m and k in b]
        sa = sum(a[k][0] for k in ks); sb = sum(b[k][0] for k in ks)
        sm = sum(b[k][0] < a[k][0] for k in ks); eq = sum(b[k][0] == a[k][0] for k in ks); lg = sum(b[k][0] > a[k][0] for k in ks)
        out.append('| `%s` | %d | %s | %s | %+.3f%% | %d / %d / %d | %.1f → %.1f | %.2f → %.2f |' % (m, len(ks), format(sa, ','), format(sb, ','), 100 * (sb - sa) / sa, sm, eq, lg,
                   sum(a[k][1] for k in ks), sum(b[k][1] for k in ks), sum(a[k][2] for k in ks), sum(b[k][2] for k in ks)))
    if per_file:
        out += ['', '| file | mode | rc2 | rc3 | change |', '|---|---|---:|---:|---:|']
        for k in sorted(a, key=lambda k: (k[0], modes.index(k[1]))):
            if k in b and (group is None or group(k[0])) and b[k][0] != a[k][0]:
                out.append('| %s | `%s` | %s | %s | %+.2f%% |' % (k[0], k[1], format(a[k][0], ','), format(b[k][0], ','), 100 * (b[k][0] - a[k][0]) / a[k][0]))
    out.append('')
    if bad: out.append('Round-trip failures: %s\n' % bad)
    else: out.append('Every frame decoded to the original (both builds).\n')
    return '\n'.join(out)
print(table('set', 'Typed arrays, ECG, CSV, model weights, generic (26 files)'))
print(table('wav', 'WAV: 40 ESC-50 clips, the 40 clips concatenated (mono), as 20 stereo pairs, and an odd-sized WAV with a trailing chunk', group=lambda f: not f[0].isdigit()))
print(table('vid', 'Y4M: akiyo, foreman, mobile (CIF, 300 frames)'))
print(table('x', '`-x` and `-x -l 2` on the small numeric files and five WAV files'))
print(table('xv', '`-x` on akiyo'))
