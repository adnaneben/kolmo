#!/usr/bin/env python3
"""Build the static benchmark page (site/index.html) from site/results.json.

    python3 site/build.py                                  # re-render from the committed data
    python3 site/build.py --results WORK/results           # merge a bench/repro run into the data, then render
    python3 site/build.py --import-report EVIDENCE_DIR     # merge the 0.0.2 report's raw data (lzb-*.csv,
                                                           # mt-result.json, openzl.json as the development panel)
Options: --source "text" labels the merged sections (machine, date); --data PATH and --out PATH override the files.

No dependencies beyond Python 3. The page is one self-contained HTML file (inline SVG charts, no external
scripts), so GitHub Pages can serve site/ as is. Wins and losses are computed from the data, not written by hand:
a regenerated run regenerates every verdict on the page."""
import argparse, csv, datetime, glob, html, json, math, os, re

HERE = os.path.dirname(os.path.abspath(__file__))
E = html.escape


# ---------------------------------------------------------------- data
def family(name):
    n = name.split()[0].lower()
    return re.sub(r"\d+$", "", n) if n.startswith(("zstd", "brotli", "bsc")) else n


def parse_lzbench(csv_dir):
    res = {}
    for p in sorted(glob.glob(os.path.join(csv_dir, "lzb-*.csv"))):
        tag = os.path.basename(p)[4:-4]
        with open(p) as f:
            for r in csv.DictReader(f):
                if not r.get("Compressor name") or r["Compressor name"].startswith("memcpy"):
                    continue
                corpus = os.path.basename(r["Filename"]) if tag != "silesia-files" else "silesia/" + os.path.basename(r["Filename"])
                res.setdefault(corpus, []).append(dict(
                    codec=r["Compressor name"], family=family(r["Compressor name"]), list=tag.split("-")[0],
                    enc_mbs=float(r["Compression speed"]), dec_mbs=float(r["Decompression speed"]),
                    bytes=int(r["Original size"]), size=int(r["Compressed size"]),
                    ratio=int(r["Original size"]) / int(r["Compressed size"])))
    return res


def load(path):
    return json.load(open(path)) if os.path.exists(path) else None


def merge_results(data, rdir, source):
    tools = load(os.path.join(rdir, "tools.json"))
    m = (tools or {}).get("machine", {})
    src = source or f"{m.get('cpu', '?')}, {m.get('os', '')}, run {datetime.date.today().isoformat()}"
    if tools:
        data["tools"] = tools
    lz = load(os.path.join(rdir, "lzbench", "lzbench.json"))
    if lz:
        data["lzbench"] = dict(source=src, corpora=lz["corpora"])
    for key, fn in (("mt", "mt.json"), ("dfloat11", "dfloat11.json")):
        v = load(os.path.join(rdir, fn))
        if v:
            data[key] = dict(source=src, data=v)
    p = load(os.path.join(rdir, "panel.json"))
    if p:
        data["panel"] = dict(source=src, training=p.pop("_training", {}), files=p)
    f = load(os.path.join(rdir, "FETCHED.json"))
    if f:
        data["fetched"] = f


def import_report(data, ev, source):
    src = source or "0.0.2 market report, Apple M5, 2026-11-23 (kolmo v0.0.2)"
    d = os.path.join(ev, "mkt-0.0.2") if os.path.isdir(os.path.join(ev, "mkt-0.0.2")) else ev
    lz = parse_lzbench(d)
    if lz:
        data["lzbench"] = dict(source=src, corpora=lz)
    mt = load(os.path.join(d, "mt-result.json"))
    if mt:
        data["mt"] = dict(source=src, data=mt)
    oz = load(os.path.join(d, "openzl.json"))
    if oz:
        data["devpanel"] = dict(source=src + "; files overlap Kolmo's development data", files=oz)


# ---------------------------------------------------------------- analysis
def dominated_by(row, rows):
    """Rivals at least as good on ratio, compress and decode speed, and better on one."""
    out = []
    for r in rows:
        if r["family"] == "kolmo":
            continue
        ge = r["ratio"] >= row["ratio"] and r["enc_mbs"] >= row["enc_mbs"] and r["dec_mbs"] >= row["dec_mbs"]
        gt = r["ratio"] > row["ratio"] or r["enc_mbs"] > row["enc_mbs"] or r["dec_mbs"] > row["dec_mbs"]
        if ge and gt:
            out.append(r)
    return out


def frontier(rows, ykey):
    """Pareto frontier of ratio (higher) and ykey (higher), sorted by ratio."""
    pts = sorted(rows, key=lambda r: (-r["ratio"], -r[ykey]))
    out, best = [], -1
    for r in pts:
        if r[ykey] > best:
            out.append(r); best = r[ykey]
    return sorted(out, key=lambda r: r["ratio"])


def klevel(codec):
    if codec.endswith(" -U"):
        return "-U"
    m = re.search(r"-(\d+)$", codec)
    return ("-U" if m and m.group(1) == "10" else "-" + m.group(1)) if m else codec


def kname(codec):
    return "kolmo " + klevel(codec) if codec.startswith("kolmo") else codec


