"""effectmap.py report: rows -> effectmap.md, cells.csv, verdict.json (docs/ACCEPTANCE.md)."""
import csv
import json
import math
import os
import statistics as st
from collections import defaultdict

import rules
import standard

NOISE_FLOOR = {"instr": 0.003, "wall": 0.01}
INV_PCT, INV_BYTES = 0.005, 16          # the ledger's level-order threshold (benchmark/ledger)
LVL_ORDER = [str(i) for i in range(1, 10)] + ["ultra"]


def _load(out):
    rows = {}
    for mode in ("sizes", "timing"):
        p = os.path.join(out, f"rows-{mode}.jsonl")
        if os.path.exists(p):
            rows[mode] = [json.loads(l) for l in open(p)]
    meta = {}
    for mode in ("timing", "sizes"):
        p = os.path.join(out, f"meta-{mode}.json")
        if os.path.exists(p):
            meta = json.load(open(p))
            break
    return rows, meta


def gmean(xs):
    xs = [x for x in xs if x and x > 0]
    return math.exp(sum(math.log(x) for x in xs) / len(xs)) if xs else 1.0


def pct(x):
    return f"{100 * (x - 1):+.3f}%"


def _cost(arm, phase, basis):
    if basis == "wall":
        return arm[f"{phase}_ns"]
    return arm.get(f"{phase}_instr") or arm[f"{phase}_cpu"]


def per_file(rows, basis):
    """group rounds -> one record per (part, input, level, threads)"""
    g = defaultdict(list)
    for r in rows:
        if any(v.get("error") for v in r["arms"].values()):
            continue
        g[(r["part"], r["input"], r["level"], r["threads"])].append(r)
    recs = []
    for (part, inp, lv, t), rs in g.items():
        r0 = rs[0]
        A, B = r0["arms"]["A"], r0["arms"]["B"]
        rec = dict(part=part, input=inp, kind=r0["kind"], split=r0["split"], size_class=r0["size_class"],
                   level=lv, threads=t, bytes_in=r0["bytes_in"], a_bytes=A["comp_bytes"], b_bytes=B["comp_bytes"],
                   a_sha=A["comp_sha"], b_sha=B["comp_sha"], identical=A["comp_sha"] == B["comp_sha"],
                   rounds=len(rs))
        if "A2" in r0["arms"]:
            for ph in ("enc", "dec"):
                rec[ph] = st.median(_cost(r["arms"]["A"], ph, basis) / _cost(r["arms"]["B"], ph, basis) for r in rs)
                rec["aa_" + ph] = st.median(_cost(r["arms"]["A"], ph, basis) / _cost(r["arms"]["A2"], ph, basis)
                                            for r in rs)
            rec["comb"] = st.median((_cost(r["arms"]["A"], "enc", basis) + _cost(r["arms"]["A"], "dec", basis)) /
                                    (_cost(r["arms"]["B"], "enc", basis) + _cost(r["arms"]["B"], "dec", basis))
                                    for r in rs)
            rec["aa_comb"] = st.median((_cost(r["arms"]["A"], "enc", basis) + _cost(r["arms"]["A"], "dec", basis)) /
                                       (_cost(r["arms"]["A2"], "enc", basis) + _cost(r["arms"]["A2"], "dec", basis))
                                       for r in rs)
            rec["rss"] = max(st.median(r["arms"]["B"][f"{ph}_rss"] for r in rs) /
                             st.median(r["arms"]["A"][f"{ph}_rss"] for r in rs) for ph in ("enc", "dec"))
            rec["aa_rss"] = max(st.median(r["arms"]["A2"][f"{ph}_rss"] for r in rs) /
                                st.median(r["arms"]["A"][f"{ph}_rss"] for r in rs) for ph in ("enc", "dec"))
        recs.append(rec)
    return recs


