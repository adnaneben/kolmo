"""Offline audit regression tests. Fixtures are deliberately synthetic, not codec wins."""
import copy
import hashlib
import json
import math
import tempfile
import unittest
import zipfile
from pathlib import Path
from audit import (AuditError, LINK_BYTES_PER_SECOND, CPU_DOLLARS_PER_HOUR, READS,
                   STORAGE_DOLLARS_PER_GB_MONTH, charged, verify_cost, audit)

def run(stored=123):
    e, d = 0.01, 0.005
    return {
        "encode_wall_s": e, "encode_cpu_s": .009,
        "decode_wall_s": d, "decode_cpu_s": .004,
        "speed_cost_s": e + d + stored / LINK_BYTES_PER_SECOND,
        "archive_cost_usd": (e + READS*d)*CPU_DOLLARS_PER_HOUR/3600 + stored/1e9*STORAGE_DOLLARS_PER_GB_MONTH,
    }

def fixture(path, corrupted=False, changed_frame=False):
    def rec(policy, stored=123):
        return dict(input_id="x", family="test", view="csv", policy={"id":policy},
                    input_bytes=1000, input_sha256="a"*64, status="MEASURED_DEVELOPMENT_CONTROL",
                    stored_bytes=stored, frame_sha256="b"*64, runs=[run(stored) for _ in range(3)])
    a = [rec("zstd_3"), rec("openzl_csv_9", 111)]
    b = copy.deepcopy(a)
    if changed_frame:
        b[0]["frame_sha256"] = "c"*64
    payload = {
        "first/records.json": json.dumps({"status":"BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE","records":a}).encode(),
        "confirmation/records.json": json.dumps({"status":"BASELINES_MEASURED_NOT_POLICY_ACCEPTANCE","records":b}).encode(),
        "first/environment.json": json.dumps({"tools":{"zstd":{"sha256":"d"}}}).encode(),
        "confirmation/environment.json": json.dumps({"tools":{"zstd":{"sha256":"d"}}}).encode(),
        "confirmation.json": json.dumps({"status":"IDENTITIES_MATCH","configurations":2,
                                         "completed":2,"failed_profiles":0}).encode(),
    }
    hashes={n:hashlib.sha256(v).hexdigest() for n,v in payload.items()}
    if corrupted:
        hashes["first/records.json"]="0"*64
    payload["MANIFEST.json"]=json.dumps(hashes).encode()
    with zipfile.ZipFile(path,"w") as z:
        for n,v in payload.items():
            z.writestr(n,v)

class AuditTests(unittest.TestCase):
    def test_cpu_charge_threshold(self):
        self.assertEqual(charged(1,1.03),1)
        self.assertEqual(charged(1,1.031),1.031)
    def test_negative_rejected(self):
        with self.assertRaises(AuditError): charged(-1,0)
    def test_nan_rejected(self):
        with self.assertRaises(AuditError): charged(1,math.nan)
    def test_cost_calculation(self):
        self.assertGreater(verify_cost(run(),123)[0],0)
        self.assertGreater(verify_cost(run(),123)[1],0)
    def test_false_speed_cost_rejected(self):
        r=run(); r["speed_cost_s"] += .01
        with self.assertRaises(AuditError): verify_cost(r,123)
    def test_false_archive_cost_rejected(self):
        r=run(); r["archive_cost_usd"] *= 2
        with self.assertRaises(AuditError): verify_cost(r,123)
    def test_manifest_and_roundtrips(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"test.zip";fixture(p)
            x=audit(p,expected_sha256=None)
            self.assertEqual((x["verified_manifest_members"],x["measured"],x["timing_samples"]),(5,2,12))
            self.assertEqual(x["diagnostics"][0]["typed_archive_rival"],"openzl_csv_9")
    def test_tampered_member(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"test.zip";fixture(p,corrupted=True)
            with self.assertRaisesRegex(AuditError,"hash mismatch"):audit(p,expected_sha256=None)
    def test_nonidentical_frames(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"test.zip";fixture(p,changed_frame=True)
            with self.assertRaisesRegex(AuditError,"non-identical"):audit(p,expected_sha256=None)
    def test_zip_identity(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/"test.zip";fixture(p)
            with self.assertRaisesRegex(AuditError,"ZIP SHA256"):audit(p,expected_sha256="0"*64)

if __name__ == "__main__":
    unittest.main()