# ---------------------------------------------------------------- rendering helpers
def fmt(x, d=None):
    if x is None:
        return "–"
    if d is None:
        d = 0 if x >= 100 else (1 if x >= 10 else 2)
    return f"{x:,.{d}f}"


def scatter(rows, ykey, title, ylabel, highlight=("kolmo",), second=("openzl",), w=640, h=360):
    """Ratio (x, linear) against speed (y, log). Kolmo in series-1, OpenZL in series-2, the rest neutral."""
    pl, pr, pt, pb = 56, 30, 14, 40
    xs = [r["ratio"] for r in rows]; ys = [max(r[ykey], 0.1) for r in rows]
    x0, x1 = min(xs), max(xs); pad = (x1 - x0) * 0.06 or 0.1; x0 -= pad; x1 += pad
    y0, y1 = math.floor(math.log10(min(ys))), math.ceil(math.log10(max(ys)))
    if y1 == y0:
        y1 += 1
    X = lambda v: pl + (v - x0) / (x1 - x0) * (w - pl - pr)
    Y = lambda v: pt + (y1 - math.log10(max(v, 0.1))) / (y1 - y0) * (h - pt - pb)
    o = [f'<svg viewBox="0 0 {w} {h}" class="chart" role="img" aria-label="{E(title)}">']
    for e in range(y0, y1 + 1):
        y = Y(10 ** e)
        o.append(f'<line x1="{pl}" x2="{w-pr}" y1="{y:.1f}" y2="{y:.1f}" class="grid"/>'
                 f'<text x="{pl-6}" y="{y+4:.1f}" class="tick" text-anchor="end">{fmt(10**e, 0) if e >= 0 else 10**e}</text>')
    step = 10 ** math.floor(math.log10((x1 - x0) / 5 or 1)); step *= 2 if (x1 - x0) / step > 10 else 1
    v = math.ceil(x0 / step) * step
    while v <= x1:
        o.append(f'<text x="{X(v):.1f}" y="{h-pb+16}" class="tick" text-anchor="middle">{v:g}</text>'); v += step
    o.append(f'<text x="{(pl+w-pr)/2}" y="{h-6}" class="axis" text-anchor="middle">compression ratio (higher is better) →</text>'
             f'<text transform="translate(14,{(pt+h-pb)/2}) rotate(-90)" class="axis" text-anchor="middle">{E(ylabel)} ↑ (log)</text>')
    fr = frontier(rows, ykey)
    if len(fr) > 1:
        d = " ".join(f"{'M' if i == 0 else 'L'}{X(r['ratio']):.1f},{Y(r[ykey]):.1f}" for i, r in enumerate(fr))
        o.append(f'<path d="{d}" class="front"/>')
    def cls(r):
        return "k" if r["family"] in highlight else ("z" if r["family"] in second else "o")
    for r in sorted(rows, key=lambda r: {"o": 0, "z": 1, "k": 2}[cls(r)]):
        c = cls(r); rad = 5 if c != "o" else 4
        tip = f"{kname(r['codec'])}: ratio {r['ratio']:.3f}, compress {fmt(r['enc_mbs'])}, decode {fmt(r['dec_mbs'])} MB/s"
        o.append(f'<circle cx="{X(r["ratio"]):.1f}" cy="{Y(r[ykey]):.1f}" r="{rad}" class="pt {c}" data-tip="{E(tip)}"><title>{E(tip)}</title></circle>')
        if c == "k":
            o.append(f'<text x="{X(r["ratio"])+7:.1f}" y="{Y(r[ykey])-6:.1f}" class="lab">{E(klevel(r["codec"]))}</text>')
    o.append("</svg>")
    return "".join(o)


def legend(second_label=None):
    s = '<div class="legend"><span><i class="sw k"></i>Kolmo (label = level)</span>'
    if second_label:
        s += f'<span><i class="sw z"></i>{E(second_label)}</span>'
    return s + '<span><i class="sw o"></i>other codecs</span><span><i class="ln"></i>Pareto frontier (all codecs)</span></div>'


