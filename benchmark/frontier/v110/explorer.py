"""Render index.html from the pinned release evidence and the verified local chart measurements."""
import argparse
import csv
import gzip
import hashlib
import io
import json
from collections import defaultdict
from pathlib import Path
from statistics import geometric_mean, median


HERE = Path(__file__).resolve().parent
REV = "87dfc86b2665cac637c98179fb8afa8db3eeb668"
URL = f"https://github.com/adnaneben/kolmo/blob/{REV}/benchmark/lzbench/results/"
MAC = "mac-m5-2026-11-10-v1.0.0-v1.0.1-v1.1.0.json"
OPENZL = "panel-2026-11-10-v1.1.0-vs-openzl.csv"
FAST = "silesia-2026-11-10-v1.1.0-fast.csv"
SLOW = "silesia-2026-11-10-v1.1.0-slow.csv"
UPSTREAM_LZBENCH = "https://github.com/inikep/lzbench/blob/master/doc/lzbench24_sorted.md"
UPSTREAM_ZPAQ5_SIZE = 40_470_202
PINS = {
    MAC: "bb69dca3c11e8bef51609315343b4d215e8cd4ee9e43a88e3e122562e8068a1c",
    OPENZL: "d7e588aa1b5e38ff27ae0ce05de773ff4d8c760cd474faadf0305e5d111cbb61",
    FAST: "83d925ac4a78ebd1b077f29089a77bd278d35c4001d865780af76fe0d6fdc42f",
    SLOW: "3195577de3e755c0c22c109b0d1f522aea454513ef6bcd4c658905ce5624450b",
}
SILESIA = {
    "dickens": "English prose", "mozilla": "Mixed Mozilla software distribution",
    "mr": "Magnetic resonance image", "nci": "Chemical structures database",
    "ooffice": "Office software library", "osdb": "MySQL database",
    "reymont": "Polish book in an uncompressed PDF", "samba": "Software sources and documentation",
    "sao": "Binary star catalogue", "webster": "HTML dictionary", "xml": "XML documents",
    "x-ray": "Medical X-ray image",
}
FAMILIES = {
    "Numeric arrays": {"year.i32", "population.i64", "log_timestamps_ms.i64", "owid_f64_colmajor.u64",
                       "owid_energy_f64.npy"},
    "Tables": {"energy_test.csv", "ld30.txt", "owid-co2-data.csv", "owid_test.csv", "wide_test.csv",
               "owid_energy.pkl", "owid_energy_test.parquet", "owid_co2.parquet"},
    "Logs and JSON": {"api.json", "loghub_all.log", "owid_arr.json"},
    "Genomics": {"chr22.fa"},
    "Image data": {"img_astronaut.u8", "img_camera.u8", "lfw_subset.npy", "img_astronaut.npy",
                   "img_chelsea.u8", "img_coffee.u8"},
    "Model weights": {"magika.gguf", "magika.onnx", "magika.safetensors", "llm_bf16_10mb.safetensors",
                      "llm_bf16_47mb.safetensors", "llm_nvfp4_17mb.safetensors"},
}
LADDER = ("rt", "fast -l 1 -q", "fast -l 2 -q", "fast -q", "fast", "hc -l 9",
          "hc -l 12", "max -l 1", "eco", "hc", "max", "ultra -l 1", "ultra")


def family(name):
    if name in SILESIA:
        return "Silesia members"
    return next(k for k, names in FAMILIES.items() if name in names)


def row(label, ratio, enc, dec, **extra):
    if min(ratio, enc, dec) <= 0:
        raise ValueError(f"Nonpositive measurement for {label}")
    return dict(label=label, ratio=ratio, enc=enc, dec=dec, **extra)


def group(key, title, category, note, rows, **extra):
    if len({r["label"] for r in rows}) != len(rows):
        raise ValueError(f"Duplicate codec in {key}")
    return dict(key=key, title=title, category=category, note=note, rows=rows, **extra)


def source(key, title, method, groups, links):
    return dict(key=key, title=title, method=method, groups=groups, links=links)


def verified_text(folder, name):
    data = (folder/name).read_bytes()
    if hashlib.sha256(data).hexdigest() != PINS[name]:
        raise ValueError(f"Expected the published {REV[:7]} copy of {name}")
    return data.decode()


