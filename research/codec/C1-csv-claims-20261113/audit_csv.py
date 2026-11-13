#!/usr/bin/env python3
"""Independent C1 audit of frozen E26/E27 evidence. No codec execution or retuning."""
from __future__ import annotations
import argparse, collections, hashlib, json, math, pathlib, re, statistics, zipfile

class EvidenceError(ValueError): pass
def require(ok, message):
    if not ok: raise EvidenceError(message)
def sha(data): return hashlib.sha256(data).hexdigest()
def _pairs(pairs):
    out={}
    for k,v in pairs:
        require(k not in out, "duplicate JSON key: "+k)
        out[k]=v
    return out
def loads(data):
    return json.loads(data, object_pairs_hook=_pairs,
                      parse_constant=lambda x: (_ for _ in ()).throw(EvidenceError("nonfinite JSON")))
def finite(x, positive=False):
    require(type(x) in (int,float) and math.isfinite(x) and (x>0 if positive else x>=0),
            "invalid numeric value")
    return x
def match(a,b,message):
    finite(a); finite(b)
    require(math.isclose(a,b,rel_tol=2e-11,abs_tol=1e-15),message)
def charged(wall,cpu):
    finite(wall,True); finite(cpu)
    return cpu if cpu>1.03*wall else wall
def costs(size,ew,ec,dw,dc):
    require(type(size) is int and size>0, "invalid stored size")
    e,d=charged(ew,ec),charged(dw,dc)
    return (e+d+size/125000000,
            (e+3.3*d)*(3.89/72)/3600+size/1e9*0.023)
def safe_name(name):
    p=pathlib.PurePosixPath(name)
    require(name and not p.is_absolute() and "\\" not in name
            and all(x not in ("..",".") for x in p.parts),"unsafe member path")
    return name
PINS={
 "E26":{"archive_sha256":"606ab9dbf841f86bc9c69b1c185e898c168bf929bb351db4ffb4d5938cf1af08",
        "commit":"087052797adb2303ce896d5b8445d64120a7e98f","run":43403830702,
        "artifact":14027086734,"inputs":36,"alpha":.01},
 "E27":{"archive_sha256":"6ca1ee6612ee1f5b92a79cf189af2ce3c55c1fedc3d2f1cc3e67b29b0a81a3ef",
        "commit":"3667bf0d3135db7659c2d584263cee20ad03e413","run":43407137403,
        "artifact":14028561769,"inputs":28,"alpha":.05/6}
}
POLICIES={"pick","kolmo_eco","tt_zstd12","kolmo_fast","openzl_csv_9","zstd_3"}
SOURCE_PINS={
 "research/codec/P2-E25/pickcli.c":"5e65137662a0e28fc40781b988b990efd21fa196a41683292643fbdb454b40a4",
 "research/codec/N1-baselines/baselines.py":"7e477271e6e9e72c2a8034c62087f27d79981cef853ebe24bdb8046130017d26"
}

def group_key(cohort,info):
    path=info["source_path"]
    if cohort=="E26":
        m=re.fullmatch(r"data/\d{4}/(\d{4}-\d{2}-\d{2})/[^/]+\.csv",path)
        require(m is not None, "unexpected E26 source path")
        return "TT:"+m[1], "same weekly source directory"
    require(cohort=="E27","unknown cohort")
    stem=pathlib.PurePosixPath(path).stem.lower()
    explicit={
      "adult_train":"adult","adult_test":"adult",
      "bank_marketing_train":"bank_marketing","bank_marketing_test":"bank_marketing",
      "har_train":"har","har_validate":"har",
      "churn_modelling":"churn_modelling","churn_modelling_m":"churn_modelling",
      "norway_new_car_sales_by_model":"norway_new_car_sales",
      "norway_new_car_sales_by_model_clean":"norway_new_car_sales",
      "norway_new_car_sales_by_make":"norway_new_car_sales",
      "orders_45d":"orders_45d","olist_orders_45d":"orders_45d"
    }
    if stem in explicit:
        return "E27:"+explicit[stem], "protocol-declared related variants; conservative grouping"
    return "E27:"+path.lower(),"no declared relation; independence not established"

def cp_lower(k,n,alpha=.05):
    require(type(k) is int and type(n) is int and 0<=k<=n and 0<alpha<1,"bad binomial inputs")
    if k==0 or n==0:return 0.
    lo,hi=0.,1.
    for _ in range(80):
        p=(lo+hi)/2
        tail=sum(math.comb(n,i)*p**i*(1-p)**(n-i) for i in range(k,n+1))
        if tail<alpha:lo=p
        else:hi=p
    return (lo+hi)/2