# ---------------------------------------------------------------- sections
def sec_lzbench(lz):
    out = []; corpora = lz["corpora"]
    names = {"silesia.tar": "Silesia (mixed files, 212 MB)", "enwik8": "enwik8 (English Wikipedia text, 100 MB)",
             "canterbury.tar": "Canterbury (small files, 2.8 MB)"}
    for c in [c for c in ("silesia.tar", "enwik8", "canterbury.tar") if c in corpora]:
        rows = corpora[c]
        wins, losses = [], []
        reported = set()
        for k in [r for r in rows if r["family"] == "kolmo"]:
            dom = dominated_by(k, rows)
            if dom:
                best = max(dom, key=lambda r: r["ratio"]); reported.add(k["codec"])
                losses.append(f"<b>{E(kname(k['codec']))}</b> ({k['ratio']:.3f}, {fmt(k['enc_mbs'])}/{fmt(k['dec_mbs'])} MB/s) is beaten on ratio "
                              f"<i>and</i> both speeds by {', '.join(E(r['codec']) for r in sorted(dom, key=lambda r: -r['ratio'])[:3])}"
                              f" (e.g. {E(best['codec'])}: {best['ratio']:.3f}, {fmt(best['enc_mbs'])}/{fmt(best['dec_mbs'])})")
                continue
            rd = [r for r in rows if r["family"] != "kolmo" and r["ratio"] >= k["ratio"] and r["dec_mbs"] > k["dec_mbs"] * 1.05]
            if rd:
                b = max(rd, key=lambda r: r["enc_mbs"]); reported.add(k["codec"])
                losses.append(f"<b>{E(kname(k['codec']))}</b> ({k['ratio']:.3f}, decode {fmt(k['dec_mbs'])}): {E(b['codec'])} is at least as small and decodes faster "
                              f"({b['ratio']:.3f}, decode {fmt(b['dec_mbs'])}), but compresses at {fmt(b['enc_mbs'])} MB/s against Kolmo’s {fmt(k['enc_mbs'])}")
        fd = frontier(rows, "dec_mbs"); fe = frontier(rows, "enc_mbs")
        kd = [r for r in fd if r["family"] == "kolmo"]; ke = [r for r in fe if r["family"] == "kolmo"]
        top = max(rows, key=lambda r: r["ratio"])
        if kd:
            wins.append("On the ratio–decode frontier: " + ", ".join(E(kname(r["codec"])) for r in kd))
        else:
            losses.append("No Kolmo level is on the ratio–decode-speed frontier")
        if ke:
            wins.append("On the ratio–compress frontier: " + ", ".join(E(kname(r["codec"])) for r in ke))
        if top["family"] == "kolmo":
            wins.append(f"Best ratio of all {len(rows)} settings: {E(kname(top['codec']))} ({top['ratio']:.3f})")
        else:
            losses.append(f"Best ratio is {E(top['codec'])} ({top['ratio']:.3f}), not Kolmo")
        # decode speed vs zstd at a similar ratio (within 3%), for levels not already listed
        for k in [r for r in rows if r["family"] == "kolmo" and r["codec"] not in reported]:
            near = [r for r in rows if r["family"] == "zstd" and abs(r["ratio"] / k["ratio"] - 1) < 0.03]
            if near:
                n = max(near, key=lambda r: r["dec_mbs"])
                if n["dec_mbs"] > k["dec_mbs"] * 1.1:
                    losses.append(f"{E(kname(k['codec']))} decodes at {fmt(k['dec_mbs'])} MB/s; {E(n['codec'])} reaches a similar ratio "
                                  f"({n['ratio']:.3f} vs {k['ratio']:.3f}) and decodes at {fmt(n['dec_mbs'])}")
        tbl = ['<table class="data"><thead><tr><th>codec</th><th>ratio</th><th>compress MB/s</th><th>decode MB/s</th><th>verdict</th></tr></thead><tbody>']
        for r in sorted(rows, key=lambda r: -r["ratio"]):
            dom = dominated_by(r, rows) if r["family"] == "kolmo" else []
            onf = r in fd or r in fe
            v = ("dominated by " + E(dom[0]["codec"])) if dom else ("frontier" if onf else "")
            tbl.append(f'<tr class="{"kr" if r["family"] == "kolmo" else ""}{" loss" if dom else ""}"><td>{E(kname(r["codec"]))}</td>'
                       f'<td>{r["ratio"]:.3f}</td><td>{fmt(r["enc_mbs"])}</td><td>{fmt(r["dec_mbs"])}</td><td>{v}</td></tr>')
        tbl.append("</tbody></table>")
        out.append(f'<section class="corpus"><h3>{E(names.get(c, c))}</h3>{verdicts(wins, losses)}'
                   f'<div class="charts"><figure>{scatter(rows, "dec_mbs", c + " ratio vs decode", "decode MB/s")}<figcaption>Ratio against <b>decode</b> speed</figcaption></figure>'
                   f'<figure>{scatter(rows, "enc_mbs", c + " ratio vs compress", "compress MB/s")}<figcaption>Ratio against <b>compress</b> speed</figcaption></figure></div>'
                   f'{legend("OpenZL generic (lzbench built-in, 0.2.3)")}<details><summary>All {len(rows)} settings</summary>{"".join(tbl)}</details></section>')
    per = {c[8:]: rs for c, rs in corpora.items() if c.startswith("silesia/")}
    if per:
        t = ['<table class="data"><thead><tr><th>Silesia file</th><th>kolmo -3</th><th>zstd -3</th><th>gain</th><th>kolmo -9</th><th>xz -9</th><th>best other</th></tr></thead><tbody>']
        for f, rs in sorted(per.items()):
            k3 = next((r for r in rs if r["family"] == "kolmo" and r["codec"].endswith("-3")), None)
            k9 = next((r for r in rs if r["family"] == "kolmo" and r["codec"].endswith("-9")), None)
            z3 = next((r for r in rs if r["family"] == "zstd" and r["codec"].endswith("-3")), None)
            x9 = next((r for r in rs if r["family"] == "xz"), None)
            bo = max((r for r in rs if r["family"] != "kolmo"), key=lambda r: r["ratio"])
            gain = k3["ratio"] / z3["ratio"] - 1 if k3 and z3 else None
            r3 = lambda r: "%.3f" % r["ratio"] if r else "–"
            gs = "" if gain is None else "%+.0f%%" % (gain * 100)
            t.append(f'<tr class="{"loss" if gain is not None and gain < 0 else ""}"><td>{E(f)}</td><td>{r3(k3)}</td><td>{r3(z3)}</td>'
                     f'<td>{gs}</td><td>{r3(k9)}</td><td>{r3(x9)}</td><td>{E(bo["codec"])} {bo["ratio"]:.3f}</td></tr>')
        t.append("</tbody></table>")
        out.append(f'<section><h3>Silesia file by file: where the gain comes from</h3><p>Kolmo’s gain over zstd at the same level is concentrated in files with structure (images, tables, records). On plain text it is a few percent; rows where Kolmo is <i>larger</i> are marked.</p>{"".join(t)}</section>')
    return "".join(out)


