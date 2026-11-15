#!/usr/bin/env python3
"""Summarise libFuzzer fork-mode logs: per target total executions, average exec/s, final edge coverage (cov),
features (ft), corpus entries, oom/timeout/crash counts and coverage at fixed points in time.
Usage: summarize.py LOG [LOG...]   (several logs of the same target from successive phases are chained)"""
import re, sys, collections
R = re.compile(r'^#(\d+): cov: (\d+) ft: (\d+) corp: (\d+) exec/s: \d+ oom/timeout/crash: (\d+)/(\d+)/(\d+) time: (\d+)s')
marks = [60, 300, 600, 1800, 3600, 5400, 6000]
runs = collections.OrderedDict()
for path in sys.argv[1:]:
    t = re.sub(r'.*/', '', path).replace('.log', '')
    pts = [tuple(map(int, m.groups())) for m in map(R.match, open(path, errors='replace')) if m]
    if pts: runs.setdefault(t, []).append((path, pts))
print('| target | phase log | wall s | executions | exec/s | cov (edges) | ft | corpus | oom/timeout/crash | ' + ' | '.join('cov@%dm' % (m // 60) for m in marks) + ' |')
print('|' + '---|' * (9 + len(marks)))
for t, phases in runs.items():
    for path, pts in phases:
        last = pts[-1]; execs, cov, ft, corp, oom, to, cr, secs = last
        at = []
        for m in marks:
            c = [p[1] for p in pts if p[7] <= m]
            at.append(str(c[-1]) if c and m <= secs + 120 else '')
        print('| %s | %s | %d | %d | %d | %d | %d | %d | %d/%d/%d | %s |' % (t, path.split('/')[-3] if '/logs/' in path else path,
              secs, execs, execs // max(secs, 1), cov, ft, corp, oom, to, cr, ' | '.join(at)))
