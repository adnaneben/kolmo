"""Synthetic UNIT fixtures only; not compression measurements."""
import copy, io, json, tempfile, unittest
from pathlib import Path
from unittest.mock import patch
import validate as v

class Parsing(unittest.TestCase):
    def source(self, **kw):
        return dict(fields=['a','b'],limit=None,delimiter=',',**kw)
    def test_exact_and_missing(self):
        vals,ix,miss,n=v.parse_csv(io.StringIO('a,b\n1.0,?\n,2\n3,4\n'),self.source())
        self.assertEqual(list(vals['a']),[1,3]);self.assertEqual(list(ix['a']),[0,2]);self.assertEqual(miss,{'a':1,'b':1});self.assertEqual(n,3)
    def test_no_imputation_or_rounding(self):
        for s in ('1.5','NaN','Infinity','-1','4294967296','NA'):
            with self.subTest(s=s),self.assertRaises(ValueError):v.parse_csv(io.StringIO('a,b\n'+s+',1\n'),self.source())
    def test_maximum_uint32(self):
        vals,_,_,_=v.parse_csv(io.StringIO('a,b\n4294967295,0\n'),self.source());self.assertEqual(vals['a'][0],4294967295)
    def test_source_rows_before_missing(self):
        src=self.source();src['limit']=2
        vals,ix,_,_=v.parse_csv(io.StringIO('a,b\n?,1\n2,2\n999,3\n'),src)
        self.assertEqual(list(vals['a']),[2]);self.assertEqual(list(ix['a']),[1])
    def test_short_source_rejected(self):
        src=self.source();src['limit']=3
        with self.assertRaises(ValueError):v.parse_csv(io.StringIO('a,b\n1,2\n'),src)
    def test_missing_column(self):
        with self.assertRaises(ValueError):v.parse_csv(io.StringIO('a,c\n1,2\n'),self.source())
    def test_duplicate_column(self):
        with self.assertRaises(ValueError):v.parse_csv(io.StringIO('a,a,b\n1,2,3\n'),self.source())
    def test_ragged(self):
        for s in ('a,b\n1\n','a,b\n1,2,3\n'):
            with self.assertRaises(ValueError):v.parse_csv(io.StringIO(s),self.source())
    def test_npy_roundtrip(self):
        data,head=v.npy_bytes([0,4294967295,17]);a=v.np.load(io.BytesIO(data),allow_pickle=False)
        self.assertEqual(a.tolist(),[0,4294967295,17]);self.assertEqual(a.dtype.str,'<u4');self.assertEqual(len(data)-head,12)
    def test_registered_source_shape(self):
        self.assertEqual(sum(len(s['fields']) for s in v.SOURCES),11);self.assertEqual(len({s['family'] for s in v.SOURCES}),4)

class Phenotypes(unittest.TestCase):
    def test_sparse(self):
        x=[1]*10000;x[::250]=[1<<20]*40;p=v.phenotype_values(x);self.assertTrue(p['H1'])
    def test_dense_is_not_sparse(self):
        x=[1,1<<20]*5000;self.assertFalse(v.phenotype_values(x)['H1'])
    def test_missing_phenotype(self):
        p=v.phenotype_values([0]*4096);self.assertFalse(p['H1']);self.assertFalse(p['H2']);self.assertFalse(p['H4'])
    def test_changing_width(self):
        self.assertTrue(v.phenotype_values([7]*4096+[65535]*4096)['H2'])
    def test_width_constraint(self):
        self.assertTrue(v.phenotype_values([511]*5000)['H4']);self.assertFalse(v.phenotype_values([255]*5000)['H4'])
    def test_empty(self):self.assertEqual(v.phenotype_values([])['count'],0)
    def test_plane_decode(self):
        self.assertEqual(v.phenotype(bytes([1,0,0,1]),dict(count=2,order=0,output_width=2,flags=8))['max_bits'],9)

class Confirmation(unittest.TestCase):
    def fixture(self, folder):
        folder.mkdir();rows=[];ph={}
        for i in range(11):
            for size,n in [('full',16384),('small128',128),('small1024',1024)]:
                name=f'stream{i}_{size}';ph[name]=dict(H1=False,H2=False,H4=False)
                for method in v.METHODS.values():
                    for backend in v.CODERS.values():
                        rows.append(dict(name=name,family=f'family{i%4}',size_class=size,count=n,bytes=n*4,sha256='unit-fixture',method=method,backend=backend,stored_bytes=1000,frame_sha256='unit-fixture',requested_codec=0,actual_codec=0,encode_ns=100,decode_ns=100,post_exec_image_peak_rss_kib=4000))
        (folder/'measurements.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows));v.save(folder/'phenotypes.json',ph);v.save(folder/'capture_hashes.json',{});v.save(folder/'product_controls.json',[])
    def test_complete_confirmation_and_absent_hypothesis(self):
        with tempfile.TemporaryDirectory() as d:
            d=Path(d);self.fixture(d/'a');self.fixture(d/'b')
            with patch('builtins.print'):v.analyze(d/'a',d/'b',d/'result.json')
            r=json.loads((d/'result.json').read_text());self.assertEqual(r['confirmed_configurations'],660)
            self.assertTrue(all(x['verdict']=='NOT_EXERCISED' for x in r['decisions'] if x['hypothesis']!='H3'))
    def test_changed_frame_is_not_confirmation(self):
        with tempfile.TemporaryDirectory() as d:
            d=Path(d);self.fixture(d/'a');self.fixture(d/'b');p=d/'b/measurements.jsonl';rows=v.load_rows(d/'b');rows[0]['frame_sha256']='changed';p.write_text(''.join(json.dumps(r)+'\n' for r in rows))
            with self.assertRaises(ValueError):v.analyze(d/'a',d/'b',d/'r.json')
    def test_duplicate_or_missing_case_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            d=Path(d);self.fixture(d/'a');self.fixture(d/'b');p=d/'b/measurements.jsonl';lines=p.read_text().splitlines();p.write_text('\n'.join(lines[:-1]))
            with self.assertRaises(ValueError):v.analyze(d/'a',d/'b',d/'r.json')
if __name__=='__main__':unittest.main(verbosity=2)