def noise_bands(recs, basis):
    """p95 of |A/A2 - 1| per (threads, size class, metric), pooled over levels (the A/A spread does not depend on
    the change). Under 8 samples the band is 1.5x the largest one seen. Keys are stored per level too, for lookup."""
    g = defaultdict(list)
    levels = set()
    for r in recs:
        if r["part"] == "main" and "aa_enc" in r:
            levels.add(r["level"])
            for m in ("enc", "dec", "comb", "rss"):
                g[(r["threads"], r["size_class"], m)].append(abs(r["aa_" + m] - 1))
    band = {}
    for (t, sc, m), v in g.items():
        v = sorted(v)
        b = v[min(len(v) - 1, int(math.ceil(0.95 * len(v))) - 1)] if len(v) >= 8 else 1.5 * v[-1]
        b = max(b, NOISE_FLOOR[basis] if m != "rss" else 0.02)
        for lv in levels:
            band[(lv, t, sc, m)] = b
    return band


def snap(x, n):
    return 1.0 if abs(x - 1) <= n else x


def metrics(recs, band, lv, t):
    """cell metrics; the noise band is the widest of the size classes in the cell"""
    scs = {r["size_class"] for r in recs}
    nb = lambda k, d: max(band.get((lv, t, sc, k), d) for sc in scs)
    m = dict(bytes=sum(r["b_bytes"] for r in recs) / sum(r["a_bytes"] for r in recs))
    for k in ("enc", "dec", "comb"):
        m[k] = snap(gmean([r[k] for r in recs]), nb(k, 0.01))
    m["rss"] = snap(max(r["rss"] for r in recs), nb("rss", 0.02))
    return m


def inversions(recs, arm):
    """files where a level's output is larger than the level below's (threads 1)"""
    by = defaultdict(dict)
    for r in recs:
        if r["part"] == "main" and r["threads"] == 1:
            by[r["input"]][r["level"]] = (r[f"{arm}_bytes"], r)
    out = set()
    for inp, d in by.items():
        lv = sorted(d, key=LVL_ORDER.index)
        for lo, hi in zip(lv, lv[1:]):
            if LVL_ORDER.index(hi) != LVL_ORDER.index(lo) + 1:
                continue
            b_lo, b_hi = d[lo][0], d[hi][0]
            if b_hi > b_lo * (1 + INV_PCT) and b_hi - b_lo > INV_BYTES:
                r = d[hi][1]
                out.add((inp, f"-{hi}>-{lo}", r["kind"], r["size_class"], f"{100 * (b_hi / b_lo - 1):+.2f}%"))
    return out


def thread_dependent(recs, arm):
    g = defaultdict(set)
    for r in recs:
        if r["part"] == "main":
            g[(r["input"], r["level"])].add(r[f"{arm}_sha"])
    return sorted(k for k, v in g.items() if len(v) > 1)


