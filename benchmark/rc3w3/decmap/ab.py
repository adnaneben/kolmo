# ab.py T ROUNDS OUT.jsonl FRAME... : decode each frame with build A (rc3 58f7cee) and build B (this branch), alternating
# the order every round, best decode speed of ROUNDS rounds (each round is dbench's best of >= 1 s); one thread count.
import sys, subprocess, json, os
T, R, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
A, B = os.environ.get('DB_A', '/home/user/dbench-base2'), os.environ.get('DB_B', '/home/user/dbench-new')
f = open(out, 'a')
for k in sys.argv[4:]:
    best = {'A': 0, 'B': 0}; info = None
    for r in range(R):
        for lab, b in ([('A', A), ('B', B)] if r % 2 == 0 else [('B', B), ('A', A)]):
            o = subprocess.run([b, '-d', '-T', T, '-s', os.environ.get('SECS', '1'), k], check=True, capture_output=True, text=True).stdout.split()
            info = (int(o[1]), int(o[2])); best[lab] = max(best[lab], float(o[3]))
    row = {'frame': k.replace('/home/user/frames/', ''), 'T': int(T), 'size': info[0], 'csize': info[1], 'A_MBs': best['A'], 'B_MBs': best['B'], 'gain': best['B'] / best['A'] - 1}
    f.write(json.dumps(row) + '\n'); f.flush()
    print(f"{row['frame']:40s} T{T} {info[1]:>10d} A {best['A']:8.1f} B {best['B']:8.1f} {row['gain']:+.1%}", flush=True)