def means(by_file, sizes, files):
    by_codec = defaultdict(list)
    for name in files:
        for r in by_file[name]:
            by_codec[r["label"]].append((r, sizes[name]))
    summary = []
    for label, vals in by_codec.items():
        if len(vals) != len(files):
            raise ValueError(f"Missing published record for {label}")
        timed = [r for r, n in vals if n >= 1_000_000]
        summary.append(row(label, geometric_mean(r["ratio"] for r, _ in vals),
                           geometric_mean(r["enc"] for r in timed), geometric_mean(r["dec"] for r in timed)))
    return summary


def mac(folder):
    raw = json.loads(verified_text(folder, MAC))
    by_file = defaultdict(list)
    sizes = {}
    for r in raw:
        if r["ok"] is not True:
            raise ValueError(f"Unverified row: {r['file']}/{r['set']}")
        ratio = r["n"]/r["size"]
        enc = r["n"]/r["tc"]/1e6
        dec = r["n"]/r["td"]/1e6
        by_file[r["file"]].append(row(r["set"], ratio, enc, dec, size=r["size"]))
        sizes[r["file"]] = r["n"]
    if len(by_file) != 35 or any(len(rs) != 29 for rs in by_file.values()):
        raise ValueError("Incomplete published M5 panel")
    if sum(n >= 1_000_000 for n in sizes.values()) != 30:
        raise ValueError("Published M5 speed denominator changed")
    groups = [group("overall", "35-file geometric means", "Overview",
                    "Per-file geometric means give each input equal weight. Ratio uses 35 files. "
                    "Speeds use the 30 files of at least 1 MB. Twelve files are individual Silesia members.",
                    means(by_file, sizes, by_file))]
    for kind in ["Silesia members", *FAMILIES]:
        files = sorted(k for k in by_file if family(k) == kind)
        timed = sum(sizes[k] >= 1_000_000 for k in files)
        if len(files) < 2 or not timed:
            continue
        note = (f"Per-file geometric means. Ratios use all {len(files)} files. Speeds use the {timed} "
                f"of at least 1 MB. Inputs include {', '.join(files)}.")
        groups.append(group("family-"+kind.lower().replace(" ", "-"), kind+" · geometric means",
                            "File families", note, means(by_file, sizes, files)))
    for name, rows in by_file.items():
        desc = SILESIA.get(name, "")
        note = f"{desc}. " if desc else ""
        if sizes[name] < 1_000_000:
            note += "Smaller than 1 MB. Process startup can affect speeds."
        groups.append(group(name, name, family(name), note, rows, n=sizes[name]))
    return source("mac", "Published M5 · 35 files",
                  "One thread, randomized order, medians of up to five rounds. Seven v1.1.0 settings, "
                  "twelve settings from v1.0.0 and v1.0.1, and ten rival settings. "
                  "Peak memory, eco and ultra were unreported in this panel.",
                  groups, [["Published M5 measurements", URL+MAC], ["Published protocol", URL+"README.md"]])


def cloud_openzl(folder):
    grouped = defaultdict(list)
    metadata = {}
    for r in csv.DictReader(io.StringIO(verified_text(folder, OPENZL))):
        name = r["file"]
        grouped[name].append(row(r["compressor"], float(r["ratio"]), float(r["encode_mbs"]),
                                 float(r["decode_mbs"])))
        metadata[name] = f"{r['size_mb']} MB · {r['area']}"
    groups = [group(k, k, family(k), metadata[k]+(" · "+SILESIA[k] if k in SILESIA else ""), rows)
              for k, rows in grouped.items()]
    return source("cloud-zl", "Published cloud VM · 32 OpenZL files",
                  "Single-worker CLI on the same machine. Kolmo versions ran back to back. "
                  "OpenZL profiles were timed earlier that day. “OpenZL best” selects its smallest "
                  "measured profile per file. Sub-MB speeds include substantial startup time.",
                  groups, [["Published OpenZL panel", URL+OPENZL], ["Published protocol", URL+"README.md"]])


