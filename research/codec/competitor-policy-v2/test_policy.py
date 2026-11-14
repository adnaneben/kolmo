import copy
import random
import unittest
from policy import select, run_cost, CostModel, charged
from typed_columns import parse, relation_proposals, classify


def fixture(mode='balanced'):
    context={'input_sha256':'a'*64,'comparison_contract_sha256':'b'*64,'machine_id':'unit-test-machine',
             'threads':1,'input_bytes':1000000,'memory_limit_bytes':10000000}
    def entry(name, size, e, d, rival=False):
        r={**context,'id':name,'tool':'zstd' if mode in ('speed','rt','balanced','fast') else 'openzl',
           'level':3,'profile_id':'synthetic-test-profile','training_regime':'none',
           'stored_bytes':size,'peak_memory_bytes':1000,'measurement_scope':'complete_pipeline',
           'byte_exact_verified':True,'all_work_included':True,'model_cost_accounted':True,
           'typed_schema_sha256':'c'*64,'typing_precedes_planning':True,'legal_dag_verified':True,
           'source_partition':'fresh_validation','implementation_id':'synthetic-not-benchmark',
           'configuration_id':'unit-test-only','runs':[]}
        for i in range(3):
            r['runs'].append({'pair_id':str(i),'encode_wall_s':e,'encode_cpu_s':e,'decode_wall_s':d,'decode_cpu_s':d})
        return r
    return {'schema_version':2,'mode':mode,'context':context,'competitor':entry('rival',500000,.01,.005,True),
            'candidates':[entry('smaller',400000,.01,.005),entry('smallest-too-slow',300000,.2,.005),entry('larger',450000,.009,.005)]}


class PolicyTests(unittest.TestCase):
    def test_smallest_feasible(self):
        r=select(fixture()); self.assertEqual(r['selected']['id'],'smaller');self.assertEqual(r['status'],'MEASURED_SIZE_WIN')
    def test_no_minimum_percentage(self):
        f=fixture();f['candidates']=f['candidates'][:1];f['candidates'][0]['stored_bytes']=499999
        self.assertEqual(select(f)['status'],'MEASURED_SIZE_WIN')
    def test_archive_uses_existing_lifetime_model(self):
        f=fixture('hc');r=select(f);self.assertIsNotNone(r['selected']);self.assertEqual(r['cost_unit'],'USD_over_registered_lifetime')
    def test_missing_rival_is_not_a_pass(self):
        f=fixture();f.pop('competitor');self.assertEqual(select(f)['status'],'BLOCKED_MISSING_COMPETITOR')
    def test_wrong_rival(self):
        f=fixture();f['competitor']['level']=19
        with self.assertRaises(ValueError): select(f)
    def test_wrong_openzl(self):
        f=fixture('max');f['competitor']['tool']='zstd'
        with self.assertRaises(ValueError): select(f)
    def test_timing_scope_rejected(self):
        f=fixture();f['candidates'][0]['measurement_scope']='residual_kernel'
        self.assertNotEqual(select(f)['selected']['id'],'smaller')
    def test_typing_required(self):
        for key,val in [('typing_precedes_planning',False),('typed_schema_sha256',None),('legal_dag_verified',False)]:
            f=fixture();f['candidates']=f['candidates'][:1];f['candidates'][0][key]=val
            self.assertEqual(select(f)['status'],'NO_FEASIBLE_CANDIDATE')
    def test_partition_required(self):
        f=fixture();f['candidates']=f['candidates'][:1];f['candidates'][0]['source_partition']='development'
        self.assertEqual(select(f)['status'],'NO_FEASIBLE_CANDIDATE')
    def test_planning_cost_required(self):
        f=fixture();f['candidates']=f['candidates'][:1];f['candidates'][0]['all_work_included']=False
        self.assertEqual(select(f)['status'],'NO_FEASIBLE_CANDIDATE')
    def test_model_cost_required(self):
        f=fixture();f['candidates']=f['candidates'][:1];f['candidates'][0]['model_cost_accounted']=False
        self.assertEqual(select(f)['status'],'NO_FEASIBLE_CANDIDATE')
    def test_memory_budget(self):
        f=fixture();f['candidates'][0]['peak_memory_bytes']=10000001
        self.assertNotEqual(select(f)['selected']['id'],'smaller')
    def test_mismatch_input(self):
        f=fixture();f['candidates'][0]['input_sha256']='d'*64
        self.assertNotEqual(select(f)['selected']['id'],'smaller')
    def test_unmatched_pairs(self):
        f=fixture();f['candidates'][0]['runs'][0]['pair_id']='x'
        self.assertNotEqual(select(f)['selected']['id'],'smaller')
    def test_unknown_mode(self):
        f=fixture();f['mode']='invented'
        with self.assertRaises(ValueError):select(f)
    def test_cpu_charging(self):
        self.assertEqual(charged(1,1.04),1.04);self.assertEqual(charged(1,1.02),1)
    def test_single_pair_overrun(self):
        f=fixture();f['candidates'][0]['runs'][1]['encode_wall_s']=1
        self.assertNotEqual(select(f)['selected']['id'],'smaller')
    def test_nonfinite(self):
        f=fixture();f['competitor']['runs'][0]['encode_wall_s']=float('nan')
        with self.assertRaises(ValueError):select(f)
    def test_negative_size(self):
        f=fixture();f['candidates'][0]['stored_bytes']=-1
        self.assertNotEqual(select(f)['selected']['id'],'smaller')
    def test_no_feasible(self):
        f=fixture();f['candidates']=[f['candidates'][1]]
        self.assertEqual(select(f)['status'],'NO_FEASIBLE_CANDIDATE')
    def test_parity(self):
        f=fixture();c=copy.deepcopy(f['competitor']);c['id']='same';f['candidates']=[c]
        self.assertEqual(select(f)['status'],'PARITY_NOT_IMPROVEMENT')
    def test_cost_only(self):
        f=fixture();c=f['candidates'][0];c['stored_bytes']=500000
        for r in c['runs']:r['encode_wall_s']=r['encode_cpu_s']=.005
        f['candidates']=[c];self.assertEqual(select(f)['status'],'MEASURED_COST_WIN_AT_SIZE_PARITY')
    def test_archive_formula_consistency(self):
        # Algebraic equality to tradeoff_model.weigh's CPU and monthly storage difference.
        m=CostModel();r0,c0,d0,r1,c1,d1=3.1,300,1100,3.2,280,1000
        b0,b1=1e9/r0,1e9/r1
        a={'encode_wall_s':1000/c0,'encode_cpu_s':1000/c0,'decode_wall_s':1000/d0,'decode_cpu_s':1000/d0}
        b={'encode_wall_s':1000/c1,'encode_cpu_s':1000/c1,'decode_wall_s':1000/d1,'decode_cpu_s':1000/d1}
        extra=((1/c1-1/c0)+m.reads*(1/d1-1/d0))*1000/3600*m.vcpu_usd_per_hour
        saved=(1/r0-1/r1)*m.storage_usd_per_gb_month
        self.assertAlmostEqual(run_cost(b,b1,False,m)-run_cost(a,b0,False,m),extra-saved,places=14)
    def test_mode_aliases(self):
        self.assertEqual(select(fixture('speed'))['mode'],'rt');self.assertEqual(select(fixture('ratio'))['mode'],'max')
    def test_malformed_parameters(self):
        for key,value in [('reads',-1),('retention_days',0),('link_bytes_per_second',float('inf'))]:
            f=fixture();f['cost_model']={key:value}
            with self.assertRaises(ValueError):select(f)


