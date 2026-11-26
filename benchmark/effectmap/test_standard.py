#!/usr/bin/env python3
"""Checks standard.py (docs/ACCEPTANCE.md section 3) on numbers published in PRs, so the formulas give the verdicts
the evidence supports. Run: python3 benchmark/effectmap/test_standard.py"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import standard  # noqa: E402


def speed(cost_change):
    """'+4.9% instructions' -> speed ratio A/B"""
    return 1 / (1 + cost_change)


def m(bytes_, enc, dec=1.0, comb=None, rss=1.0):
    return dict(bytes=bytes_, enc=enc, dec=dec, comb=enc if comb is None else comb, rss=rss)


CASES = [
    # PR #37 (KLZH2 lower-level second encode at -6/-7), its table "Result" and "Per type" (instructions, 46 files)
    ("#37 -6 all files", "6", m(1 - 0.00047, speed(0.049)), False, "TRADE"),
    ("#37 -7 all files", "7", m(1 - 0.00083, speed(0.037)), False, "TRADE"),
    ("#37 -6 audio (same bytes, +49.6%)", "6", m(1.0, speed(0.496)), True, "FAIL"),
    ("#37 -7 weights bf16", "7", m(1 - 0.00373, speed(0.502)), False, "TRADE"),
    ("#37 -6 genomics (same bytes, -2.2%)", "6", m(1.0, speed(-0.022)), True, "KEEP-ID"),
    ("#37 -7 canterbury.tar wall (-1.636%, +59%)", "7", m(1 - 0.01636, speed(0.593)), False, "TRADE"),
    # PR #121 (warm-ups only where they can pay), its instruction and wall tables against main
    ("#121 -7 population-i64 (identical, -62.8%)", "7", m(1.0, speed(-0.628)), True, "KEEP-ID"),
    ("#121 -5 kennedy.xls (identical, +1.9% wall, inside 5-round noise)", "5", m(1.0, 1.0), True, "NEUTRAL"),
    ("#121 -5 records.bin (+0.37%, encode -12%)", "5", m(1.0037, speed(-0.12)), False, "PASS"),
    ("#121 -7 owid f64 (+0.06%, encode -49%)", "7", m(1.0006, speed(-0.49)), False, "PASS"),
    # Codex prior-routing evidence: stacked KLZMA at L8 (Backblaze rows), latency 2.43-2.84x > 1.5
    ("Codex L8 stacked KLZMA, Backblaze", "8", m(338823 / 385328, 155.8 / 563.5, dec=50.1 / 20.1,
                                                  comb=(155.8 + 50.1) / (563.5 + 20.1)), False, "TRADE"),
    # tier edges
    ("fast: 1% smaller at 0.91 combined", "2", m(0.99, 0.91), False, "PASS"),
    ("fast: 0.5% smaller at 0.94 combined", "2", m(0.995, 0.94), False, "TRADE"),
    ("middle: 1% smaller for 10% encode", "5", m(0.99, speed(0.10)), False, "PASS"),
    ("middle: slower decode 0.94", "5", m(0.98, 1.0, dec=0.94), False, "TRADE"),
    ("high: 2.5% smaller at 0.70 encode", "9", m(0.975, 0.70, comb=0.72), False, "PASS"),
    ("high: 1% smaller at 0.85 encode", "9", m(0.99, 0.85, comb=0.86), False, "TRADE"),
    ("high: RSS 1.2x", "8", m(0.97, 0.9, rss=1.2), False, "TRADE"),
    ("ultra: 1.5% smaller at 0.6 encode, 0.85 decode", "ultra", m(0.985, 0.6, dec=0.85, comb=0.65), False, "PASS"),
    ("ultra: 0.5% smaller at 0.9 encode", "ultra", m(0.995, 0.9, comb=0.9), False, "TRADE"),
    ("free size gain", "3", m(0.99, 1.0), False, "KEEP-FREE"),
]


def main():
    bad = 0
    for name, lv, met, ident, want in CASES:
        got, why = standard.classify(lv, met, ident)
        ok = got == want
        bad += not ok
        print(f"{'ok ' if ok else 'BAD'} {name:<62} {got:<9} {'; '.join(why)}")
    print(f"{len(CASES) - bad}/{len(CASES)} as expected")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