def command_check(command, policy, operation, input_id):
    require(isinstance(command,list) and command and all(isinstance(x,str) for x in command),"bad command")
    first=command[0]
    if operation=="encode":
        expected={
         "pick":["build/pickcli","pick"],
         "kolmo_eco":["build/kolmo","-m","eco","-T1"],
         "kolmo_fast":["build/kolmo","-m","fast","-T1"],
         "tt_zstd12":["build/ttcli","tt","zstd","12"],
         "openzl_csv_9":["_rivals/openzl/build/cli/zli","compress","-p","csv","-l","9","-f"],
         "zstd_3":["_rivals/openzl/deps/zstd/programs/zstd","--single-thread","-3","-q","-f"]}
        prefix=expected[policy]
        require(command[:len(prefix)]==prefix,"unexpected encoder configuration")
        require(any(pathlib.PurePosixPath(x).name==input_id+".csv" for x in command),"wrong encode input")
    else:
        prefix=(["_rivals/openzl/build/cli/zli","decompress","-f"] if policy=="openzl_csv_9"
                else ["_rivals/openzl/deps/zstd/programs/zstd","-d","-q","-f"] if policy=="zstd_3"
                else ["build/kolmo","-d","-T1"])
        require(command[:len(prefix)]==prefix,"unexpected decoder configuration")
        require(any("/"+input_id+"/"+policy+"/retained.frame" in x for x in command),"wrong decode frame")

