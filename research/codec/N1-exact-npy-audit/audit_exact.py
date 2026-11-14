#!/usr/bin/env python3
"""Independent audit of N1 typed-NPY GitHub Actions evidence; development only."""
import argparse, ast, hashlib, json, math, os, statistics, zipfile
from pathlib import Path, PurePosixPath

ZIP_SHA="5e0f1960b5cc9a841cc6ce4d1d53b103f3a287fdb4c7cd9c4f08e071e5a7dcff"
COMMIT="2e5d51ca6d537a2a2711a33e358bdc4441a3c985"
class AuditError(ValueError): pass
def sha(b): return hashlib.sha256(b).hexdigest()
def close(a,b):
    if not math.isclose(a,b,rel_tol=1e-9,abs_tol=1e-11): raise AuditError("cost/timing disagreement")
def charge(w,c):
    if any(not math.isfinite(x) or x<0 for x in (w,c)): raise AuditError("invalid time")
    return c if c>1.03*w else w
def costs(r,n):
    e=charge(r["encode_wall_s"],r["encode_cpu_s"])
    d=charge(r["decode_wall_s"],r["decode_cpu_s"])
    return (e+d+n/125000000,(e+3.3*d)*(3.89/72)/3600+n/1e9*0.023)
def audit(path,expected_sha=ZIP_SHA):
    p=Path(path)
    digest=sha(p.read_bytes())
    if expected_sha and digest!=expected_sha: raise AuditError("artifact digest mismatch")
    with zipfile.ZipFile(p) as z:
        infos=z.infolist(); names=[v.filename for v in infos]
        if len(names)!=len(set(names)): raise AuditError("duplicate ZIP path")
        if sum(v.file_size for v in infos)>150*1024*1024: raise AuditError("archive too large")
        for v in infos:
            if (v.filename.startswith("/") or ".." in PurePosixPath(v.filename).parts
                    or "\\" in v.filename or v.file_size>40*1024*1024):
                raise AuditError("unsafe ZIP path or size")
        manifest=json.loads(z.read("MANIFEST.json"))
        unlisted=set(names)-set(manifest)-{"MANIFEST.json"}
        if len(unlisted)!=10 or any(not n.startswith("prepared/inputs/") for n in unlisted) or set(manifest)-set(names):
            raise AuditError("unexpected manifest inventory")
        for n,h in manifest.items():
            if sha(z.read(n))!=h: raise AuditError("file digest mismatch: "+n)
        if z.read("COMMIT.txt").decode().strip()!=COMMIT: raise AuditError("wrong source commit")
        prepared=json.loads(z.read("prepared/manifest.json"))
        panel={x["id"]:x for x in prepared["inputs"]}
        if len(panel)!=10: raise AuditError("wrong source panel")
        for x in panel.values():
            if sha(z.read("prepared/"+x["file"]))!=x["input_sha256"]: raise AuditError("source bytes changed")
        matrices=[]; toolhashes=[]; seedset=set(); samples=0
        for stage in ("first","confirmation"):
            data=json.loads(z.read(stage+"/records.json"))
            env=json.loads(z.read(stage+"/environment.json"))
            if (data["status"]!="BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE"
                    or data["completed"]!=88 or data["failed_profiles"]!=0 or data["rounds"]!=3):
                raise AuditError("incomplete stage")
            toolhashes.append({k:v["sha256"] for k,v in env["tools"].items()})
            seedset.add(env["seed"]); matrix={}
            raw={}
            for b in z.read(stage+"/timings.jsonl").splitlines():
                x=json.loads(b);key=(x["input_id"],x["policy_id"],x["operation"],str(x["pair_id"]))
                if key in raw or x["returncode"]!=0 or x["timed_out"]: raise AuditError("failed or duplicate raw job")
                raw[key]=x
            if len(raw)!=528: raise AuditError("missing raw timing samples")
            samples+=len(raw)
            for x in data["records"]:
                key=(x["input_id"],x["policy"]["id"])
                if key in matrix or x["input_id"] not in panel: raise AuditError("bad input/policy key")
                if x["input_sha256"]!=panel[x["input_id"]]["input_sha256"]: raise AuditError("input SHA changed")
                if x["status"]!="MEASURED_DEVELOPMENT_CONTROL" or x["byte_exact_verified"] is not True:
                    raise AuditError("unverified configuration")
                if len(x["runs"])!=3 or x["stored_bytes"]<=0 or len(x["frame_sha256"])!=64:
                    raise AuditError("missing frame or pairs")
                if key[1].startswith("openzl_exact_npy"):
                    if x["policy"]["profile"]!="header_described_exact_int_payload" or x["policy"]["level"] not in (6,7,9):
                        raise AuditError("invalid OpenZL numeric profile")
                for pair in x["runs"]:
                    e=raw[(key[0],key[1],"encode",pair["pair_id"])]
                    d=raw[(key[0],key[1],"decode",pair["pair_id"])]
                    for op,r in (("encode",e),("decode",d)):
                        close(pair[op+"_wall_s"],r["wall_s"]);close(pair[op+"_cpu_s"],r["cpu_s"])
                        if r["peak_rss_kib"]*1024>pair["peak_memory_bytes"]: raise AuditError("RSS undercount")
                    s,a=costs(pair,x["stored_bytes"])
                    close(s,pair["speed_cost_s"]);close(a,pair["archive_cost_usd"])
                matrix[key]=x
            if len(matrix)!=88: raise AuditError("missing configurations")
            matrices.append(matrix)
        confirm=ast.literal_eval(z.read("confirmation.json").decode("ascii"))
        if confirm!={"status":"IDENTITIES_MATCH","configurations":88,"completed":88,"failed_profiles":0}:
            raise AuditError("bad confirmation")
    first,second=matrices
    if set(first)!=set(second) or toolhashes[0]!=toolhashes[1] or len(seedset)!=2:
        raise AuditError("changed measurement identities")
    for key in first:
        if (first[key]["frame_sha256"]!=second[key]["frame_sha256"]
                or first[key]["stored_bytes"]!=second[key]["stored_bytes"]):
            raise AuditError("nonidentical frame")
    rows=[]
    for id,x in sorted(panel.items()):
        opts={}
        for (ident,pol),rec in first.items():
            if ident!=id: continue
            rs=rec["runs"]+second[(ident,pol)]["runs"]
            opts[pol]={"bytes":rec["stored_bytes"],
                       "archive_usd":statistics.median(v["archive_cost_usd"] for v in rs),
                       "speed_s":statistics.median(v["speed_cost_s"] for v in rs)}
        rivals=(["openzl_exact_npy_6","openzl_exact_npy_7","openzl_exact_npy_9"] if x["view"]=="npy"
                else ["openzl_csv_9"])
        # Retrospective profile-minimum is only an offline diagnostic.
        cheapest=min(rivals,key=lambda n:(opts[n]["archive_usd"],opts[n]["bytes"]))
        rows.append({"id":id,"family":x["family"],"view":x["view"],
                     "offline_low_cost_openzl":cheapest,"controls":opts})
    return {"status":"AUDITED_DEVELOPMENT_NO_P2_ACCEPTANCE","source_commit":COMMIT,
            "workflow_run":43334484927,"artifact_sha256":digest,
            "manifest_verified":len(manifest),"separately_verified_inputs":len(panel),
            "configurations_per_pass":len(first),"identical_reported_frames":len(first),
            "raw_timing_samples":samples,"rows":rows,
            "warnings":["Frames not retained; frame equality is from two matching recorded SHA256 values and runner checks.",
                        "Python wrapper/extra CLI/temp I/O imposes about 45-50ms of decode overhead on typed OpenZL NPY.",
                        "Do not claim archive acceptance against this wrapper; native fair OpenZL control needed.",
                        "Observed development NPY and CSV views are correlated; no holdout.",
                        "Kolmo baseline is not proven column-first. No trained OpenZL Pareto frontier or ML/RL."]}
if __name__=="__main__":
    ap=argparse.ArgumentParser();ap.add_argument("artifact");ap.add_argument("--output")
    a=ap.parse_args();result=json.dumps(audit(a.artifact),indent=2,sort_keys=True)+"\n"
    if a.output: Path(a.output).write_text(result)
    else: print(result,end="")
