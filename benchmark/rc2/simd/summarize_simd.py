"""Tables for SIMD-ASM.md from run_simd.py output: python3 -I summarize_simd.py RESULTS.jsonl BASE_BUILD
Fastest wall time of the rounds per (input, mode, threads, build), MB/s of the input, and the ratio to BASE_BUILD.
Also: steal summary and whether every build wrote the same frame."""
import collections, json, sys
recs = [json.loads(l) for l in open(sys.argv[1])]; base = sys.argv[2]
SZ = {"silesia.tar": 211948032, "trades.csv": 53908132, "events.ndjson": 75510157, "walk_i32.npy": 67108992}
builds = list(dict.fromkeys(r["build"] for r in recs))
best = collections.defaultdict(lambda: [9e9, 9e9]); n = collections.Counter()
for r in recs:
    k = (r["input"], r["mode"], r["threads"], r["build"]); n[k] += 1
    best[k][0] = min(best[k][0], r["c"]["wall"]); best[k][1] = min(best[k][1], r["d"]["wall"])
keys = list(dict.fromkeys((r["input"], r["mode"], r["threads"]) for r in recs))
for di, dn in ((0, "compress"), (1, "decompress")):
    print(f"\n#### {dn}: MB/s (fastest of {min(n.values())}–{max(n.values())}), and time relative to `{base}`\n")
    print("| input | mode | T | " + " | ".join(builds) + " | " + " | ".join(f"{b}/{base}" for b in builds if b != base) + " |")
    print("|---|---|---:|" + "---:|" * (2 * len(builds) - 1))
    for (i, m, t) in keys:
        row = [f"{SZ.get(i, 0) / 1e6 / best[(i, m, t, b)][di]:.1f}" for b in builds]
        rel = [f"{best[(i, m, t, b)][di] / best[(i, m, t, base)][di]:.3f}" for b in builds if b != base]
        print(f"| {i} | {m} | {t} | " + " | ".join(row) + " | " + " | ".join(rel) + " |")
st = [r[x]["steal"] for r in recs for x in ("c", "d")]
print(f"\nsteal: mean {100 * sum(st) / len(st):.2f}% of CPU time, max {100 * max(st):.2f}%, runs with steal > 1%: {sum(s > 0.01 for s in st)} of {len(st)}")
fr = collections.defaultdict(dict)
for r in recs:
    if "frame_sha256" in r: fr[(r["input"], r["mode"], r["threads"])][r["build"]] = (r["frame_sha256"], r["size"], r["roundtrip"])
same = all(len({v[0] for v in d.values()}) == 1 for d in fr.values())
print(f"\nframes: {len(fr)} (input, mode, threads) cases x {len(builds)} builds; all builds byte-identical: {same}; all round trips exact: {all(v[2] for d in fr.values() for v in d.values())}")
print("\n| input | mode | T | size (bytes) | SHA-256 (first 16) | builds agreeing |\n|---|---|---:|---:|---|---:|")
for (i, m, t), d in fr.items():
    hs = collections.Counter(v[0] for v in d.values()); h = hs.most_common(1)[0][0]
    print(f"| {i} | {m} | {t} | {next(v[1] for v in d.values() if v[0] == h):,} | `{h[:16]}` | {hs[h]}/{len(d)} |")
