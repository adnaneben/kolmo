#!/usr/bin/env python3
"""Offline tests; supply N1_EXACT_ARTIFACT for full evidence integration."""
import importlib.util, json, io, os, unittest, zipfile
from pathlib import Path
spec=importlib.util.spec_from_file_location("audit_exact",Path(__file__).with_name("audit_exact.py"))
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class AuditTests(unittest.TestCase):
    def test_cpu_charging(self):
        self.assertEqual(m.charge(1,1.02),1)
        self.assertEqual(m.charge(1,1.04),1.04)
    def test_bad_time(self):
        for v in (-1,float("nan"),float("inf")):
            with self.assertRaises(m.AuditError):m.charge(v,1)
    def test_cost_formula(self):
        r={"encode_wall_s":.1,"encode_cpu_s":.08,"decode_wall_s":.03,"decode_cpu_s":.04}
        speed,archive=m.costs(r,1000)
        self.assertAlmostEqual(speed,.140008)
        self.assertAlmostEqual(archive,(.1+3.3*.04)*(3.89/72)/3600+1000/1e9*.023)
    def test_real_zip(self):
        p=os.environ.get("N1_EXACT_ARTIFACT")
        if not p:self.skipTest("optional downloaded N1 ZIP")
        r=m.audit(p)
        self.assertEqual(r["manifest_verified"],1430)
        self.assertEqual(r["raw_timing_samples"],1056)
        self.assertEqual(r["identical_reported_frames"],88)
        self.assertEqual(r["status"],"AUDITED_DEVELOPMENT_NO_P2_ACCEPTANCE")
    def test_reject_wrong_zip_digest(self):
        with self.assertRaises(m.AuditError):
            m.audit(__file__)
if __name__=="__main__":unittest.main()