class TypingTests(unittest.TestCase):
    def test_exact_quoted_newlines(self):
        data=b'id,note\r\n1,"x,y"\r\n2,"a\n""b"\r\n'
        t=parse(data,header=True);self.assertIsNone(t.opaque);self.assertEqual(t.restore(),data);self.assertEqual(t.columns[0]['logical'],'uint64')
    def test_spelling_preserved(self):
        data=b'1,1.0,+01.00,1e0,-0,NaN,\r\n'
        t=parse(data);self.assertEqual(t.restore(),data);self.assertEqual(t.columns[2]['logical'],'decimal_lexical');self.assertEqual(t.columns[5]['logical'],'bytes')
    def test_missing_not_null(self):
        t=parse(b'1,\n2,""\n');self.assertEqual(t.columns[1]['empty_lexemes'],2);self.assertEqual(t.columns[1]['null_semantics'],'not_inferred')
    def test_trailing_empty(self):
        t=parse(b'a,b,');self.assertEqual(len(t.columns),3);self.assertEqual(t.restore(),b'a,b,')
    def test_malformed_fallback(self):
        for b in [b'"a',b'"a"z,1',b'a"b,1',b'a,b\n1',b'a\rb']:
            t=parse(b);self.assertIsNotNone(t.opaque);self.assertEqual(t.restore(),b)
    def test_non_utf8(self):
        b=b'\xff,\x00\n';self.assertEqual(parse(b).restore(),b)
    def test_extreme_values(self):
        self.assertEqual(classify([b'18446744073709551615'])['logical'],'uint64')
        self.assertEqual(classify([b'-9223372036854775808'])['logical'],'int64')
        self.assertNotEqual(classify([b'-1',b'18446744073709551615'])['logical'],'int64')
    def test_exact_relation(self):
        t=parse(b'a,b\n1,10\n2,11\n',header=True);p=relation_proposals(t,header=True)
        self.assertEqual(p[0]['delta'],9);self.assertLess(p[0]['source'],p[0]['target'])
    def test_non_relation(self):
        self.assertEqual(relation_proposals(parse(b'1,10\n2,12\n')),[])
    def test_all_rows_validated(self):
        t=parse(b'1\n2\ntext\n');self.assertEqual(t.columns[0]['logical'],'bytes')
    def test_explicit_header(self):
        self.assertEqual(parse(b'a\n1\n',header=False).columns[0]['logical'],'bytes')
        self.assertEqual(parse(b'a\n1\n',header=True).columns[0]['logical'],'uint64')
    def test_deterministic_schema(self):
        self.assertEqual(parse(b'1\n2\n').schema_sha256(),parse(b'1\n2\n').schema_sha256())
    def test_random_bytes_always_recover(self):
        rng=random.Random(121126)
        for _ in range(500):
            b=bytes(rng.randrange(256) for _ in range(rng.randrange(100)))
            self.assertEqual(parse(b).restore(),b)
    def test_bad_delimiter(self):
        with self.assertRaises(ValueError):parse(b'a',b'::')


if __name__=='__main__':unittest.main()