def cloud_archive(folder):
    rows = []
    for name in (FAST, SLOW):
        for r in csv.DictReader(io.StringIO(verified_text(folder, name))):
            original, size = int(r["Original size"]), int(r["Compressed size"])
            if original != 211957760:
                raise ValueError("Cloud tar size changed")
            label = r["Compressor name"]
            if label.startswith("kolmo 1.1.0 -"):
                level = int(label.rsplit("-", 1)[1])
                label = f"kolmo 1.1.0 {LADDER[level]} (level {level})"
            rows.append(row(label, original/size, float(r["Compression speed"]),
                            float(r["Decompression speed"]), size=size))
    return source("cloud-lz", "Published cloud VM · Silesia lzbench",
                  "One-thread in-memory lzbench on a two-core VM. Publisher reports 10–15% speed "
                  "variation between runs. The November 10 fast and slow tiers cover ladder levels 0–10.",
                  [group("tar", "Silesia tar", "Mixed archive", "Twelve Silesia members in a single stream.",
                         rows, n=original)],
                  [["Fast tier", URL+FAST], ["Slow tier", URL+SLOW], ["Published protocol", URL+"README.md"]])


def local_files(data, category):
    groups = []
    for name, d in data.items():
        rows = [row(r["label"], r["ratio"], r["encode_mbps"], r["decode_mbps"], size=r["frame_bytes"],
                    rss=[r["encode_rss"], r["decode_rss"]]) for r in d["codecs"].values()]
        groups.append(group(name, d["title"], category, d["description"], rows, n=d["source_bytes"],
                            gates=d.get("opt_in_gates"), sha=d["source_sha256"]))
    return groups


def fresh(path):
    measurements = json.loads(path.read_text())
    if measurements["freeze"]["source_revision"] != REV:
        raise ValueError("Unexpected source revision in local review")
    shared = ("One worker. Seven randomized timed pairs after a warm-up. Includes process startup and "
              "I/O. SHA-256 verifies every restored file. Peak RSS is the maximum by phase. ")
    links = [["Verified measurements", "measurements.json"]]
    release = source("fresh", "Local M5 · matched file-to-file review",
                     shared + "Previously studied inputs were remeasured after the builds were frozen.",
                     local_files(measurements["datasets"], "Complete files"), links)
    if not measurements["sampling"]:
        return [release]
    guard = source("guard", "PR #5 · FASTA selector", shared +
                   "The selector, unmodified v1.1.0 default and opt-in builds, Zstd and both NAF levels "
                   "were frozen before this evaluation. ce11.fa, pao1.fa and danrer25.fa are separate "
                   "from the three preservation checks on prior inputs.",
                   local_files(measurements["sampling"], "FASTA files"), links)
    return [release, guard]


EXPERIMENT_LABELS = {
    "file-planner": {"v110": "Kolmo fast · stock", "proposed": "Kolmo fast · per-member mapping",
                     "m1": "Nov 7 · M1 fast"},
    "ultra": {"stock": "Kolmo ultra · stock", "compact": "Kolmo ultra · compact candidate"},
    "electricity": {"stock": "Kolmo fast · stock", "fixed_quarter_mib": "Kolmo fast · 256 KiB preset",
                    "zstd": "Zstd −3", "openzl": "Trained OpenZL"},
}
EXPERIMENT_CASES = {
    "file-planner/archive": ("Silesia tar", "Tar member mapping", "Complete mixed Silesia archive."),
    "file-planner/structured": ("Structured tar", "Tar member mapping",
                                "Bacterial FASTA, electricity CSV and GitHub events JSONL archived together."),
    "file-planner/binary": ("Binary tar", "Tar member mapping", "SQLite, Y4M video and DOCX archived together."),
    "ultra/dickens": ("dickens · 256 KiB prefix", "Ultra memory", "English prose."),
    "ultra/osdb": ("osdb · 1 MiB prefix", "Ultra memory", "MySQL database."),
    "ultra/webster": ("webster · 1 MiB prefix", "Ultra memory", "HTML dictionary."),
    "ultra/x-ray": ("x-ray · 1 MiB prefix", "Ultra memory", "Medical X-ray image."),
    "ultra/mr": ("mr · complete scan", "Ultra memory", "Complete Silesia magnetic resonance image."),
    "electricity/eia2022": ("US electricity · 2022", "Electricity holdout",
                            "EIA-930, January–June 2022. A time split separates training and test records. "
                            "The 256 KiB preset was frozen before scoring."),
}


