#!/usr/bin/env python3
"""Small synthetic fixtures exercise audit rejection, not compression performance."""
import copy, json, math, pathlib, statistics, tempfile, unittest, zipfile
from unittest.mock import patch
import audit_csv as a

class MemoryZip:
    def __init__(self, data):self.data=data
    def read(self,key):return self.data[key]
def dump(x):return json.dumps(x,allow_nan=True).encode()
def fixture():
    info={"id":"TT__2000-01-01__x","family":"TT:2000-01-01","view":"csv","input_bytes":100,
          "input_sha256":"a"*64,"source_path":"data/2000/2000-01-01/x.csv"}
    i=info["id"];records=[];raw=[]
    enc={
     "pick":["build/pickcli","pick"],
     "kolmo_eco":["build/kolmo","-m","eco","-T1"],
     "kolmo_fast":["build/kolmo","-m","fast","-T1"],
     "tt_zstd12":["build/ttcli","tt","zstd","12"],
     "openzl_csv_9":["_rivals/openzl/build/cli/zli","compress","-p","csv","-l","9","-f"],
     "zstd_3":["_rivals/openzl/deps/zstd/programs/zstd","--single-thread","-3","-q","-f"]}
    for p in sorted(a.POLICIES):
        size=10 if p=="pick" else 20
        policy={"id":p}
        if p=="openzl_csv_9":
            policy.update(profile="csv",level=9,training_regime="untrained_per_source_builtin_models_allowed")
        r=dict(input_id=i,family=info["family"],view="csv",policy=policy,
          input_bytes=100,input_sha256="a"*64,status="MEASURED_DEVELOPMENT_CONTROL",
          stored_bytes=size,frame_sha256=("b" if p=="pick" else "c")*64,byte_exact_verified=True,runs=[])
        for k in range(3):
            pair={"pair_id":str(k)}
            for op in ("encode","decode"):
                prefix=enc[p] if op=="encode" else (
                  ["_rivals/openzl/build/cli/zli","decompress","-f"] if p=="openzl_csv_9"
                  else ["_rivals/openzl/deps/zstd/programs/zstd","-d","-q","-f"] if p=="zstd_3"
                  else ["build/kolmo","-d","-T1"])
                target="/inputs/"+i+".csv" if op=="encode" else "ev/first/"+i+"/"+p+"/retained.frame"
                row=dict(command=prefix+[target],returncode=0,timed_out=False,wall_s=.001*(k+1),cpu_s=.0009*(k+1),
                  peak_rss_kib=100,assigned_cpu=0,thread_constraint="single_CPU_affinity_and_thread_environment",
                  input_id=i,policy_id=p,operation=op,pair_id=str(k))
                raw.append(row)
                pair[op+"_wall_s"]=row["wall_s"];pair[op+"_cpu_s"]=row["cpu_s"]
                if k==0:r["warmup_"+op]=row
            pair["peak_memory_bytes"]=102400
            pair["speed_cost_s"],pair["archive_cost_usd"]=a.costs(size,pair["encode_wall_s"],pair["encode_cpu_s"],pair["decode_wall_s"],pair["decode_cpu_s"])
            r["runs"].append(pair)
        r["medians"]={key:statistics.median(x[key] for x in r["runs"]) for key in r["runs"][0] if key!="pair_id"}
        records.append(r)
    packet=dict(rounds=3,records=records,completed=6,failed_profiles=0)
    return info,packet,raw
def mem(packet,raw):
    return MemoryZip({"first/records.json":dump(packet),
                     "first/timings.jsonl":b"\n".join(dump(x) for x in raw)})
class CoreTests(unittest.TestCase):
    def test_charged_wall(self): self.assertEqual(a.charged(1,1.03),1)
    def test_charged_cpu(self): self.assertEqual(a.charged(1,1.031),1.031)
    def test_manual_cost(self):
        s,c=a.costs(1000000000,1,1,2,2)
        self.assertEqual(s,11)
        self.assertAlmostEqual(c,7.6*(3.89/72)/3600+.023)
    def test_numeric_rejections(self):
        for x in (-1,float("nan"),float("inf"),True,"1"):
            with self.subTest(x=x),self.assertRaises(a.EvidenceError):a.finite(x)
    def test_zero_time(self):
        with self.assertRaises(a.EvidenceError):a.charged(0,0)
    def test_size_integer_required(self):
        with self.assertRaises(a.EvidenceError):a.costs(1.2,1,1,1,1)
    def test_duplicate_json(self):
        with self.assertRaises(a.EvidenceError):a.loads('{"a":1,"a":2}')
    def test_nonfinite_json(self):
        with self.assertRaises(a.EvidenceError):a.loads('{"a":NaN}')
    def test_safe_paths(self):
        for p in ("/x","../x",r"a\b",""):
            with self.subTest(p=p),self.assertRaises(a.EvidenceError):a.safe_name(p)
    def test_cp_endpoints(self):
        self.assertEqual(a.cp_lower(0,10),0)
        self.assertAlmostEqual(a.cp_lower(10,10),.05**.1)
    def test_cp_bad_inputs(self):
        with self.assertRaises(a.EvidenceError):a.cp_lower(11,10)
    def test_group_adult(self):
        keys=[a.group_key("E27",{"source_path":"selva86/"+s})[0] for s in ("adult_train.csv","adult_test.csv")]
        self.assertEqual(*keys)
    def test_group_order_duplicates(self):
        keys=[a.group_key("E27",{"source_path":"selva86/"+s})[0] for s in ("olist_orders_45d.csv","orders_45d.csv")]
        self.assertEqual(*keys)
    def test_group_does_not_guess_income(self):
        k=a.group_key("E27",{"source_path":"selva86/income.csv"})[0]
        self.assertNotEqual(k,"E27:adult")
    def test_weekly_group(self):
        k,_=a.group_key("E26",{"source_path":"data/2000/2000-01-01/x.csv"})
        self.assertEqual(k,"TT:2000-01-01")