def verdicts(wins, losses):
    s = '<div class="verdicts"><div class="col win"><h4>Wins</h4><ul>' + "".join(f"<li>{w}</li>" for w in wins) + "</ul></div>"
    s += '<div class="col lose"><h4>Losses</h4><ul>' + ("".join(f"<li>{l}</li>" for l in losses) or "<li>none found</li>") + "</ul></div></div>"
    return s


def best(rows, prefix):
    ok = [r for r in rows if "size" in r and r["tool"].startswith(prefix)]
    return max(ok, key=lambda r: r["ratio"]) if ok else None


def verdict_cell(k, z):
    if not k or not z:
        return "", "–"
    q = k["ratio"] / z["ratio"] - 1
    if abs(q) < 0.01:
        return "tie", f"tie ({q:+.1%})"
    return ("win", f"Kolmo {q:+.0%}") if q > 0 else ("loss", f"Kolmo {q:+.0%}")


def sec_panel(p, title_note=""):
    files = p["files"]; sets = {}
    for f, v in files.items():
        if f.startswith("_") or f.startswith("weights/"):
            continue
        sets.setdefault(v.get("set", v.get("area")), []).append((f, v))
    has_t = any(r["tool"].startswith("openzl trained") for _, vs in sets.items() for _, v in vs for r in v["rows"])
    pairs = [("Kolmo best vs OpenZL untrained best", "kb", "zu"), ("Kolmo -3 vs OpenZL untrained best", "k3", "zu")]
    if has_t:
        pairs += [("Kolmo best vs OpenZL trained", "kb", "zt"), ("Kolmo -3 vs OpenZL trained", "k3", "zt")]
    score = {lab: [0, 0, 0] for lab, _, _ in pairs}
    head = ['file', 'MB', 'zstd -19', 'xz -9', 'OpenZL<br>untrained best'] + (['OpenZL<br>trained'] if has_t else []) + \
           ['Kolmo -3', 'Kolmo -5', 'Kolmo best', 'Kolmo best vs<br>OpenZL best'] + (['Kolmo -3 vs<br>OpenZL trained'] if has_t else ['Kolmo -3 vs<br>OpenZL best'])
    ncol = len(head)
    t = ['<table class="data panel"><thead><tr>' + "".join(f"<th>{h}</th>" for h in head) + '</tr></thead><tbody>']
    for s_, items in sets.items():
        label = items[0][1]["area"]
        t.append(f'<tr class="grp"><td colspan="{ncol}">{E(label)}{" — holdout, new to Kolmo" if "holdout/" in items[0][0] else ""}</td></tr>')
        for f, v in sorted(items):
            rows = v["rows"]
            g = lambda n: next((r for r in rows if r["tool"] == n and "size" in r), None)
            zu, zt = best(rows, "openzl -l"), best(rows, "openzl trained")
            ob = max([x for x in (zu, zt) if x], key=lambda r: r["ratio"], default=None)
            sel = dict(kb=best(rows, "kolmo"), k3=g("kolmo -3"), zu=zu, zt=zt)
            kb, k3, k5 = sel["kb"], sel["k3"], g("kolmo -5")
            c1, t1 = verdict_cell(kb, ob); c2, t2 = verdict_cell(k3, zt if has_t else ob)
            for lab, x, y in pairs:
                k, z = sel[x], sel[y]
                if k and z:
                    q = k["ratio"] / z["ratio"] - 1
                    score[lab][0 if q >= 0.01 else (1 if q > -0.01 else 2)] += 1
            cell = lambda r: f'{r["ratio"]:.3f}' if r else "–"
            zerr = next((r for r in rows if r["tool"].startswith("openzl trained") and "error" in r), None)
            ztc = cell(zt) if zt else ('<span class="muted">' + E(zerr["error"]) + "</span>" if zerr else "–")
            prof = lambda r: f'<span class="muted"> {E(r["tool"].split("[")[-1].rstrip("]"))}</span>' if r else ""
            t.append(f'<tr><td>{E(os.path.basename(f))}</td><td>{v["bytes"]/1e6:.1f}</td><td>{cell(g("zstd -19"))}</td><td>{cell(g("xz -9"))}</td>'
                     f'<td>{cell(zu)}{prof(zu)}</td>' + (f'<td>{ztc}</td>' if has_t else '') + f'<td>{cell(k3)}</td><td>{cell(k5)}</td>'
                     f'<td>{cell(kb)}<span class="muted"> {E(kb["tool"][6:]) if kb else ""}</span></td><td class="{c1}">{t1}</td><td class="{c2}">{t2}</td></tr>')
    t.append("</tbody></table>")
    lines = "".join(f"<li>{E(lab)}: <b>{w} wins, {ti} ties, {l} losses</b></li>" for lab, (w, ti, l) in score.items() if w + ti + l)
    summary = f'<div class="score"><ul>{lines}</ul><span class="muted">Ties are within ±1% in ratio.</span></div>' if lines else ""
    return summary + "".join(t), score