def packed_experiments(path):
    if not path.exists():
        return []
    pack = json.loads(gzip.decompress(path.read_bytes()))
    if pack["revision"] != REV:
        raise ValueError("Experimental receipt bundle has a different source revision")
    groups = []
    for key, (title, category, note) in EXPERIMENT_CASES.items():
        if key not in pack["cases"]:
            continue
        d = pack["cases"][key]
        names = EXPERIMENT_LABELS[key.split("/")[0]]
        if set(d["metrics"]) != set(names) or set(d["commands"]) != set(names):
            raise ValueError(f"Unexpected settings for {key}: {sorted(d['metrics'])}")
        n, rounds = d["source_bytes"], d["rounds"]
        rows = []
        frames = {}
        for codec, m in d["metrics"].items():
            raw = sorted((t for t in d["trials"] if t["codec"] == codec), key=lambda t: t["repeat"])
            if [t["repeat"] for t in raw] != list(range(rounds+1)):
                raise ValueError(f"Incomplete trial pairs for {key}/{codec}")
            if any(not t["exact"] or t["decoded_sha256"] != d["source_sha256"] or t["size"] != m["size"] for t in raw):
                raise ValueError(f"Failed verification for {key}/{codec}")
            frames[codec] = {t["frame_sha256"] for t in raw}
            for phase in ("encode", "decode"):
                ms = 1000 * median(t[phase]["seconds"] for t in raw[1:])
                peak = max(t[phase]["peak_rss_bytes"] for t in raw[1:])
                if abs(ms - m[phase+"_ms"]) > 1e-6 or peak != m[phase+"_rss"]:
                    raise ValueError(f"Inconsistent {phase} measurements for {key}/{codec}")
            rows.append(row(names[codec], n/m["size"], n/(m["encode_ms"]*1000), n/(m["decode_ms"]*1000),
                            size=m["size"], rss=[m["encode_rss"], m["decode_rss"]], repeats=rounds))
        tested, stock = ("compact", "stock") if "compact" in names else ("proposed", "v110")
        if tested in frames and len(frames[tested]) == 1 and frames[tested] == frames[stock]:
            note += " Candidate and stock produced byte-identical frames."
        item = group(key, title, category, note, rows, n=n, sha=d["source_sha256"])
        if key == "electricity/eia2022":
            item["gates_title"] = "256 KiB preset · five checks"
            item["gates"] = {}
            candidate = d["metrics"]["fixed_quarter_mib"]
            for rival in ("zstd", "openzl"):
                target = d["metrics"][rival]
                checks = {"at_least_20pct_smaller": 5*candidate["size"] <= 4*target["size"]}
                for metric in ("encode_ms", "decode_ms", "encode_rss", "decode_rss"):
                    checks[metric] = candidate[metric] <= target[metric]
                item["gates"][rival] = {"checks": checks, "pass": all(checks.values()),
                                        "bytes_saved_pct": 100*(1-candidate["size"]/target["size"])}
        groups.append(item)
    if not groups:
        return []
    return [source("experiments", "Local M5 · experimental file comparisons",
                   "One-worker file-to-file tests. Reported speeds use median wall times. Each timed pair "
                   "verifies the original SHA-256 after decoding. Memory is maximum observed encode and decode "
                   "RSS. Warm-up is excluded. The right column gives the timed pairs per setting.",
                   groups, [["Matched trials and commands", "file-trials.json.gz"], ["Replay guide", "REPRODUCE.md"]])]


def historical(path):
    data = json.loads(gzip.decompress(path.read_bytes()))
    sources = []
    cases = [
        ("old-lz", "Historical M5 · Silesia and diagnostic slices", ["archive"] +
         [g["id"] for g in data["groups"] if g["id"].startswith("slice-")],
         "November 7 research builds, one-worker in-memory lzbench. Each slice covers up to 4 MiB. "
         "Three encode and five decode iterations. Each file has its own comparison."),
        ("old-media", "Historical M5 · lossless specialists",
         ["cli-cellar", "cli-akiyo", "cli-reads", "cli-bsub", "rgb-api"],
         "November 7 research builds. Audio, video and genome comparisons use verified file-to-file "
         "runs. The RGB case passes identical pixels and dimensions to native image APIs. "
         "Each file has its own protocol."),
    ]
    for key, title, ids, method in cases:
        groups = []
        for g in data["groups"]:
            if g["id"] not in ids:
                continue
            rows = []
            for k, r in g["rows"].items():
                extras = {}
                if "encode_rss" in r and "decode_rss" in r:
                    extras["rss"] = [r["encode_rss"], r["decode_rss"]]
                if "repeats" in r:
                    extras["repeats"] = r["repeats"]
                rows.append(row(data["labels"].get(k, k), g["original"]/r["bytes"], r["encode"],
                                r["decode"], size=r["bytes"], **extras))
            category = "Mixed archive" if g["id"] == "archive" else (
                "Diagnostic slices" if g["id"].startswith("slice-") else g["method"])
            groups.append(group(g["id"], g["title"], category, g["note"], rows, n=g["original"]))
        sources.append(source(key, title, method, groups, [["Historical measurements", "../comparison.json.gz"]]))
    return sources