class PhaseTests(unittest.TestCase):
    def setUp(self):self.i,self.p,self.r=fixture()
    def check(self):return a.validate_phase(mem(self.p,self.r),"first",{self.i["id"]:self.i})
    def test_valid(self):
        records,receipt=self.check();self.assertEqual(len(records),6);self.assertEqual(receipt["raw_samples"],36)
    def test_missing_record(self):
        self.p["records"].pop()
        with self.assertRaises(a.EvidenceError):self.check()
    def test_duplicate_record(self):
        self.p["records"].append(self.p["records"][0])
        with self.assertRaises(a.EvidenceError):self.check()
    def test_missing_operation(self):
        self.r.pop()
        with self.assertRaises(a.EvidenceError):self.check()
    def test_duplicate_operation(self):
        self.r.append(self.r[0])
        with self.assertRaises(a.EvidenceError):self.check()
    def test_timeout(self):
        self.r[0]["timed_out"]=True
        with self.assertRaises(a.EvidenceError):self.check()
    def test_nonzero_exit(self):
        self.r[0]["returncode"]=1
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_cpu_boundary(self):
        self.r[0]["assigned_cpu"]=1
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_encoder(self):
        self.r[0]["command"][0]="other"
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_input_hash(self):
        self.p["records"][0]["input_sha256"]="d"*64
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_rival_profile(self):
        next(x for x in self.p["records"] if x["policy"]["id"]=="openzl_csv_9")["policy"]["level"]=6
        with self.assertRaises(a.EvidenceError):self.check()
    def test_missing_pair(self):
        self.p["records"][0]["runs"].pop()
        with self.assertRaises(a.EvidenceError):self.check()
    def test_duplicate_pair(self):
        self.p["records"][0]["runs"][1]=self.p["records"][0]["runs"][0]
        with self.assertRaises(a.EvidenceError):self.check()
    def test_raw_time_mismatch(self):
        self.r[0]["wall_s"]+=.001
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_archive_cost(self):
        self.p["records"][0]["runs"][0]["archive_cost_usd"]*=1.01
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_speed_cost(self):
        self.p["records"][0]["runs"][0]["speed_cost_s"]*=1.01
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_median(self):
        self.p["records"][0]["medians"]["archive_cost_usd"]*=1.01
        with self.assertRaises(a.EvidenceError):self.check()
    def test_wrong_rss(self):
        self.p["records"][0]["runs"][0]["peak_memory_bytes"]+=1
        with self.assertRaises(a.EvidenceError):self.check()
    def test_no_reconstruction_receipt(self):
        self.p["records"][0]["byte_exact_verified"]=False
        with self.assertRaises(a.EvidenceError):self.check()
    def test_false_completed_count(self):
        self.p["completed"]=5
        with self.assertRaises(a.EvidenceError):self.check()
    def test_order_independent_pairs(self):
        for r in self.p["records"]:r["runs"].reverse()
        self.check()
class AnalysisTests(unittest.TestCase):
    def setUp(self):
        i,p,r=fixture();self.inputs={i["id"]:i}
        self.records={ (x["input_id"],x["policy"]["id"]):x for x in p["records"] }
        self.i=i["id"]
    def test_actual_not_hindsight(self):
        # Candidate loses: actual output stays 30; oracle substitutes 20. No free fallback.
        self.records[self.i,"pick"]["stored_bytes"]=30
        result=a.analyze(self.inputs,self.records,copy.deepcopy(self.records),"E26")["policies"]["pick"]
        self.assertEqual(result["actual_bytes"],30)
        self.assertEqual(result["robust_hindsight_fallback_bytes"],20)
        self.assertEqual(result["win_both"],0)
    def test_repeat_frame_mismatch(self):
        other=copy.deepcopy(self.records);other[self.i,"pick"]["frame_sha256"]="e"*64
        with self.assertRaises(a.EvidenceError):a.analyze(self.inputs,self.records,other,"E26")
    def test_cost_fail_is_not_size_win(self):
        self.records[self.i,"pick"]["runs"][0]["archive_cost_usd"]=1
        result=a.analyze(self.inputs,self.records,copy.deepcopy(self.records),"E26")
        self.assertEqual(result["policies"]["pick"]["win_both"],0)
        self.assertEqual(result["policies"]["pick"]["size_win_only"],1)
    def test_one_pass_failure_is_failure(self):
        other=copy.deepcopy(self.records);other[self.i,"pick"]["runs"][0]["archive_cost_usd"]=1
        result=a.analyze(self.inputs,self.records,other,"E26")
        self.assertEqual(result["policies"]["pick"]["win_both"],0)
    def test_pair_ids_not_position(self):
        r=copy.deepcopy(self.records[self.i,"pick"]);o=copy.deepcopy(self.records[self.i,"openzl_csv_9"])
        o["runs"].reverse()
        self.assertTrue(a.smaller_feasible(r,o))
    def test_zip_truncation_rejected(self):
        r=copy.deepcopy(self.records[self.i,"pick"]);o=copy.deepcopy(self.records[self.i,"openzl_csv_9"])
        o["runs"].pop()
        with self.assertRaises(a.EvidenceError):a.smaller_feasible(r,o)
    def test_archive_digest_guard(self):
        with tempfile.TemporaryDirectory() as d:
            p=pathlib.Path(d)/"x.zip";p.write_bytes(b"wrong")
            with self.assertRaises(a.EvidenceError):a.audit(p,"E26")
if __name__=="__main__":unittest.main()
