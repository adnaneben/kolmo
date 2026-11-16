#!/usr/bin/env python3
"""Markdown tables from the JSON written by run.py: totals per set, per-clip wins/losses, per-file sizes.

usage: report.py results.json [--per-file SET]
"""
import json, sys

ORDER = ['flac-5', 'flac-8', 'flac-8-nometa', 'wavpack-hh', 'wavpack-hhx6', 'kolmo-balanced-typedhuf-T1',
         'kolmo-ratio-T1', 'kolmo-x-l2-T1', 'kolmo-ratio-T4', 'kaud-0-T1', 'kaud-0-T4', 'kaud-1-T1', 'kaud-1-T4']
REF = 'flac-8'


def totals(res, s):
    r = res[s]
    ref = r[REF]['size']
    print('| tool | bytes | vs flac -8 | ratio | enc MB/s wall | enc MB/s CPU | dec MB/s wall | dec MB/s CPU |')
    print('|---|---:|---:|---:|---:|---:|---:|---:|')
    for t in ORDER:
        if t not in r:
            continue
        x = r[t]
        print('| %s | %d | %+.2f%% | %.4f | %.2f | %.2f | %.2f | %.2f |' % (
            t, x['size'], 100.0 * (x['size'] / ref - 1), x['size'] / x['raw'], x['enc_MBps_wall'], x['enc_MBps_cpu'],
            x['dec_MBps_wall'], x['dec_MBps_cpu']))
    print()


def wins(res, s, mine=('kaud-0-T1', 'kaud-1-T1')):
    r = res[s]
    rivals = [t for t in ORDER if t in r and not t.startswith('kaud') and not t.endswith('-T4')]
    print('| kaud setting | rival | kaud smaller | kaud larger | equal |')
    print('|---|---|---:|---:|---:|')
    for m in mine:
        if m not in r:
            continue
        for t in rivals:
            a, b = r[m]['per_file'], r[t]['per_file']
            w = sum(1 for f in a if a[f] < b[f])
            l = sum(1 for f in a if a[f] > b[f])
            print('| %s | %s | %d | %d | %d |' % (m, t, w, l, len(a) - w - l))
    print()


def per_file(res, s):
    r = res[s]
    cols = [t for t in ['flac-8', 'wavpack-hhx6', 'kolmo-ratio-T1', 'kolmo-x-l2-T1', 'kaud-0-T1', 'kaud-1-T1'] if t in r]
    print('| file | ' + ' | '.join(cols) + ' | kaud -1 vs flac -8 |')
    print('|---|' + '---:|' * (len(cols) + 1))
    for f in sorted(r[REF]['per_file']):
        v = [r[t]['per_file'][f] for t in cols]
        print('| %s | %s | %+.1f%% |' % (f.replace('.wav', ''), ' | '.join(str(x) for x in v),
                                       100.0 * (r['kaud-1-T1']['per_file'][f] / r[REF]['per_file'][f] - 1)))
    print()


def main():
    res = json.load(open(sys.argv[1]))
    pf = sys.argv[sys.argv.index('--per-file') + 1].split(',') if '--per-file' in sys.argv else []
    for s in res:
        print('### %s (%d files, %d bytes)\n' % (s, next(iter(res[s].values()))['files'], next(iter(res[s].values()))['raw']))
        totals(res, s)
        wins(res, s)
        if s in pf:
            per_file(res, s)


if __name__ == '__main__':
    main()
