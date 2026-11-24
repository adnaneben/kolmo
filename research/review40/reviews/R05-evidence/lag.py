import sys, numpy as np
for f in sys.argv[1:]:
    s = np.fromfile(f, np.uint8).astype(np.int16); n = len(s); maxlag = min(16384, n // 4); NS = 4096
    stp = max(1, (n - maxlag) // NS); pos = np.arange(maxlag, n, stp)[:NS]
    lags = np.arange(1, maxlag + 1)
    cost = np.zeros(len(lags), np.int64)
    for i in range(0, len(lags), 2048):
        L = lags[i:i+2048]; cost[i:i+2048] = np.abs(s[pos][:, None] - s[pos[:, None] - L[None, :]]).sum(0)
    order = np.argsort(cost)[:5]; avg = cost.mean()
    best = int(lags[order[0]])
    # smallest lag whose cost is within 10% of the best (a multiple of the row is as good as the row)
    thr = cost[order[0]] * 1.10; small = int(lags[np.nonzero(cost <= thr)[0][0]])
    lc = [int(np.abs(s[pos] - s[pos - c]).sum()) for c in (1, 2, 3, 4)]
    print(f.split('/')[-1], 'n=%d' % n, 'best lags', [(int(lags[o]), int(cost[o])) for o in order], 'mean cost %d' % avg, 'row=%d' % small, 'left-lag costs c=1..4', lc)