def local_archive(path):
    data = json.loads(gzip.decompress(path.read_bytes()))
    if data["manifest"]["source"] != REV:
        raise ValueError("Local lzbench release pin differs")
    groups = []
    for single_pass in (False, True):
        rows = []
        for key, r in data["receipts"].items():
            if ("-i1,1" in r["flags"]) != single_pass:
                continue
            if not r["valid"] or r["original"] != 211947520:
                raise ValueError(f"Invalid local lzbench result for {key}")
            if r["setting"].startswith("kolmo,"):
                label = f"kolmo 1.1.0 {r['codec']}"
            else:
                raw = next(csv.DictReader(io.StringIO(r["raw"])))
                label = raw["Compressor name"]
            rows.append(row(label, r["ratio"], r["encode"], r["decode"], size=r["bytes"]))
        if rows:
            suffix = "ultra single-pass screen" if single_pass else "repeated measurements"
            note = "One encode and one decode iteration." if single_pass else "Three encode and five decode iterations."
            groups.append(group(suffix, "Silesia tar · "+suffix, "Mixed archive", note, rows, n=211947520,
                                sha=data["source_sha256"]))
    if len(groups) == 2:
        main, screen = groups
        repeated = {r["label"] for r in main["rows"]}
        for r in screen["rows"]:
            if r["label"] not in repeated:
                main["rows"].append({**r, "label": r["label"]+" · 1 pass", "single_pass": True})
        main.update(key="all-modes", title="Silesia tar · release modes and rivals",
                    note="One archive and machine for every setting. Frame sizes are comparable. "
                         "Speeds use medians of three encode and five decode iterations, except rows "
                         "marked “1 pass.” Only repeated rows qualify for the two speed leader badges.")
        ultra = next(r for r in main["rows"] if r["label"].startswith("kolmo 1.1.0 ultra"))
        historical = json.loads(gzip.decompress((HERE.parent/"comparison.json.gz").read_bytes()))
        archive = next(g for g in historical["groups"] if g["id"] == "archive")
        if archive["original"] != main["n"] or archive["rows"]["zpaq"]["bytes"] != UPSTREAM_ZPAQ5_SIZE:
            raise ValueError("Historical local and upstream ZPAQ archive evidence differs")
        savings = 100 * (1 - ultra["size"] / UPSTREAM_ZPAQ5_SIZE)
        main["upstream_size_comparison"] = (f"Ultra produced {savings:.2f}% fewer bytes than ZPAQ −5 "
                                             f"({UPSTREAM_ZPAQ5_SIZE:,} bytes on upstream lzbench, matched locally).")
    return source("local-lz", "Local M5 · v1.1.0 Silesia lzbench",
                  "Frozen harness, unmodified v1.1.0 library, one worker. Modes map to their published "
                  "CLI defaults. Only completed release runs appear. The local tar has 211,947,520 bytes.",
                  groups, [["Local replay", "../v110-lz/replay.json.gz"], ["Local protocol", "../v110-lz/README.md"],
                           ["Upstream lzbench size leaderboard", UPSTREAM_LZBENCH]])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("published", type=Path, help="Pinned release benchmark/lzbench/results directory")
    args = parser.parse_args()
    sources = [local_archive(HERE.parent/"v110-lz/replay.json.gz"), mac(args.published),
               *fresh(HERE/"measurements.json"), *packed_experiments(HERE/"file-trials.json.gz"),
               cloud_archive(args.published), cloud_openzl(args.published)]
    sources.extend(historical(HERE.parent/"comparison.json.gz"))
    payload = json.dumps(dict(sources=sources, members=SILESIA, revision=REV), separators=(",", ":"),
                         allow_nan=False).replace("<", "\\u003c")
    template = (HERE/"explorer.template.html").read_text()
    (HERE/"index.html").write_text(template.replace("__CHART_DATA__", payload))


if __name__ == "__main__":
    main()