def rematch_chart(p):
    """One chart per holdout set: each tool's mean ratio and decode speed over the test files."""
    out = []
    groups = {}
    for f, v in p["files"].items():
        if "holdout/" in f:
            groups.setdefault(v["set"], []).append(v)
    for s, vs in groups.items():
        agg = {}
        for v in vs:
            for r in v["rows"]:
                if "size" not in r:
                    continue
                a = agg.setdefault(r["tool"], [0, 0, 0.0])
                a[0] += v["bytes"]; a[1] += r["size"]; a[2] += v["bytes"] / (r["dec_mibs"] * 2**20)
        rows = []
        for tool, (n, s_, dt) in agg.items():
            fam = "kolmo" if tool.startswith("kolmo") else ("openzl" if tool.startswith("openzl") else "other")
            rows.append(dict(codec=tool, family=fam, ratio=n / s_, dec_mbs=n / dt / 1e6, enc_mbs=0))
        out.append(f'<figure>{scatter(rows, "dec_mbs", s, "decode MB/s", second=("openzl",))}<figcaption>{E(vs[0]["area"])}: '
                   f'all test files together (total size / total compressed size)</figcaption></figure>')
    return '<div class="charts">' + "".join(out) + "</div>" + legend("OpenZL (untrained profiles and trained)") if out else ""


def sec_weights(p, df):
    files = {f: v for f, v in p["files"].items() if f.startswith("weights/")}
    if not files:
        return '<p class="muted">Not measured in this run.</p>'
    dfm = {d["file"]: d for d in (df or {}).get("data", [])}
    t = ['<table class="data"><thead><tr><th>model file</th><th>MB</th><th>zstd -19</th><th>xz -9</th><th>OpenZL safetensors</th><th>ZipNN</th>'
         '<th>DFloat11<br>(method, computed)</th><th>Kolmo -3</th><th>Kolmo best</th><th>ZipNN decode MB/s</th><th>Kolmo -3 decode MB/s</th></tr></thead><tbody>']
    for f, v in sorted(files.items()):
        rows = v["rows"]; g = lambda n: next((r for r in rows if r["tool"] == n and "size" in r), None)
        zn, oz, kb, k3 = best(rows, "zipnn"), best(rows, "openzl"), best(rows, "kolmo"), g("kolmo -3")
        d = dfm.get(os.path.basename(f), {}).get("ratio")
        others = [x["ratio"] for x in (zn, oz, g("zstd -19"), g("xz -9")) if x] + ([d] if d else [])
        cls = "win" if kb and others and kb["ratio"] > max(others) * 1.01 else ("loss" if kb and others and kb["ratio"] < max(others) * 0.99 else "tie")
        c = lambda r: f'{r["ratio"]:.3f}' if r else "–"
        t.append(f'<tr><td>{E(os.path.basename(f))}</td><td>{v["bytes"]/1e6:.0f}</td><td>{c(g("zstd -19"))}</td><td>{c(g("xz -9"))}</td><td>{c(oz)}</td>'
                 f'<td>{c(zn)}</td><td>{f"{d:.3f}" if d else "n/a (not bf16)"}</td><td>{c(k3)}</td><td class="{cls}">{c(kb)} <span class="muted">{E(kb["tool"][6:]) if kb else ""}</span></td>'
                 f'<td>{fmt(zn["dec_mibs"]*1.048576) if zn else "–"}</td><td>{fmt(k3["dec_mibs"]*1.048576) if k3 else "–"}</td></tr>')
    t.append("</tbody></table>")
    return "".join(t)


def sec_mt(mt):
    r = mt["data"]["results"]
    t = ['<table class="data"><thead><tr><th>tool</th><th>ratio</th><th>compress MiB/s</th><th>decode MiB/s</th><th>memory compress / decode (MB)</th></tr></thead><tbody>']
    for n, v in r.items():
        t.append(f'<tr class="{"kr" if n.startswith("kolmo") else ""}"><td>{E(n)}</td><td>{v["ratio"]:.3f}</td><td>{fmt(v["enc_mibs"])}</td><td>{fmt(v["dec_mibs"])}</td>'
                 f'<td>{v["enc_rss_mb"]:.0f} / {v["dec_rss_mb"]:.0f}</td></tr>')
    t.append("</tbody></table>")
    k3, z3 = r.get("kolmo -3 -T1"), r.get("zstd -3 -T1"); k9, x9 = r.get("kolmo -9 -T1"), r.get("xz -9 -T1")
    k30, z30 = r.get("kolmo -3 -T0"), r.get("zstd -3 -T0")
    wins, losses = [], []
    if k30 and z30:
        (wins if k30["dec_mibs"] > z30["dec_mibs"] else losses).append(
            f"All cores, level 3: Kolmo decodes at {fmt(k30['dec_mibs'])} MiB/s, zstd -3 at {fmt(z30['dec_mibs'])} (zstd’s decoder is single-threaded)")
        (wins if k30["enc_mibs"] > z30["enc_mibs"] else losses).append(
            f"All cores, level 3: Kolmo compresses at {fmt(k30['enc_mibs'])} MiB/s, zstd -3 at {fmt(z30['enc_mibs'])}")
    if k3 and z3 and k3["enc_rss_mb"] > z3["enc_rss_mb"] * 1.2:
        losses.append(f"Memory: Kolmo -3 uses {k3['enc_rss_mb']:.0f} MB to compress, zstd -3 {z3['enc_rss_mb']:.0f} MB")
    if k9 and x9:
        (losses if k9["enc_rss_mb"] > x9["enc_rss_mb"] else wins).append(
            f"Memory: Kolmo -9 uses {k9['enc_rss_mb']:.0f} MB to compress, xz -9 {x9['enc_rss_mb']:.0f} MB")
    return verdicts(wins, losses) + "".join(t)


