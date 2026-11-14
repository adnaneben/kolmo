import copy
import hashlib
import io
import json
import pathlib
import sys
import unittest
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import audit


def make_row(inp, pol, run=0, size=100, cost_wall=0.001):
    e = cost_wall
    d = cost_wall
    a, s = audit.model_cost(e, e, d, d, size)
    return dict(input_id=inp, family='F:test', view='npy',policy=dict(id=pol),
        input_bytes=10, input_sha256=audit.digest(b'0123456789'),
        status='MEASURED_DEVELOPMENT_CONTROL', stored_bytes=size,
        frame_sha256=audit.digest((inp+pol).encode()),
        byte_exact_verified=True, runs=[dict(pair_id=str(k),
            encode_wall_s=e, encode_cpu_s=e, decode_wall_s=d, decode_cpu_s=d,
            archive_cost_usd=a, speed_cost_s=s) for k in range(3)])


def fake_zip():
    inputs=[f'a{i:02d}' for i in range(61)]
    policy_ids=['zstd_3','kolmo_fast','kolmo_hc','kolmo_max','tb_best','tp_arch19','tp_pent','openzl_le-i32_9']
    members={'COMMIT.txt':(audit.EXPECTED_COMMIT+'\n').encode()}
    for inp in inputs:
        members[f'prepared/inputs/{inp}.npy']=b'0123456789'
    for trial in ('first','confirmation'):
        records=[]
        for inp in inputs:
            for p in policy_ids:
                records.append(make_row(inp,p,size=80 if p=='tp_pent' else 100))
        members[f'{trial}/records.json']=json.dumps(dict(completed=488,failed_profiles=0,records=records)).encode()
    members['confirmation.json']=json.dumps(dict(status='IDENTITIES_MATCH',completed=488,configurations=488,failed_profiles=0)).encode()
    manifest={k:audit.digest(v) for k,v in members.items() if not k.startswith('prepared/inputs/')}
    members['MANIFEST.json']=json.dumps(manifest).encode()
    bio=io.BytesIO()
    with zipfile.ZipFile(bio,'w',zipfile.ZIP_DEFLATED) as z:
        for k,v in members.items():z.writestr(k,v)
    return bio.getvalue()


class AuditTests(unittest.TestCase):
    def test_cost_cpu_rule_and_archive_formula(self):
        cost,speed=audit.model_cost(0.01,0.01031,0.02,0.0207,1000)
        self.assertAlmostEqual(speed,0.01031+0.0207+1000/125000000)
        self.assertAlmostEqual(cost,(0.01031+3.3*0.0207)*(3.89/72)/3600+1000/1e9*.023)
        _,speed2=audit.model_cost(.01,.0103,.02,.0206,1000)
        self.assertAlmostEqual(speed2,.01+.02+1000/125000000)

    def test_every_pair_must_fit_and_smaller(self):
        a=make_row('x','tp_pent',size=50)
        b=make_row('x','openzl_le-i32_9',size=100)
        self.assertTrue(audit.fits(a,b))
        b['runs'][1]['archive_cost_usd']*=.01
        self.assertFalse(audit.fits(a,b))
        b['runs'][1]['archive_cost_usd']*=100
        a['stored_bytes']=100
        self.assertFalse(audit.fits(a,b))

    def test_robust_not_hindsight_single_run(self):
        inp='x';rival=make_row(inp,'openzl_le-i32_9',size=100)
        first={p:make_row(inp,p,size=120) for p in ['kolmo_fast','kolmo_hc','kolmo_max','tb_best','tp_arch19','tp_pent','zstd_3']}
        first['tp_pent']=make_row(inp,'tp_pent',size=80)
        first[rival['policy']['id']]=rival
        second=copy.deepcopy(first)
        second['tp_pent']['runs'][0]['archive_cost_usd']*=1000
        r=audit.audit({'first':{inp:first},'confirmation':{inp:second}})
        self.assertEqual(r['per_run']['first']['kolmo_winners'],1)
        self.assertEqual(r['per_run']['confirmation']['kolmo_winners'],0)
        self.assertNotIn('oracle_feasible_and_smaller',r['robust_both_runs'])

    def test_fixture_accept_and_reject_corrupted_source(self):
        original=fake_zip()
        with zipfile.ZipFile(io.BytesIO(original)) as z:
            rows,n=audit.validate_receipt(z)
        self.assertEqual(n,4)
        self.assertEqual(len(rows['first']),61)
        self.assertEqual(audit.audit(rows)['robust_both_runs']['tp_pent'],61)
        bad=io.BytesIO()
        with zipfile.ZipFile(io.BytesIO(original)) as z,zipfile.ZipFile(bad,'w',zipfile.ZIP_DEFLATED) as out:
            for name in z.namelist():
                value=z.read(name)
                if name=='prepared/inputs/a00.npy':value=b'0000000000'
                out.writestr(name,value)
        with zipfile.ZipFile(io.BytesIO(bad.getvalue())) as z:
            with self.assertRaisesRegex(ValueError,'prepared input mismatch'):
                audit.validate_receipt(z)

    def test_fixture_reject_mismatched_run_cost(self):
        original=fake_zip();bad=io.BytesIO()
        with zipfile.ZipFile(io.BytesIO(original)) as z:
            m=json.loads(z.read('MANIFEST.json'))
            entries={k:z.read(k) for k in z.namelist() if k!='MANIFEST.json'}
        o=json.loads(entries['first/records.json']);o['records'][0]['runs'][0]['archive_cost_usd']*=1.1
        entries['first/records.json']=json.dumps(o).encode()
        m['first/records.json']=audit.digest(entries['first/records.json'])
        entries['MANIFEST.json']=json.dumps(m).encode()
        with zipfile.ZipFile(bad,'w',zipfile.ZIP_DEFLATED) as z:
            for n,v in entries.items():z.writestr(n,v)
        with zipfile.ZipFile(io.BytesIO(bad.getvalue())) as z:
            with self.assertRaisesRegex(ValueError,'archive cost mismatch'):
                audit.validate_receipt(z)

if __name__=='__main__':unittest.main()
