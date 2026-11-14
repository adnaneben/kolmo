#!/usr/bin/env python3
"""Independent offline audit of an N1 baseline artifact. No codec benchmark runs."""
import argparse
import collections
import hashlib
import json
import math
import statistics
import zipfile
from pathlib import Path

EXPECTED_ARTIFACT_SHA256 = "7f14230393e38e8ed790db70f0de4f6ad30b7e09ca708418562d0dfda440e357"
LINK_BYTES_PER_SECOND = 125_000_000
CPU_DOLLARS_PER_HOUR = 3.89 / 72
READS = 3.3
STORAGE_DOLLARS_PER_GB_MONTH = 0.023
MEASURED = "MEASURED_DEVELOPMENT_CONTROL"
ERROR = "ERROR_NOT_A_MEASUREMENT"

class AuditError(ValueError):
    pass

def sha256_stream(stream):
    h = hashlib.sha256()
    for buf in iter(lambda: stream.read(1024 * 1024), b""):
        h.update(buf)
    return h.hexdigest()

def charged(wall, cpu):
    if not all(math.isfinite(v) and v >= 0 for v in (wall, cpu)):
        raise AuditError("negative or non-finite time")
    return cpu if cpu > wall * 1.03 else wall

def verify_cost(run, stored):
    e = charged(run["encode_wall_s"], run["encode_cpu_s"])
    d = charged(run["decode_wall_s"], run["decode_cpu_s"])
    speed = e + d + stored / LINK_BYTES_PER_SECOND
    archive = (e + READS * d) * CPU_DOLLARS_PER_HOUR / 3600
    archive += stored / 1_000_000_000 * STORAGE_DOLLARS_PER_GB_MONTH
    for key, value in (("speed_cost_s", speed), ("archive_cost_usd", archive)):
        if not math.isclose(run[key], value, rel_tol=1e-8, abs_tol=1e-10):
            raise AuditError("incorrect measured cost: " + key)
    return speed, archive