# ---------------------------------------------------------------- page
CSS = """
:root{color-scheme:light;--bg:#fcfcfb;--panel:#f4f3f0;--ink:#0b0b0b;--ink2:#52514e;--muted:#7a7974;--rule:#e2e0da;
--k:#2a78d6;--z:#eb6834;--o:#a9a7a0;--win:#0b7a3e;--winbg:#e6f4ec;--loss:#b3261e;--lossbg:#fbeaea;--front:#7a7974}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){color-scheme:dark;--bg:#1a1a19;--panel:#232322;--ink:#f0efec;
--ink2:#c3c2b7;--muted:#9a998f;--rule:#34332f;--k:#3987e5;--z:#d95926;--o:#6b6a63;--win:#5cc98a;--winbg:#16301f;--loss:#f08a84;--lossbg:#3a1d1b;--front:#9a998f}}
:root[data-theme="dark"]{color-scheme:dark;--bg:#1a1a19;--panel:#232322;--ink:#f0efec;--ink2:#c3c2b7;--muted:#9a998f;--rule:#34332f;
--k:#3987e5;--z:#d95926;--o:#6b6a63;--win:#5cc98a;--winbg:#16301f;--loss:#f08a84;--lossbg:#3a1d1b;--front:#9a998f}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.55 system-ui,-apple-system,Segoe UI,sans-serif}
main{max-width:1120px;margin:0 auto;padding:24px 16px 64px}h1{font-size:28px;margin:8px 0 4px}h2{font-size:21px;margin:40px 0 8px;border-top:1px solid var(--rule);padding-top:24px}
h3{font-size:17px;margin:24px 0 8px}h4{margin:0 0 4px;font-size:14px;text-transform:uppercase;letter-spacing:.04em}
p,li{color:var(--ink2)}code,pre{font:13px ui-monospace,SFMono-Regular,Menlo,monospace;background:var(--panel);border-radius:4px}code{padding:1px 4px}
pre{padding:12px;overflow-x:auto}a{color:var(--k)}.muted{color:var(--muted);font-size:12px}
.lede{font-size:17px;color:var(--ink2);max-width:780px}.meta{color:var(--muted);font-size:13px}
.verdicts{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin:8px 0 12px}.col{border-radius:8px;padding:10px 14px}
.col ul{margin:0;padding-left:18px}.col li{font-size:14px;margin:3px 0}.win{background:var(--winbg)}.win h4{color:var(--win)}
.lose{background:var(--lossbg)}.lose h4{color:var(--loss)}
.charts{display:grid;grid-template-columns:repeat(auto-fit,minmax(320px,1fr));gap:12px}figure{margin:0;background:var(--panel);border-radius:8px;padding:8px}
figcaption{font-size:13px;color:var(--ink2);padding:2px 6px}.chart{width:100%;height:auto;display:block}
.grid{stroke:var(--rule);stroke-width:1}.tick{fill:var(--muted);font-size:11px}.axis{fill:var(--ink2);font-size:12px}
.front{fill:none;stroke:var(--front);stroke-width:1.5;stroke-dasharray:4 3}.pt{stroke:var(--panel);stroke-width:2}.pt.k{fill:var(--k)}.pt.z{fill:var(--z)}.pt.o{fill:var(--o)}
.pt:hover{stroke:var(--ink)}.lab{fill:var(--ink);font-size:11px;font-weight:600}
.legend{display:flex;flex-wrap:wrap;gap:16px;font-size:13px;color:var(--ink2);margin:6px 0}.sw{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:6px;vertical-align:middle}
.sw.k{background:var(--k)}.sw.z{background:var(--z)}.sw.o{background:var(--o)}.ln{display:inline-block;width:18px;border-top:2px dashed var(--front);margin-right:6px;vertical-align:middle}
.tablewrap{overflow-x:auto}table.data{border-collapse:collapse;width:100%;font-size:13px;margin:8px 0;font-variant-numeric:tabular-nums}
table.data th,table.data td{padding:5px 8px;border-bottom:1px solid var(--rule);text-align:right;white-space:nowrap}
table.data th:first-child,table.data td:first-child{text-align:left}table.data th{color:var(--ink2);font-weight:600;vertical-align:bottom}
tr.kr td:first-child{color:var(--k);font-weight:600}tr.loss td{background:var(--lossbg)}td.win{background:var(--winbg);color:var(--win);font-weight:600}
td.loss{background:var(--lossbg);color:var(--loss);font-weight:600}td.tie{color:var(--ink2)}tr.grp td{background:var(--panel);font-weight:600;text-align:left;color:var(--ink)}
.score{background:var(--panel);padding:10px 14px;border-radius:8px}details summary{cursor:pointer;color:var(--k);font-size:14px;margin:6px 0}
.src{font-size:12px;color:var(--muted)}#tip{position:fixed;pointer-events:none;background:var(--ink);color:var(--bg);font-size:12px;padding:4px 8px;border-radius:4px;display:none;max-width:320px;z-index:9}
@media (max-width:640px){.verdicts{grid-template-columns:1fr}h1{font-size:23px}}
"""

