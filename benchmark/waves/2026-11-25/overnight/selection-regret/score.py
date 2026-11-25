"""Audit native selector measurements and derive per-input regret and Pareto frontiers."""
import argparse
from collections import Counter, defaultdict
import csv
import json
from itertools import combinations
from pathlib import Path
from statistics import median

from replay import HERE, require, sha, slices


def read(path):
    with path.open(newline="") as src:
        return list(csv.DictReader(src))


def write(path, rows):
    with path.open("w", newline="") as out:
        writer = csv.DictWriter(out, fieldnames=rows[0])
        writer.writeheader()
        writer.writerows(rows)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--data", type=Path, default=HERE)
    p.add_argument("--input-dir", type=Path)
    args = p.parse_args()
    m = json.loads((HERE / "freeze.json").read_text())
    whole = read(args.data / "whole.csv")
    probes = read(args.data / "probes.csv")
    files = m["producers"]
    eligible = m["eligible"]
    arms = ["target", *eligible, *m["other_public_presets"]]
    warmups = m["warmups"]
    rounds = range(warmups + m["scored_rounds"])
    require(set(r["file"] for r in whole) == set(files), "wrong input set")
    require(Counter((r["file"], int(r["repeat"]), r["arm"]) for r in whole)
            == Counter((f, rep, a) for f in files for rep in rounds for a in arms), "unmatched whole-file trials")
    require(Counter((r["file"], int(r["repeat"]), int(r["order"])) for r in whole)
            == Counter((f, rep, i) for f in files for rep in rounds for i in range(len(arms))), "bad trial positions")
    for r in whole + probes:
        require(all(int(r[c]) > 0 for c in ("stored_bytes", "encode_ns", "decode_ns", "encode_rss", "decode_rss"))
                and len(bytes.fromhex(r["frame_sha256"])) == 32, "invalid trial metrics")

    summaries, bounds, spans, comparisons, expected_probes = [], [], [], [], []
    for f, info in files.items():
        path = args.input_dir / f if args.input_dir else Path(info["source"])
        data = path.read_bytes()
        n = len(data)
        require(n == info["bytes"] and sha(data) == info["sha256"], "input digest mismatch: "+f)
        sampled = slices(data)
        trials = defaultdict(list)
        for r in whole:
            if r["file"] == f:
                trials[r["arm"]].append(r)
        metrics = {}
        costs = {}
        for arm, rows in trials.items():
            require(len({(r["stored_bytes"], r["frame_sha256"], r["actual_chosen"], r["predicted_bytes"])
                         for r in rows}) == 1, "unstable frame or selection")
            if arm != "target":
                require(all(not r["actual_chosen"] and not r["predicted_bytes"] for r in rows), "route on forced setting")
            scored = sorted((r for r in rows if int(r["repeat"]) >= warmups), key=lambda r: int(r["repeat"]))
            enc = [int(r["encode_ns"]) for r in scored]
            dec = [int(r["decode_ns"]) for r in scored]
            costs[arm] = [e+d for e, d in zip(enc, dec)]
            total = median(costs[arm])
            metrics[arm] = dict(file=f, arm=arm, eligible=arm in eligible, stored_bytes=int(rows[0]["stored_bytes"]),
                                frame_sha256=rows[0]["frame_sha256"], encode_ms=median(enc)/1e6,
                                decode_ms=median(dec)/1e6, combined_ms=total/1e6, encode_MB_s=n*1e3/median(enc),
                                decode_MB_s=n*1e3/median(dec), roundtrip_MB_s=n*1e3/total,
                                peak_encode_MiB=max(int(r["encode_rss"]) for r in scored)/2**20,
                                peak_decode_MiB=max(int(r["decode_rss"]) for r in scored)/2**20)
        chosen = trials["target"][0]["actual_chosen"]
        target, direct = metrics["target"], metrics[chosen]
        require(target["frame_sha256"] == direct["frame_sha256"] and target["stored_bytes"] == direct["stored_bytes"],
                "reported route is not byte-identical to its forced frame")

        sample_size = sum(b-a for a,b in sampled)
        sample_sums = Counter()
        for start,end in sampled:
            length = end-start
            digest = sha(data[start:end])
            spans.append(dict(file=f, offset=start, length=length, input_sha256=digest))
            if len(sampled) == 1:
                sample_sums.update({a:metrics[a]["stored_bytes"] for a in eligible})
                continue
            rows = [r for r in probes if r["file"]==f and int(r["offset"])==start]
            require(Counter(r["arm"] for r in rows) == Counter(list(eligible)), "missing candidate on sampled offset")
            for r in rows:
                require(int(r["length"]) == length and r["input_sha256"] == digest, "sample input digest mismatch")
                expected_probes.append((f,start,r["arm"]))
                sample_sums[r["arm"]] += int(r["stored_bytes"])
        best_est = min(sample_sums.values())
        require(sample_sums[chosen] == best_est, "selected route differs from the sampled-size optimum")
        pred = int(trials["target"][0]["predicted_bytes"])
        require(pred == int(best_est*n/sample_size), "scaled sample prediction differs from native -v report")

        for a,b in combinations(eligible,2):
            sampled_delta = sample_sums[a] - sample_sums[b]
            full_delta = metrics[a]["stored_bytes"] - metrics[b]["stored_bytes"]
            product = sampled_delta * full_delta
            if product > 0:
                comparison = "agrees"
            elif product < 0:
                comparison = "reverses"
            elif not sampled_delta and not full_delta:
                comparison = "both_tied"
            else:
                comparison = "sample_only_tie" if not sampled_delta else "whole_only_tie"
            comparisons.append(dict(file=f, arm_a=a, arm_b=b, sample_frame_bytes_a_minus_b=sampled_delta,
                                    whole_frame_bytes_a_minus_b=full_delta, relation=comparison))
        min_bytes = min(metrics[a]["stored_bytes"] for a in eligible)
        smallest = [a for a in eligible if metrics[a]["stored_bytes"] == min_bytes]
        fastest = min(eligible, key=lambda a: metrics[a]["combined_ms"])
        frontier = []
        for a in eligible:
            size, elapsed = metrics[a]["stored_bytes"], metrics[a]["combined_ms"]
            if not any(metrics[b]["stored_bytes"] <= size and metrics[b]["combined_ms"] <= elapsed and
                       (metrics[b]["stored_bytes"] < size or metrics[b]["combined_ms"] < elapsed) for b in eligible):
                frontier.append(a)
        for a in arms:
            metrics[a].update(predicted_bytes=int(sample_sums[a]*n/sample_size) if a in eligible else "",
                              eligible_pareto=(a in frontier) if a in eligible else "")
            summaries.append(metrics[a])
        matched = min((a for a in eligible if metrics[a]["stored_bytes"] <= target["stored_bytes"]),
                      key=lambda a: metrics[a]["combined_ms"])
        ratio = [t/d for t,d in zip(costs["target"], costs[chosen])]
        bounds.append(dict(file=f, input_bytes=n, sample_bytes=sample_size, chosen=chosen, native_predicted_bytes=pred,
                           actual_bytes=target["stored_bytes"], minimum_eligible_bytes=min_bytes,
                           minimum_all_tested_bytes=min(v["stored_bytes"] for v in metrics.values()),
                           smallest_eligible="|".join(smallest),
                           size_regret_B=target["stored_bytes"]-min_bytes,
                           size_regret_pct=100*(target["stored_bytes"]/min_bytes-1),
                           actual_minus_predicted_B=target["stored_bytes"]-pred,
                           target_encode_ms=target["encode_ms"], target_decode_ms=target["decode_ms"],
                           target_combined_ms=target["combined_ms"], target_roundtrip_MB_s=target["roundtrip_MB_s"],
                           chosen_direct_encode_ms=direct["encode_ms"], chosen_direct_decode_ms=direct["decode_ms"],
                           chosen_direct_combined_ms=direct["combined_ms"], chosen_direct_roundtrip_MB_s=direct["roundtrip_MB_s"],
                           paired_target_vs_forced_combined_x=median(ratio),
                           paired_target_vs_forced_faster_rounds=sum(x<1 for x in ratio),
                           fastest_eligible=fastest, fastest_eligible_bytes=metrics[fastest]["stored_bytes"],
                           fastest_eligible_combined_ms=metrics[fastest]["combined_ms"],
                           target_vs_fastest_eligible_combined_x=target["combined_ms"]/metrics[fastest]["combined_ms"],
                           fastest_at_or_below_selected_size=matched,
                           fastest_at_or_below_selected_size_ms=metrics[matched]["combined_ms"],
                           target_vs_matched_size_direct_x=target["combined_ms"]/metrics[matched]["combined_ms"],
                           eligible_pareto="|".join(frontier)))
    require(Counter((r["file"],int(r["offset"]),r["arm"]) for r in probes) == Counter(expected_probes),
            "extra or missing sampled-offset result")
    write(args.data/"settings.csv", summaries)
    write(args.data/"regret.csv", bounds)
    write(args.data/"spans.csv", spans)
    write(args.data/"rank-pairs.csv", comparisons)
    print(f"{len(whole)} verified whole-file trials ({len(files)*len(arms)*m['scored_rounds']} scored) and "
          f"{len(probes)} verified slice trials; {len(whole)+len(probes)} stock restores in replay.")
    for b in bounds:
        print(b["file"], b["chosen"], b["actual_bytes"], b["size_regret_B"],
              f"native/forced median paired {b['paired_target_vs_forced_combined_x']:.2f}x")

if __name__ == "__main__":
    main()