def validate_phase(root,phase,inputs):
    packet=loads(root.read(phase+"/records.json"))
    require(packet["rounds"]==3,"wrong repeat count")
    records={}
    for r in packet["records"]:
        key=(r["input_id"],r["policy"]["id"])
        require(key not in records,"duplicate record")
        require(key[0] in inputs and key[1] in POLICIES,"unknown input/policy")
        info=inputs[key[0]]
        require(r["input_bytes"]==info["input_bytes"] and r["input_sha256"]==info["input_sha256"],"wrong input identity")
        require(r["family"]==info["family"] and r["view"]=="csv","wrong record metadata")
        if key[1]=="openzl_csv_9":
            require(r["policy"].get("profile")=="csv" and r["policy"].get("level")==9
                    and r["policy"].get("training_regime")=="untrained_per_source_builtin_models_allowed",
                    "wrong OpenZL rival")
        records[key]=r
    require(set(records)=={(i,p) for i in inputs for p in POLICIES},"incomplete matrix")
    raw={}
    for line in root.read(phase+"/timings.jsonl").decode().splitlines():
        a=loads(line); key=(a["input_id"],a["policy_id"],a["pair_id"],a["operation"])
        require(key not in raw,"duplicate timing")
        require(key[:2] in records and key[2] in ("0","1","2") and key[3] in ("encode","decode"),"unexpected timing key")
        require(a["returncode"]==0 and a["timed_out"] is False,"failed/timed-out measured operation")
        finite(a["wall_s"],True);finite(a["cpu_s"])
        require(a["assigned_cpu"]==0 and a["thread_constraint"]=="single_CPU_affinity_and_thread_environment","thread boundary mismatch")
        command_check(a["command"],key[1],key[3],key[0])
        raw[key]=a
    matched=set();medians=0; failures=[]
    for (i,p),r in records.items():
        if "stored_bytes" not in r:
            require(r["status"]=="ERROR_NOT_A_MEASUREMENT" and not r.get("runs"),"unclassified failed record")
            w=r["warmup_encode"]
            require(w["returncode"]!=0 or w["timed_out"] is True,"failure has no failed command")
            require(not any(k[:2]==(i,p) for k in raw),"timed data attached to failed record")
            failures.append({"input_id":i,"policy":p,"returncode":w["returncode"],"timed_out":w["timed_out"]})
            continue
        require(r["status"]=="MEASURED_DEVELOPMENT_CONTROL" and r["byte_exact_verified"] is True,"no reconstruction receipt")
        require(re.fullmatch(r"[0-9a-f]{64}",r["frame_sha256"]) is not None,"invalid frame hash")
        for name,op in (("warmup_encode","encode"),("warmup_decode","decode")):
            w=r[name];require(w["returncode"]==0 and w["timed_out"] is False,"failed warmup")
            command_check(w["command"],p,op,i)
        pairs={}
        for pair in r["runs"]:
            pid=pair["pair_id"]
            require(pid in ("0","1","2") and pid not in pairs,"duplicate/bad paired round")
            pairs[pid]=pair
        require(set(pairs)=={"0","1","2"},"missing pair")
        for pid,pair in pairs.items():
            for op in ("encode","decode"):
                key=(i,p,pid,op)
                require(key in raw,"missing raw operation")
                a=raw[key]; matched.add(key)
                for metric in ("wall_s","cpu_s"):
                    match(pair[op+"_"+metric],a[metric],"raw/record timing mismatch")
            sc,ac=costs(r["stored_bytes"],pair["encode_wall_s"],pair["encode_cpu_s"],pair["decode_wall_s"],pair["decode_cpu_s"])
            match(pair["speed_cost_s"],sc,"speed cost mismatch")
            match(pair["archive_cost_usd"],ac,"archive cost mismatch")
            rss=max(raw[i,p,pid,"encode"]["peak_rss_kib"],raw[i,p,pid,"decode"]["peak_rss_kib"])*1024
            require(pair["peak_memory_bytes"]==rss,"RSS record mismatch")
        for field,value in r["medians"].items():
            match(value,statistics.median(pair[field] for pair in pairs.values()),"median mismatch")
            medians+=1
    require(set(raw)==matched,"unaccounted raw rows")
    require(packet["completed"]==len(records)-len(failures) and packet["failed_profiles"]==len(failures),"wrong completion count")
    return records,{"records":len(records),"raw_samples":len(raw),"paired_costs":len(raw)//2,"medians":medians,"failures":failures}

def smaller_feasible(r,o):
    require(len(r["runs"])==len(o["runs"])==3,"incomplete comparison")
    a={x["pair_id"]:x for x in r["runs"]};b={x["pair_id"]:x for x in o["runs"]}
    require(set(a)==set(b)=={"0","1","2"},"unpaired comparison")
    return r["stored_bytes"]<o["stored_bytes"] and all(a[k]["archive_cost_usd"]<=b[k]["archive_cost_usd"] for k in a)

def analyze(inputs,first,confirm,cohort):
    valid=[]; unsupported=[]
    for i in sorted(inputs):
        comparable=all("stored_bytes" in records[i,"openzl_csv_9"] for records in (first,confirm))
        (valid if comparable else unsupported).append(i)
    for key,a in first.items():
        b=confirm[key]
        for field in ("input_bytes","input_sha256","status","stored_bytes","frame_sha256","policy"):
            require(a.get(field)==b.get(field),"repeat identity mismatch")
    policies={};per_input={}
    for i in valid:
        per_input[i]={"source_path":inputs[i]["source_path"],"group":group_key(cohort,inputs[i])[0],
            "input_sha256":inputs[i]["input_sha256"],"input_bytes":inputs[i]["input_bytes"],"policies":{}}
        for p in ("pick","kolmo_eco","tt_zstd12","kolmo_fast"):
            a,b=first[i,p],confirm[i,p]
            require("stored_bytes" in a and "stored_bytes" in b,"candidate failure may not be excluded")
            ratios=[q["archive_cost_usd"]/o["archive_cost_usd"]
                    for records in (first,confirm)
                    for q,o in zip(sorted(records[i,p]["runs"],key=lambda x:x["pair_id"]),
                                   sorted(records[i,"openzl_csv_9"]["runs"],key=lambda x:x["pair_id"]))]
            costpass=max(ratios)<=1
            sizepass=a["stored_bytes"]<first[i,"openzl_csv_9"]["stored_bytes"]
            per_input[i]["policies"][p]={
             "stored_bytes":a["stored_bytes"],
             "openzl_bytes":first[i,"openzl_csv_9"]["stored_bytes"],
             "smaller":sizepass,"fits_all_six_pairs":costpass,
             "win_both":sizepass and costpass,
             "worst_cost_ratio":max(ratios),
             "median_cost_ratio":statistics.median(ratios),
             "size_delta_pct":100*(a["stored_bytes"]/first[i,"openzl_csv_9"]["stored_bytes"]-1)}
    ob=sum(first[i,"openzl_csv_9"]["stored_bytes"] for i in valid)
    for p in ("pick","kolmo_eco","tt_zstd12","kolmo_fast"):
        actual=sum(first[i,p]["stored_bytes"] for i in valid)
        robust=[i for i in valid if per_input[i]["policies"][p]["win_both"]]
        oracle=sum(first[i,p]["stored_bytes"] if i in robust else first[i,"openzl_csv_9"]["stored_bytes"] for i in valid)
        phase_stats={}
        for name,records in (("first",first),("confirmation",confirm)):
            winners=[i for i in valid if smaller_feasible(records[i,p],records[i,"openzl_csv_9"])]
            fallback=sum(records[i,p]["stored_bytes"] if i in winners else records[i,"openzl_csv_9"]["stored_bytes"] for i in valid)
            phase_stats[name]={"wins":len(winners),"hindsight_fallback_bytes":fallback,
              "hindsight_fallback_delta_pct":100*(fallback/ob-1)}
        policies[p]={"compared_files":len(valid),"win_both":len(robust),
            "size_win_only":sum(per_input[i]["policies"][p]["smaller"] and not per_input[i]["policies"][p]["fits_all_six_pairs"] for i in valid),
            "cost_fit_without_size_win":sum(not per_input[i]["policies"][p]["smaller"] and per_input[i]["policies"][p]["fits_all_six_pairs"] for i in valid),
            "neither":sum(not per_input[i]["policies"][p]["smaller"] and not per_input[i]["policies"][p]["fits_all_six_pairs"] for i in valid),
            "actual_bytes":actual,"openzl_bytes":ob,"actual_delta_pct":100*(actual/ob-1),
            "robust_hindsight_fallback_bytes":oracle,"robust_hindsight_fallback_delta_pct":100*(oracle/ob-1),
            "passes":phase_stats,"nonwinning_files":[i for i in valid if i not in robust]}
    groups=collections.defaultdict(list)
    for i in valid:groups[group_key(cohort,inputs[i])[0]].append(i)
    grouped={}
    for p in policies:
        all_wins=sum(all(per_input[i]["policies"][p]["win_both"] for i in ids) for ids in groups.values())
        reps=[min(ids,key=lambda i:(inputs[i]["source_path"],i)) for ids in groups.values()]
        rep_wins=sum(per_input[i]["policies"][p]["win_both"] for i in reps)
        grouped[p]={"groups":len(groups),"all_members_win_both":all_wins,"representative_win_both":rep_wins,
          "cp_sensitivity_all_members":cp_lower(all_wins,len(groups),PINS[cohort]["alpha"]),
          "cp_sensitivity_representative":cp_lower(rep_wins,len(groups),PINS[cohort]["alpha"]),
          "note":"Retrospective dependence sensitivity only; groups not proven independent or randomly sampled.",
          "representatives":reps}
    incremental={"smaller_bytes":[],"equal_bytes":[],"larger_bytes":[],"dominates_eco_all_pairs":[],"dominated_by_eco_all_pairs":[],
                 "adds_robust_wins":[],"loses_robust_wins":[]}
    for i in valid:
        p=first[i,"pick"]["stored_bytes"];e=first[i,"kolmo_eco"]["stored_bytes"]
        incremental["smaller_bytes" if p<e else "equal_bytes" if p==e else "larger_bytes"].append(i)
        pairs=[(x["archive_cost_usd"],y["archive_cost_usd"])
               for records in (first,confirm) for x,y in zip(
                   sorted(records[i,"pick"]["runs"],key=lambda q:q["pair_id"]),
                   sorted(records[i,"kolmo_eco"]["runs"],key=lambda q:q["pair_id"]))]
        if p<=e and all(a<=b for a,b in pairs) and (p<e or any(a<b for a,b in pairs)):
            incremental["dominates_eco_all_pairs"].append(i)
        if p>=e and all(a>=b for a,b in pairs) and (p>e or any(a>b for a,b in pairs)):
            incremental["dominated_by_eco_all_pairs"].append(i)
        pw=per_input[i]["policies"]["pick"]["win_both"];ew=per_input[i]["policies"]["kolmo_eco"]["win_both"]
        if pw and not ew:incremental["adds_robust_wins"].append(i)
        if ew and not pw:incremental["loses_robust_wins"].append(i)
    duplicates=collections.defaultdict(list)
    for i,v in inputs.items():duplicates[v["input_sha256"]].append(i)
    dedup=[min(v) for v in duplicates.values() if all(i in valid for i in v)]
    dedup_ob=sum(first[i,"openzl_csv_9"]["stored_bytes"] for i in dedup)
    dedup_b=sum(first[i,"pick"]["stored_bytes"] for i in dedup)
    return {"input_count":len(inputs),"comparable":len(valid),"unsupported_rival":unsupported,
      "policies":policies,"groups":dict(sorted(groups.items())),"grouped_sensitivity":grouped,
      "picker_vs_eco":incremental,
      "exact_input_duplicates":{k:v for k,v in duplicates.items() if len(v)>1},
      "unique_comparable_inputs":len(dedup),"unique_input_picker_bytes":dedup_b,
      "unique_input_openzl_bytes":dedup_ob,"unique_input_actual_delta_pct":100*(dedup_b/dedup_ob-1),
      "per_input":per_input}

def audit(path,cohort):
    pin=PINS[cohort]
    require(sha(path.read_bytes())==pin["archive_sha256"],"archive digest mismatch")
    with zipfile.ZipFile(path) as z:
        names=[safe_name(i.filename) for i in z.infolist()]
        require(len(names)==len(set(names)),"duplicate archive member")
        require(sum(i.file_size for i in z.infolist())<256*1024*1024,"archive exceeds bound")
        manifest=loads(z.read("MANIFEST.json"))
        for n,h in manifest.items():
            safe_name(n);require(sha(z.read(n))==h,"member hash mismatch: "+n)
        prepared=loads(z.read("prepared/manifest.json")); inputs={}
        for i in prepared["inputs"]:
            require(i["id"] not in inputs,"duplicate input")
            b=z.read("prepared/"+safe_name(i["file"]))
            require(len(b)==i["input_bytes"] and sha(b)==i["input_sha256"],"input byte/hash mismatch")
            inputs[i["id"]]=i
        require(len(inputs)==pin["inputs"],"changed cohort")
        expected=set(manifest)|{"MANIFEST.json"}|{"prepared/"+v["file"] for v in inputs.values()}
        require(set(names)==expected,"unlisted or missing archive member")
        require(z.read("COMMIT.txt").decode().strip()==pin["commit"],"wrong executed commit")
        sources={}
        for line in z.read("source-hashes.txt").decode().splitlines():
            h,n=line.split(None,1)
            require(n not in sources or sources[n]==h,"conflicting source hash")
            sources[n]=h
        for n,h in SOURCE_PINS.items():require(sources.get(n)==h,"wrong recorded source pin")
        first,a=validate_phase(z,"first",inputs);second,b=validate_phase(z,"confirmation",inputs)
        analysis=analyze(inputs,first,second,cohort)
        conf=loads(z.read("confirmation.json"))
        require(conf.get("status")=="IDENTITIES_MATCH","confirmation failed")
        analysis["verification"]={"status":"ARCHIVED_EVIDENCE_AUDIT_PASS","pin":pin,
          "verified_manifest_members":len(manifest),"verified_original_inputs":len(inputs),
          "first":a,"confirmation":b,
          "all_original_input_bytes":sum(x["input_bytes"] for x in inputs.values()),
          "frame_payloads_present":False,
          "new_roundtrips_executed":0,
          "boundary":"Existing complete CLI, single-CPU affinity. No timings from different runs pooled.",
          "limit":"Frame identities and successful decodes cross-checked as recorded; compressed payloads excluded from artifact."}
        analysis["input_map"]=[dict(i,group=group_key(cohort,i)[0],group_reason=group_key(cohort,i)[1])
                               for i in sorted(inputs.values(),key=lambda x:x["id"])]
        return analysis
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--e26",required=True,type=pathlib.Path);p.add_argument("--e27",required=True,type=pathlib.Path)
    p.add_argument("--output",required=True,type=pathlib.Path)
    a=p.parse_args();require(not a.output.exists(),"output exists; do not overwrite")
    out={k:audit(getattr(a,k.lower()),k) for k in ("E26","E27")}
    allhash=collections.defaultdict(list)
    for c,v in out.items():
        for i in v["input_map"]:allhash[i["input_sha256"]].append([c,i["id"]])
    out["cross_cohort_exact_duplicates"]={h:ids for h,ids in allhash.items() if len({x[0] for x in ids})>1}
    out["status"]="ARCHIVED_EVIDENCE_AUDIT_PASS_NOT_NEW_VALIDATION"
    a.output.write_text(json.dumps(out,indent=2,sort_keys=True)+"\n")
if __name__=="__main__":main()