JS = """<div id="tip"></div><script>
(function(){var t=document.getElementById('tip');document.querySelectorAll('[data-tip]').forEach(function(e){
var tt=e.querySelector('title');if(tt)tt.remove();
e.addEventListener('mouseenter',function(){t.textContent=e.getAttribute('data-tip');t.style.display='block'});
e.addEventListener('mousemove',function(v){t.style.left=Math.min(v.clientX+12,innerWidth-330)+'px';t.style.top=(v.clientY+12)+'px'});
e.addEventListener('mouseleave',function(){t.style.display='none'})});
document.querySelectorAll('table.data').forEach(function(x){if(!x.parentNode.classList.contains('tablewrap')){var w=document.createElement('div');w.className='tablewrap';x.parentNode.insertBefore(w,x);w.appendChild(x)}})})();
</script>"""


def render(data):
    tl = data.get("tools", {}); m = tl.get("machine", {})
    kv = tl.get("kolmo", {}).get("version", "0.0.2")
    parts = [f"""<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Kolmo benchmarks</title><meta name="description" content="Kolmo against zstd, xz, OpenZL (trained and untrained), ZipNN and others: wins and losses, with one-command reproduction.">
<style>{CSS}</style></head><body><main>
<h1>Kolmo benchmarks</h1>
<p class="lede">Kolmo {E(kv)} against the standard codecs, against OpenZL with and without training, and against ML-weight compressors. Every number on this page comes from
<code>bench/repro</code> and can be reproduced with one command. <b>Losses are listed next to wins</b>, and every verdict is computed from the data, not written by hand.</p>
<p class="meta">Built {datetime.date.today().isoformat()}. Machine: {E(m.get('cpu', 'Apple M5'))} ({E(m.get('arch', 'arm64'))}, {E(str(m.get('cores', '10')))} cores), {E(m.get('os', 'macOS'))}.
One thread per tool unless a row says otherwise. x86-64 results come from the <code>bench-x86</code> workflow.</p>
<nav class="meta"><a href="#std">Standard corpora</a> · <a href="#openzl">OpenZL rematch</a> · <a href="#weights">ML weights</a> · <a href="#mt">Multi-thread</a> · <a href="#dev">Development panel</a> · <a href="#method">Method and caveats</a> · <a href="#repro">Reproduce</a></nav>
"""]
    if "lzbench" in data:
        parts.append(f'<h2 id="std">Standard corpora (lzbench, one thread)</h2><p class="src">Source: {E(data["lzbench"]["source"])}. Speeds in MB/s (10<sup>6</sup> B/s) as lzbench reports them; Kolmo level 10 is <code>-U</code>.</p>'
                     '<p>Kolmo is plotted in blue; the dashed line is the Pareto frontier over all codecs (nothing is better on both axes). A Kolmo point below the line is beaten by something on that trade-off.</p>')
        parts.append(sec_lzbench(data["lzbench"]))
    if "panel" in data:
        tbl, score = sec_panel(data["panel"])
        tr = data["panel"].get("training", {})
        trl = "".join(f"<li><code>{E(k)}</code>: {v['seconds']:.0f} s, {'ok' if v.get('ok') else 'failed'}"
                      f"{', compressor ' + fmt(v['compressor_bytes']/1024, 1) + ' KiB' if v.get('compressor_bytes') else ''}</li>" for k, v in tr.items())
        parts.append(f"""<h2 id="openzl">Structured data: Kolmo (zero configuration) vs OpenZL, untrained and trained</h2>
<p class="src">Source: {E(data["panel"]["source"])}.</p>
<p>OpenZL is Meta’s format-aware framework; its best results come from training (<code>zli train</code>) on samples of the data. Here OpenZL gets
its matching profile at levels 6 and 9 (<i>untrained</i>), and a compressor <b>trained on a separate training split</b> of the same kind of data (<i>trained</i>);
the training files are never measured. Kolmo gets no profile, no schema and no training. The holdout sets are public data new to Kolmo’s development.</p>
{rematch_chart(data["panel"])}{tbl}
<details><summary>OpenZL training runs</summary><ul>{trl or "<li>none</li>"}</ul></details>""")
        parts.append(f'<h2 id="weights">ML weights</h2><p>Model files compress poorly with every general tool: the gain comes from the exponent bits. '
                     f'ZipNN and DFloat11 are built for exactly this. DFloat11’s decoder needs an NVIDIA GPU; its column is the size its published method gives, computed on the CPU '
                     f'(<code>bench/repro/dfloat11_tool.py</code>), not a run of DFloat11 itself. ZipNN times are in-process (Python start-up excluded).</p>'
                     + sec_weights(data["panel"], data.get("dfloat11")))
    if "mt" in data:
        parts.append(f'<h2 id="mt">Command line, one thread and all cores (Silesia)</h2><p class="src">Source: {E(data["mt"]["source"])}. Best of {data["mt"]["data"].get("n", 3)}; MiB/s; peak memory from the OS.</p>'
                     + sec_mt(data["mt"]))
    if "devpanel" in data:
        tbl, _ = sec_panel(data["devpanel"])
        parts.append(f'<h2 id="dev">Development panel (not independent)</h2><p class="src">Source: {E(data["devpanel"]["source"])}.</p>'
                     '<p><b>Read with care:</b> these 20 files come from the data used while developing Kolmo, so some tuning bias is possible, and OpenZL ran untrained. '
                     'They are kept for continuity with the 0.0.2 report; the holdout sets above are the fair comparison.</p>' + tbl)
    tools = data.get("tools", {})
    cli = tools.get("cli", {})
    clis = ", ".join(f"{k} {v.get('version')}" + ("" if v.get("matches") else f" (pin {v.get('pinned')})") for k, v in cli.items())
    parts.append(f"""<h2 id="method">Method</h2><ul>
<li><b>One thread for every tool</b> (lzbench’s <code>NO_THREADING</code>, <code>-T1</code>/<code>-j1</code> on the command line), except the rows marked <code>-T0</code> or <code>-jN</code> in the multi-thread table.</li>
<li><b>Same compiler flags.</b> Kolmo is built with <code>make ARCH=</code> (generic <code>-O3</code>, no <code>-mcpu</code>), the flags of its release binaries; lzbench builds every codec, Kolmo included, with its own flags. OpenZL is a CMake Release build.</li>
<li><b>Pinned versions:</b> lzbench {E(tools.get('lzbench', {}).get('commit', '9be0e25')[:10])}, OpenZL {E(tools.get('openzl', {}).get('commit', '50f8789a')[:10])} (zli 0.3.0), Kolmo {E(str(tools.get('kolmo', {}).get('commit', 'v0.0.2'))[:12])}{', CLIs ' + E(clis) if clis else ''}.</li>
<li><b>Verified:</b> every round trip is compared byte for byte; a failure is shown as an error, never dropped.</li>
<li><b>Best of 3</b> for command-line runs (one run above 60 MB or for the slowest settings); lzbench: best of 3 × 3 s for fast codecs, one run for slow ones.</li>
<li><b>Serialised:</b> each timed workload holds a machine-wide lock, so no two benchmarks run at once.</li>
<li><b>Data:</b> every input is fetched from its source and checked by sha256 (<code>bench/repro/datasets.json</code>, <code>bench/lzbench/corpora.py</code>); nothing is redistributed.</li></ul>
<h2>Caveats</h2><ul>
<li><b>One machine per page build.</b> These numbers are from the machine above. x86-64 numbers come from a separate workflow run.</li>
<li><b>OpenZL training</b> used <code>zli train</code>’s default (greedy) trainer with a 20-minute cap per profile, on four training files per set. More training data or time may help OpenZL further.</li>
<li><b>Holdout data is new to Kolmo</b>, but Silesia, enwik8 and Canterbury were seen in earlier tuning, and the development panel overlaps the development data.</li>
<li><b>DFloat11 was not run</b> (it needs CUDA); its column is computed from its method. cmix, paq8px and nncp were not run.</li>
<li><b>Levels are being tuned</b> before launch; numbers will be regenerated with the same command.</li></ul>
<h2 id="repro">Reproduce</h2>
<pre>git clone https://github.com/adnaneben/kolmo && cd kolmo
python3 -m venv .venv && .venv/bin/pip install -r bench/repro/requirements.txt
PYTHON=.venv/bin/python sh bench/repro/run.sh ../kolmo-bench          # everything, ~3 h on an M5
PYTHON=.venv/bin/python sh bench/repro/run.sh ../kolmo-bench --quick  # smoke test, minutes</pre>
<p>The run writes JSON to <code>../kolmo-bench/results/</code> and regenerates this page (<code>site/build.py</code>). See <code>bench/repro/README.md</code>.</p>
</main>{JS}</body></html>""")
    return "".join(parts)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results"); ap.add_argument("--import-report"); ap.add_argument("--source")
    ap.add_argument("--data", default=os.path.join(HERE, "results.json")); ap.add_argument("--out", default=os.path.join(HERE, "index.html"))
    a = ap.parse_args()
    data = load(a.data) or {}
    if a.import_report:
        import_report(data, a.import_report, a.source)
    if a.results:
        merge_results(data, a.results, a.source)
    if a.import_report or a.results:
        os.makedirs(os.path.dirname(a.data), exist_ok=True)
        json.dump(data, open(a.data, "w"), indent=1, sort_keys=True)
    open(a.out, "w").write(render(data))
    print(f"wrote {a.out} from {a.data} (sections: {', '.join(k for k in data if k not in ('tools', 'fetched'))})")


if __name__ == "__main__":
    main()