def main(out):
    rows, meta = _load(out)
    if not rows:
        raise SystemExit(f"no rows in {out}")
    basis = "wall" if "timing" in rows else "instr"
    recs = per_file(rows.get("timing") or rows["sizes"], basis)
    if "timing" in rows and "sizes" in rows:      # stacking rows live in the sizes file
        recs += [r for r in per_file(rows["sizes"], "instr") if r["part"] != "main"]
    main_recs = [r for r in recs if r["part"] == "main" and "enc" in r]
    band = noise_bands(main_recs, basis)
    all_identical = all(r["identical"] for r in main_recs)

    # per file classes (tree samples) and cells
    for r in main_recs:
        bk = lambda k, d: band.get((r["level"], r["threads"], r["size_class"], k), d)
        m = dict(bytes=r["b_bytes"] / r["a_bytes"], rss=snap(r["rss"], bk("rss", .02)),
                 **{k: snap(r[k], bk(k, .01)) for k in ("enc", "dec", "comb")})
        r["m"] = m
        r["cls"], r["why"] = standard.classify(r["level"], m, r["identical"])
    cells = defaultdict(list)
    for r in main_recs:
        cells[(r["level"], r["kind"], r["size_class"], r["threads"])].append(r)
    cell_rows = []
    for (lv, kind, sc, t), rs in sorted(cells.items(), key=lambda kv: (LVL_ORDER.index(kv[0][0]), kv[0][1],
                                                                        kv[0][2], kv[0][3])):
        m = metrics(rs, band, lv, t)
        ident = all(r["identical"] for r in rs)
        cls, why = standard.classify(lv, m, ident)
        worst = max(rs, key=lambda r: r["b_bytes"] / r["a_bytes"])
        slowest = min(rs, key=lambda r: r["enc"])
        cell_rows.append(dict(level=lv, tier=standard.tier(lv), kind=kind, size_class=sc, threads=t, n=len(rs),
                              a_bytes=sum(r["a_bytes"] for r in rs), b_bytes=sum(r["b_bytes"] for r in rs),
                              identical=ident, cls=cls, why="; ".join(why), worst_file=worst["input"],
                              worst_bytes=worst["b_bytes"] / worst["a_bytes"], slowest_file=slowest["input"],
                              slowest_enc=slowest["enc"], **m))
    with open(os.path.join(out, "cells.csv"), "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(cell_rows[0].keys()))
        w.writeheader()
        w.writerows(cell_rows)

    # never allowed
    inv_a, inv_b = inversions(main_recs, "a"), inversions(main_recs, "b")
    added, removed = sorted(inv_b - inv_a), sorted(inv_a - inv_b)
    td_b = thread_dependent(main_recs, "b")
    dec_bad = [c for c in cell_rows if standard.decode_violation(c["level"], c["dec"])]
    rss_bad = [c for c in cell_rows if c["rss"] > standard.RSS_MAX]
    violations = ([f"adds level inversion {i[1]} on {i[0]} ({i[4]})" for i in added] +
                  [f"output depends on thread count: {k[0]} -{k[1]}" for k in td_b] +
                  [f"decode slower beyond noise at -{c['level']}: {c['kind']} {c['size_class']} T{c['threads']} "
                   f"({pct(c['dec'])} speed)" for c in dec_bad] +
                  [f"RSS {c['rss']:.2f}x at -{c['level']}: {c['kind']} {c['size_class']} T{c['threads']}"
                   for c in rss_bad])

    # gain threshold: 0.5% on some type
    by_kind = defaultdict(lambda: dict(bytes=1.0, speed=1.0))
    for c in cell_rows:
        k = by_kind[c["kind"]]
        k["bytes"] = min(k["bytes"], c["bytes"])
        k["speed"] = max(k["speed"], c["enc"], c["dec"], c["comb"])
    gain_kinds = sorted(k for k, v in by_kind.items()
                        if v["bytes"] <= 1 - standard.GAIN_MIN or v["speed"] >= 1 + standard.GAIN_MIN)

    counts = defaultdict(int)
    for c in cell_rows:
        counts[c["cls"]] += 1
    nonneutral = [c for c in cell_rows if c["cls"] != "NEUTRAL"]
    wins = [c for c in nonneutral if c["cls"] in standard.WIN]
    losses = [c for c in nonneutral if c["cls"] in standard.LOSE]
    trades = [c for c in nonneutral if c["cls"] == "TRADE"]
    if violations:
        verdict = "FAIL (never allowed)"
    elif not gain_kinds:
        verdict = "FAIL (no 0.5% gain on any type)" if not removed else "KEEP (removes level inversions only)"
    elif nonneutral and all(c["cls"] == "KEEP-ID" for c in nonneutral):
        verdict = "KEEP (byte-identical speedup)"
    elif nonneutral and all(c["cls"] in ("KEEP-ID", "KEEP-FREE") for c in nonneutral):
        verdict = "KEEP (free gain)"
    elif not losses:
        verdict = "PASS"
    elif wins:
        verdict = "ROUTE (passes on some conditions only: gate it)"
    elif len(trades) == len(losses):
        verdict = "TRADE (decide: new mode or rebase, ACCEPTANCE.md section 6)"
    else:
        verdict = "FAIL"
    tier_verdicts = {}
    for t in ("fast", "middle", "high", "ultra"):
        cs = [c for c in nonneutral if c["tier"] == t]
        if any(c["tier"] == t for c in cell_rows):
            w_ = sum(c["cls"] in standard.WIN for c in cs)
            tier_verdicts[t] = ("no effect" if not cs else "pass" if w_ == len(cs) else "fail" if not w_ else
                                f"mixed: {w_} of {len(cs)} cells pass")

    # rules: dev fit, holdout check
    feats = ["kind", "size_class", "level"] + (["threads"] if all_identical else [])
    samp = lambda split: [dict(kind=r["kind"], size_class=r["size_class"], level=r["level"],
                               threads=str(r["threads"]), win=r["cls"] in standard.WIN)
                          for r in main_recs if r["split"] == split and r["cls"] != "NEUTRAL"]
    dev, hold = samp("dev"), samp("holdout")
    leaves = rules.fit(dev, feats) if dev else []
    hscore, hacc = rules.score(leaves, hold) if leaves else ([], None)
    gate_leaves = []
    for (conds, n, w), (hn, hw) in zip(leaves, hscore):
        if w * 2 >= n and n:
            holds = hn == 0 or hw * 2 >= hn
            gate_leaves.append((rules.leaf_text(conds), n, w, hn, hw, holds))
    if not leaves:
        gate = "none: no condition changes (every row neutral)" if not dev else "none"
    elif all(w * 2 >= n for _, n, w in leaves):
        gate = "none needed: on everywhere it was measured"
    elif not gate_leaves:
        gate = "none: no condition where it wins on dev"
    else:
        ok = [g for g in gate_leaves if g[5]]
        gate = ("on when " + " OR ".join(f"({g[0]})" for g in ok)) if ok else "none: dev rules fail on holdout"
    if removed:
        gate += ("; in any case keep it where it removes an inversion: " +
                 ", ".join(sorted({f"{i[2]} {i[3]} {i[1]}" for i in removed})))

    stack_tab = stacking(recs)

    v = dict(verdict=verdict, basis=basis, tiers=tier_verdicts, violations=violations,
             removed_inversions=[list(i) for i in removed], added_inversions=[list(i) for i in added],
             gain_kinds=gain_kinds, counts=dict(counts), gate=gate, holdout_accuracy=hacc,
             rules=[dict(rule=rules.leaf_text(c), dev_n=n, dev_wins=w, holdout_n=hn, holdout_wins=hw)
                    for (c, n, w), (hn, hw) in zip(leaves, hscore)])
    with open(os.path.join(out, "verdict.json"), "w") as f:
        json.dump(v, f, indent=1)
    write_md(out, meta, basis, band, cell_rows, main_recs, v, stack_tab, inv_a, inv_b)
    print(f"{verdict}\n-> {os.path.join(out, 'effectmap.md')}")


def stacking(recs):
    alone = defaultdict(lambda: [0, 0])
    for r in recs:
        if r["part"] == "main" and r["threads"] == 1:
            alone[(r["input"], r["level"])] = [r["a_bytes"], r["b_bytes"]]
    agg = defaultdict(lambda: [0, 0, 0, 0, 0])
    for r in recs:
        if not r["part"].startswith("stack:") or (r["input"], r["level"]) not in alone:
            continue
        p = r["part"][6:]
        al = alone[(r["input"], r["level"])]
        for key in ((p, r["kind"]), (p, "(all)")):
            a = agg[key]
            a[0] += al[0]; a[1] += al[1]; a[2] += r["a_bytes"]; a[3] += r["b_bytes"]; a[4] += 1
    out = []
    for (p, kind), (a0, b0, a1, b1, n) in sorted(agg.items()):
        ga, gs = b0 / a0, b1 / a1
        if abs(ga - 1) < 1e-5 and abs(gs - 1) < 1e-5:
            lab = "none"
        elif gs > 1 + 1e-5 and gs > ga:
            lab = "conflicts"
        elif gs <= ga - 0.005:
            lab = "amplifies"
        elif ga < 1 and (1 - gs) < 0.5 * (1 - ga):
            lab = "absorbed"
        else:
            lab = "stacks"
        out.append(dict(partner=p, kind=kind, n=n, alone=ga, with_=gs, label=lab))
    return out


def write_md(out, meta, basis, band, cells, recs, v, stack_tab, inv_a, inv_b):
    L = []
    names = meta.get("names", {})
    L.append(f"# Effect map: {names.get('B', 'B')} against {names.get('A', 'A')}\n")
    L.append(f"**Verdict: {v['verdict']}**\n")
    for t, s in v["tiers"].items():
        L.append(f"- {t} tier: {s}")
    L.append(f"- gain of 0.5% or more on: {', '.join(v['gain_kinds']) or 'no type'}")
    L.append(f"- suggested gate: {v['gate']}")
    L.append(f"- cells: " + ", ".join(f"{k} {n}" for k, n in sorted(v["counts"].items())))
    L.append("")
    L.append("## Builds and method\n")
    for n, a in meta.get("arms", {}).items():
        L.append(f"- {n}: `{a['binary']}` sha256 `{a['sha256'][:16]}` env {a['env'] or '{}'}")
    lo = [r for r in recs]
    L.append(f"- cost basis: **{basis}** "
             + ("(instructions retired from proc_pid_rusage, load-independent; sizes-only mode, 1 round)"
                if basis == "instr" else "(wall time, interleaved rounds under timed.sh)"))
    L.append(f"- {len(recs)} condition rows (file x level x threads); arms A, B and A2 (A again) rotate per round; "
             "speed ratios inside the A/A band count as 1.00")
    L.append(f"- command: `{' '.join(meta.get('cmd', []))}`\n")
    L.append("## Never allowed\n")
    L.append(f"- level inversions (threads 1, >0.5% and >16 B): A {len(inv_a)}, B {len(inv_b)}")
    for i in v["added_inversions"]:
        L.append(f"  - **added** {i[1]} {i[0]} ({i[2]} {i[3]}, {i[4]})")
    for i in v["removed_inversions"]:
        L.append(f"  - removed {i[1]} {i[0]} ({i[2]} {i[3]}, {i[4]} on A)")
    L.append(f"- violations: {len(v['violations'])}")
    for x in v["violations"][:40]:
        L.append(f"  - {x}")
    L.append("")
    L.append("## A/A noise band (p95 of |A/A2 - 1| per threads and size class, pooled over levels)\n")
    L.append("| level | threads | size | enc | dec | comb | rss |\n|---|---|---|---:|---:|---:|---:|")
    lv0 = min({k[0] for k in band}, key=LVL_ORDER.index) if band else None
    for lv, t, sc in sorted({k[:3] for k in band if k[0] == lv0}, key=lambda x: (x[1], x[2])):
        L.append(f"| all | {t} | {sc} | " + " | ".join(f"{100 * band.get((lv, t, sc, m), 0):.2f}%"
                                                         for m in ("enc", "dec", "comb", "rss")) + " |")
    L.append("")
    L.append("## Effect per type and level (threads 1, all size classes)\n")
    L.append("bytes: B/A - 1 (negative = smaller). enc, dec, comb: B's speed change (positive = faster), "
             "0 inside the noise band.\n")
    g = defaultdict(list)
    for r in recs:
        if r["threads"] == 1:
            g[(r["kind"], r["level"])].append(r)
    levels = sorted({r["level"] for r in recs}, key=LVL_ORDER.index)
    L.append("| type | files | " + " | ".join(f"-{l} bytes | -{l} enc" for l in levels) + " |")
    L.append("|---|---:|" + "---:|---:|" * len(levels))
    for kind in sorted({k for k, _ in g}):
        row = [kind, str(len({r['input'] for l in levels for r in g[(kind, l)]}))]
        for l in levels:
            rs = g[(kind, l)]
            if not rs:
                row += ["", ""]
                continue
            m = metrics(rs, band, l, 1)
            row += [pct(m["bytes"]), pct(m["enc"])]
        L.append("| " + " | ".join(row) + " |")
    L.append("")
    L.append("## Cells (threads 1)\n")
    L.append("| level | type | size | n | bytes | enc | dec | comb | rss | class | worst file | why |")
    L.append("|---|---|---|---:|---:|---:|---:|---:|---:|---|---|---|")
    for c in cells:
        if c["threads"] != 1:
            continue
        L.append(f"| {c['level']} | {c['kind']} | {c['size_class']} | {c['n']} | {pct(c['bytes'])} | "
                 f"{pct(c['enc'])} | {pct(c['dec'])} | {pct(c['comb'])} | {c['rss']:.2f} | {c['cls']} | "
                 f"{c['worst_file']} {pct(c['worst_bytes'])} | {c['why']} |")
    t1 = {(c["level"], c["kind"], c["size_class"]): c["cls"] for c in cells if c["threads"] == 1}
    diff = [c for c in cells if c["threads"] != 1 and t1.get((c["level"], c["kind"], c["size_class"])) != c["cls"]]
    L.append(f"\n## Threads 4 and 10: cells whose class differs from threads 1 ({len(diff)})\n")
    if diff:
        L.append("| level | type | size | threads | enc | dec | class (T1) | class | why |")
        L.append("|---|---|---|---:|---:|---:|---|---|---|")
        for c in diff:
            L.append(f"| {c['level']} | {c['kind']} | {c['size_class']} | {c['threads']} | {pct(c['enc'])} | "
                     f"{pct(c['dec'])} | {t1.get((c['level'], c['kind'], c['size_class']))} | {c['cls']} | "
                     f"{c['why']} |")
    L.append("\nFull table: `cells.csv` (every level, type, size class and thread count).\n")
    L.append("## Stacking (sizes, threads 1)\n")
    if not stack_tab:
        L.append("Not measured (pass --a-impact/--b-impact and --partners, or use switch mode).\n")
    else:
        L.append("alone: B/A - 1 without the partner; with: (B+partner)/(A+partner) - 1. Rows: each partner's total, "
                 "then every type that is not plain `stacks`/`none`.\n")
        L.append("| partner | type | n | alone | with | label |\n|---|---|---:|---:|---:|---|")
        for s in stack_tab:
            if s["kind"] == "(all)" or s["label"] not in ("stacks", "none"):
                L.append(f"| {s['partner']} | {s['kind']} | {s['n']} | {pct(s['alone'])} | {pct(s['with_'])} | "
                         f"{s['label']} |")
        L.append("")
    L.append("## Rules (depth-2 tree on dev, checked on holdout)\n")
    hacc = "n/a" if v["holdout_accuracy"] is None else f"{100 * v['holdout_accuracy']:.0f}%"
    L.append(f"Samples: condition rows that are not NEUTRAL; win = PASS/KEEP. Features: kind, size class, level"
             f"{', threads (all frames identical)' if any('threads' in r['rule'] for r in v['rules']) else ''}. "
             f"Holdout accuracy: {hacc}.\n")
    L.append("| rule | dev rows | dev wins | holdout rows | holdout wins | predicts |\n|---|---:|---:|---:|---:|---|")
    for r in v["rules"]:
        pred = "on" if r["dev_wins"] * 2 >= r["dev_n"] else "off"
        L.append(f"| {r['rule']} | {r['dev_n']} | {r['dev_wins']} | {r['holdout_n']} | {r['holdout_wins']} | {pred} |")
    L.append(f"\n**Suggested gate:** {v['gate']}\n")
    L.append("A gate that changes output bytes must not use thread count; it must be explained (why the change wins "
             "there) before it is implemented, and its files added to the CI ledger.\n")
    open(os.path.join(out, "effectmap.md"), "w").write("\n".join(L))
