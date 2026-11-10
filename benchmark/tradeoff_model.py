#!/usr/bin/env python3
"""Weigh a compression-ratio change against a speed change with the models in TRADEOFFS.md.

Each comparison gives a baseline and a candidate as ratio, compress MB/s and decompress MB/s.
The script reports:
  - the exchange rate: percent of bytes saved per percent of combined speed lost
  - the break-even link speed below which the candidate wins on total time
    (compress + transfer + decompress, and transfer + decompress only)
  - the cloud cost model: extra CPU dollars per GB against storage dollars saved per GB-month,
    and the storage lifetime at which the two are equal

Default inputs are the 2026-11-10 measurements of claude/graph against main recorded in
TRADEOFFS.md. Prices are list prices and only meant to give orders of magnitude: change them
with the flags to match your own costs.

Usage:
  python3 benchmark/tradeoff_model.py
  python3 benchmark/tradeoff_model.py --storage 0.004 --vcpu-hour 0.02 --reads 1
  python3 benchmark/tradeoff_model.py --pair "mine:3.1,300,1100,3.2,280,1000"
"""
import argparse

# name: (baseline ratio, compress MB/s, decompress MB/s, candidate ratio, compress MB/s, decompress MB/s)
DEFAULT = {
    'fast':      (3.230, 327.3, 1189.9, 3.302, 302.4, 1037.3),
    'fast -l 9': (3.410, 134.9, 1210.9, 3.475, 128.3, 1030.4),
    'hc -l 9':   (3.466, 102.4, 1129.2, 3.527, 100.4, 1092.3),
}


def weigh(r0, c0, d0, r1, c1, d1, storage, egress, vcpu_hour, reads):
    """All speeds in MB/s of original data; returns a dict of derived figures."""
    mb0, mb1 = 1 / r0, 1 / r1                    # stored MB per MB of input
    saved = mb0 - mb1                             # MB saved per MB of input (negative: candidate is bigger)
    t0, t1 = 1 / c0 + 1 / d0, 1 / c1 + 1 / d1     # seconds per MB, compress + decompress
    dc, dd = 1 / c1 - 1 / c0, 1 / d1 - 1 / d0     # extra seconds per MB (negative: candidate is faster)
    out = {
        'bytes_pct': 100 * saved / mb0,           # percent fewer bytes
        'speed_pct': 100 * (t0 / t1 - 1),         # percent change in combined speed
    }
    out['rate'] = out['bytes_pct'] / -out['speed_pct'] if out['speed_pct'] < 0 and saved > 0 else None
    # Break-even link: candidate wins when saved / B > extra time, i.e. on links slower than B.
    out['link_roundtrip_mbit'] = saved / (dc + dd) * 8 if saved > 0 and dc + dd > 0 else None
    out['link_read_mbit'] = saved / dd * 8 if saved > 0 and dd > 0 else None
    # Cloud cost per GB of input: seconds per MB equal kiloseconds per GB.
    cpu = (dc + reads * dd) * 1000 / 3600 * vcpu_hour
    store = saved / 1000 * 1000 * storage          # MB saved per MB = GB saved per GB
    out['cpu_usd_per_gb'] = cpu
    out['storage_usd_per_gb_month'] = store
    out['egress_usd_per_gb_sent'] = saved * egress
    out['breakeven_days'] = cpu / store * 30 if store > 0 and cpu > 0 else None
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--storage', type=float, default=0.023, help='storage $/GB-month (S3 Standard list price)')
    p.add_argument('--egress', type=float, default=0.09, help='internet egress $/GB (AWS list price)')
    p.add_argument('--vcpu-hour', type=float, default=3.89 / 72, help='$/vCPU-hour (c5n.18xlarge, as in BtrBlocks)')
    p.add_argument('--reads', type=float, default=3.3, help='decompressions per compressed byte (Google fleet average)')
    p.add_argument('--pair', action='append', default=[],
                   help='name:r0,c0,d0,r1,c1,d1 (baseline then candidate); replaces the defaults')
    a = p.parse_args()
    pairs = DEFAULT
    if a.pair:
        pairs = {}
        for s in a.pair:
            name, vals = s.split(':', 1)
            pairs[name] = tuple(float(v) for v in vals.split(','))
    fmt = lambda v, f: '-' if v is None else f.format(v)
    print(f"storage ${a.storage}/GB-month, egress ${a.egress}/GB, vCPU ${a.vcpu_hour:.4f}/h, {a.reads} reads per byte\n")
    print(f"{'setting':10} {'bytes':>7} {'speed':>7} {'rate':>5} {'link rt':>10} {'link read':>10} "
          f"{'CPU $/GB':>9} {'store $/GB-mo':>13} {'break-even':>10}")
    for name, v in pairs.items():
        w = weigh(*v, a.storage, a.egress, a.vcpu_hour, a.reads)
        print(f"{name:10} {-w['bytes_pct']:+6.1f}% {w['speed_pct']:+6.1f}% {fmt(w['rate'], '{:5.2f}')} "
              f"{fmt(w['link_roundtrip_mbit'], '{:6.0f} Mb/s'):>10} {fmt(w['link_read_mbit'], '{:6.0f} Mb/s'):>10} "
              f"{w['cpu_usd_per_gb']:9.1e} {w['storage_usd_per_gb_month']:13.1e} {fmt(w['breakeven_days'], '{:6.1f} days'):>10}")
    print("\nrate: percent of bytes saved per percent of combined speed lost (Google's zstd high-vs-low trade is 0.45).")
    print("link: the candidate wins on total time only on links slower than this.")
    print("break-even: storage lifetime at which the bytes saved pay for the extra CPU.")


if __name__ == '__main__':
    main()
