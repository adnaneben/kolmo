#!/usr/bin/env python3
"""Summarise aa.py rows: per level, the paired encode/decode time change of each arm against
the first arm (median over rounds of the within-round ratio), per input and summed over inputs.

  python3 aa_report.py rows/aa.jsonl [--ref A] > charts/dev/aa.md
"""
import argparse
import json
import statistics
from collections import defaultdict


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rows")
    ap.add_argument("--ref", default="A")
    a = ap.parse_args()
    rows = [json.loads(line) for line in open(a.rows)]
    arms = [n for n in rows[0]["arms"] if n != a.ref]
    levels = sorted({r["level"] for r in rows}, key=lambda x: (len(x), x))
    by = defaultdict(list)
    for r in rows:
        by[(r["level"], r["input"])].append(r)
    maxload = max(max(r["load1_before"], r["load1_after"]) for r in rows)
    print(f"# Interleaved A/B check ({len(rows)} rounds, max load1 {maxload:.1f})\n")
    print("Change = median over rounds of (arm time / A time) - 1, within the same round; "
          "negative = faster than A. Speed change is the inverse. 'All inputs' sums each round's "
          "times over the inputs first.\n")
    for lv in levels:
        inputs = sorted({i for (l, i) in by if l == lv})
        print(f"## Level {lv}\n")
        print("| Input | " + " | ".join(f"{n} encode time | {n} decode time" for n in arms) + " | bytes same |")
        print("|---|" + "---:|---:|" * len(arms) + "---|")

        def line(name, rs_by_round):
            cells = []
            for n in arms:
                for ph in ("enc_ns", "dec_ns"):
                    rat = [sum(r["arms"][n][ph] for r in rs) / sum(r["arms"][a.ref][ph] for r in rs)
                           for rs in rs_by_round]
                    cells.append(f"{(statistics.median(rat) - 1) * 100:+.1f}%")
            same = all(len({r["arms"][n]["comp_sha"] for n in r["arms"]}) == 1 for rs in rs_by_round for r in rs)
            print(f"| {name} | " + " | ".join(cells) + f" | {'yes' if same else 'no'} |")

        for i in inputs:
            line(i, [[r] for r in by[(lv, i)]])
        rounds = defaultdict(list)
        for i in inputs:
            for r in by[(lv, i)]:
                rounds[r["round"]].append(r)
        line("**All inputs**", [rs for rs in rounds.values() if len(rs) == len(inputs)])
        print()


if __name__ == "__main__":
    main()
