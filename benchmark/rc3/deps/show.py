"""Prints a libprof TSV (benchmark/rc3/deps/profile_modes.py) as a table with the share of each phase's wall time."""
import csv, sys
r = list(csv.reader(open(sys.argv[1]), delimiter='\t'))
tot = {(x[0], x[1]): float(x[5]) for x in r[1:] if x[2] == '_total_'}
for x in r[1:]:
    if x[2].startswith('zstd_level'): print(f"{x[0]:9}{x[1]:2}{x[2]:32}{x[3]:>9}"); continue
    t = tot[(x[0], x[1])]
    print(f"{x[0]:9}{x[1]:2}{x[2]:32}{x[3]:>9}{int(x[4])/1e6:>10.1f}MB{float(x[5]):>11.1f}ms {100*float(x[5])/t:5.1f}%" + (f"  out {int(x[6])/1e6:.2f}MB" if x[6] else ""))