def audit(path, expected_sha256=EXPECTED_ARTIFACT_SHA256):
    path = Path(path)
    with path.open("rb") as fh:
        digest = sha256_stream(fh)
    if expected_sha256 and digest != expected_sha256:
        raise AuditError("artifact ZIP SHA256 mismatch")
    with zipfile.ZipFile(path) as z:
        names = set(z.namelist())
        manifest = json.loads(z.read("MANIFEST.json"))
        if set(manifest) != names - {"MANIFEST.json"}:
            raise AuditError("manifest inventory mismatch")
        for name, declared_hash in manifest.items():
            with z.open(name) as fh:
                if sha256_stream(fh) != declared_hash:
                    raise AuditError("manifest member hash mismatch: " + name)
        stages = [json.loads(z.read(n + "/records.json")) for n in ("first", "confirmation")]
        env = [json.loads(z.read(n + "/environment.json")) for n in ("first", "confirmation")]
        confirmation = json.loads(z.read("confirmation.json"))
    if confirmation["status"] != "IDENTITIES_MATCH":
        raise AuditError("failed confirmation")
    tools = [{k: v["sha256"] for k, v in e["tools"].items()} for e in env]
    if tools[0] != tools[1]:
        raise AuditError("binary hashes changed between passes")
    matrices = []
    for stage in stages:
        if stage["status"] != "BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE":
            raise AuditError("unknown stage status")
        matrix = {}
        for record in stage["records"]:
            key = (record["input_id"], record["policy"]["id"])
            if key in matrix:
                raise AuditError("duplicate candidate")
            matrix[key] = record
        matrices.append(matrix)
    first, second = matrices
    if set(first) != set(second) or len(first) != confirmation["configurations"]:
        raise AuditError("different input/configuration matrix")
    failed = collections.Counter()
    measured = {}
    for key in sorted(first):
        a, b = first[key], second[key]
        for field in ("input_sha256", "input_bytes", "view", "status"):
            if a[field] != b[field]:
                raise AuditError("changed source/status: " + str(key))
        if a["status"] != MEASURED:
            if a["status"] != ERROR:
                raise AuditError("unknown failure status")
            failed[key[1]] += 1
            continue
        if a["stored_bytes"] != b["stored_bytes"] or a["frame_sha256"] != b["frame_sha256"]:
            raise AuditError("non-identical complete compressed frames")
        if len(a["runs"]) != 3 or len(b["runs"]) != 3:
            raise AuditError("missing timing repeats")
        costs = [verify_cost(r, a["stored_bytes"]) for r in a["runs"] + b["runs"]]
        measured[key] = dict(input_id=key[0], policy=key[1], family=a["family"],
                             view=a["view"], input_bytes=a["input_bytes"],
                             stored_bytes=a["stored_bytes"], frame_sha256=a["frame_sha256"],
                             timing_samples=len(costs),
                             speed_s=statistics.median(v[0] for v in costs),
                             archive_usd=statistics.median(v[1] for v in costs))
    if len(measured) != confirmation["completed"] or sum(failed.values()) != confirmation["failed_profiles"]:
        raise AuditError("incorrect result counts")
    if set(failed) - {"openzl_numpy_6", "openzl_numpy_9"}:
        raise AuditError("unexpected profile failures")
    diagnostics = []
    for input_id in sorted({key[0] for key in first}):
        zstd = measured[(input_id, "zstd_3")]
        row = dict(input_id=input_id, family=zstd["family"], view=zstd["view"],
                   typed_archive_rival="openzl_csv_9" if zstd["view"] == "csv" else None,
                   controls={})
        for p in ("kolmo_fast_quick", "kolmo_fast", "kolmo_hc",
                  "openzl_serial_9", "openzl_csv_9"):
            if (input_id, p) not in measured:
                continue
            t = measured[(input_id, p)]
            diag = dict(bytes_vs_zstd_pct=100*(t["stored_bytes"]/zstd["stored_bytes"]-1),
                        cost_vs_zstd_pct=100*(t["speed_s"]/zstd["speed_s"]-1),
                        speed_cost_fits=t["speed_s"] <= zstd["speed_s"],
                        smaller_than_zstd=t["stored_bytes"] < zstd["stored_bytes"])
            if row["typed_archive_rival"]:
                rival = measured[(input_id, "openzl_csv_9")]
                diag["archive_cost_vs_typed_openzl_pct"] = 100*(t["archive_usd"]/rival["archive_usd"]-1)
                diag["archive_cost_fits"] = t["archive_usd"] <= rival["archive_usd"]
                diag["smaller_than_typed_openzl"] = t["stored_bytes"] < rival["stored_bytes"]
            row["controls"][p] = diag
        diagnostics.append(row)
    return dict(status="AUDITED_DEVELOPMENT_CONTROLS_NO_P2_ACCEPTANCE",
                github_run=43299261465, artifact_sha256=digest,
                verified_manifest_members=len(manifest), configurations=len(first),
                measured=len(measured), failed_profiles=dict(sorted(failed.items())),
                timing_samples=sum(x["timing_samples"] for x in measured.values()),
                binaries=tools[0], diagnostics=diagnostics,
                limitations=["All data observed development; no fresh validation",
                             "OpenZL -p numpy is unsupported in pinned v0.3.0",
                             "OpenZL serial is generic and cannot replace typed NPY comparator",
                             "Native le-iXX profiles require an exact NPY-header/payload wrapper and its full costs",
                             "Kolmo controls are not demonstrated column-first under P2",
                             "Full CLI data cannot be pooled with in-memory research",
                             "Derived CSV and NPY inputs are correlated; no independent-family inflation"])

def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("zip", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--sha256", default=EXPECTED_ARTIFACT_SHA256)
    a = ap.parse_args()
    result = json.dumps(audit(a.zip, a.sha256), indent=2, sort_keys=True) + "\n"
    if a.output:
        a.output.write_text(result, encoding="utf-8")
    else:
        print(result, end="")

if __name__ == "__main__":
    main()
