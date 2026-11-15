"""Tables for benchmark/rc2/linux/README.md from the A1 and C jsonl rows, plus steal time."""
import json, os, sys
R = sys.argv[1]
def rows(p):
    if not os.path.exists(p): return []
    return [json.loads(l) for l in open(p) if l.strip() and json.loads(l).get("type") == "row"]
def last(rs, key):
    d = {}
    for r in rs: d[key(r)] = r   # later rows (re-runs) replace earlier ones
    return list(d.values())
def flags(r):
    f = []
    if r.get("threads"): f.append("threads")
    if r.get("busy"): f.append("busy")
    if "error" in r: f.append("ERROR: " + r["error"].strip().replace("\n", " ").replace("|", "/")[-120:])
    return ", ".join(f)
def steal(tag):
    try:
        a = [int(x) for x in open(f"{R}/logs/procstat_{tag}_before.txt").read().split()[1:]]
        b = [int(x) for x in open(f"{R}/logs/procstat_{tag}_after.txt").read().split()[1:]]
    except FileNotFoundError:
        return "not recorded"
    d = [y - x for x, y in zip(a, b)]
    tot = sum(d[:8])
    return f"{100*d[7]/tot:.2f}% of all CPU time ({d[7]/100:.0f} s steal over {tot/100/4:.0f} s wall x 4 CPUs)"
def vmst(name):
    p = f"{R}/logs/{name}"
    if not os.path.exists(p): return "no log"
    st = []
    for l in open(p):
        a = l.split()
        if a and a[0].isdigit() and len(a) >= 17: st.append(int(a[16]))
    if not st: return "empty"
    return f"{len(st)} samples, st mean {sum(st)/len(st):.2f}%, max {max(st)}%, samples with st>0: {sum(1 for s in st if s>0)}"
out = []
for tier, sub in (("A1", "A1/A1.jsonl"), ("A1 control library (kolmo_ctl)", "A1_ctl/A1.jsonl")):
    rs = last(rows(f"{R}/{sub}"), lambda r: (r["round"], r["entry"]))
    out.append(f"\n### Tier {tier}: silesia.tar (211,948,032 bytes), lzbench in memory, one thread\n")
    out.append("| Round | Setting | lzbench name | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | CPU s / elapsed s | Marks |")
    out.append("|---|---|---|---:|---:|---:|---:|---:|---|")
    for r in rs:
        if "error" in r:
            out.append(f"| {r['round']} | `{r['entry']}` | | | | | | {r.get('cpu_s','')} / {r.get('elapsed_s','')} | {flags(r)} |")
        else:
            out.append(f"| {r['round']} | `{r['entry']}` | {r['name']} | {r['size']:,} | {r['n']/r['size']:.3f} | {r['c_mbps']:.1f} | {r['d_mbps']:.1f} | {r['cpu_s']} / {r['elapsed_s']} | {flags(r)} |")
for g in ("fastq", "video", "research"):
    rs = last(rows(f"{R}/C/{g}.jsonl"), lambda r: (r["file"], r["label"]))
    out.append(f"\n### Tier C, {g}: whole processes, one thread, fastest of up to 3 rounds within 20 s\n")
    out.append("| File | Setting | Input (bytes) | Size (bytes) | Ratio | Compress MB/s | Decompress MB/s | Rounds | Peak RSS c / d (MiB) | Marks |")
    out.append("|---|---|---:|---:|---:|---:|---:|---:|---:|---|")
    for r in rs:
        if "error" in r:
            out.append(f"| {r['file']} | {r['label']} | {r['input']:,} | | | | | | | {flags(r)} |")
        else:
            out.append(f"| {r['file']} | {r['label']} | {r['input']:,} | {r['compressed']:,} | {r['input']/r['compressed']:.3f} | {r['c_mbs']:.2f} | {r['d_mbs']:.2f} | {r['rounds']} | {r['c_rss']/2**20:.0f} / {r['d_rss']/2**20:.0f} | {flags(r)} |")
out.append("\n### Steal time\n")
for tag, v in (("A1", "vmstat_A1.log"), ("A1ctl", "vmstat_A1_ctl.log"), ("C", "vmstat_C.log")):
    out.append(f"- {tag}: /proc/stat: {steal(tag)}; vmstat: {vmst(v)}")
print("\n".join(out))
