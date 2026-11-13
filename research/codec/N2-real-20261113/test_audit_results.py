import importlib.util,json,pathlib,tempfile,unittest,hashlib,statistics,copy,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent));import audit_results as a
class AuditTests(unittest.TestCase):
 def row(self,wall=1000,cpu=900):return {'wall_ns':wall,'cpu_ns':cpu,'batch':1}
 def test_cost_model(self):
  c=a.costs(self.row(1000000000),self.row(2000000000),125000000)
  self.assertEqual(c['speed_cost_s'],4.0)
  self.assertAlmostEqual(c['archive_cost_usd'],(1+3.3*2)*(3.89/72)/3600+0.125*0.023)
 def test_helper_cpu(self):self.assertEqual(a.charged(1.0,1.04),1.04)
 def test_exact_three_percent(self):self.assertEqual(a.charged(1.0,1.03),1.0)
 def fixture(self,p):
  # Synthetic ledger only: never used for benchmark/performance claims.
  inp={'name':'fixture.npy','sha256':a.sha(b'x'*100),'bytes':100};(p/'inputs').mkdir();(p/'inputs/fixture.npy').write_bytes(b'x'*100)
  (p/'INPUTS.json').write_text(json.dumps([inp]));summary={};ids={'fixture.npy':{k:a.sha(b'f'*80) for k in a.ARMS}}
  for pn,nr in [('first',7),('confirmation',5)]:
   d=p/pn/'fixture.npy';d.mkdir(parents=True);rows=[];summary[pn]={'fixture.npy':{}}
   for arm in a.ARMS:
    (d/(arm+'.frame')).write_bytes(b'f'*80)
    for rd in range(nr):
     for op in ['encode','decode']:rows.append({'arm':arm,'operation':op,'round':rd,'batch':16,'input_bytes':100,'stored_bytes':80,'wall_ns':1000,'cpu_ns':900})
    c=a.costs(rows[-2],rows[-1],80);summary[pn]['fixture.npy'][arm]={'stored_bytes':80,'runs':[c for _ in range(nr)],'medians':c}
   (d/'stdout.jsonl').write_text(''.join(json.dumps(x)+'\n' for x in rows))
  (p/'SUMMARY.json').write_text(json.dumps(summary));(p/'FRAME_HASHES.json').write_text(json.dumps(ids));self.manifest(p)
 def manifest(self,p):
  m={str(x.relative_to(p)):a.sha(x.read_bytes()) for x in p.rglob('*') if x.is_file() and x.name!='MANIFEST.json'};(p/'MANIFEST.json').write_text(json.dumps(m))
 def test_complete(self):
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t);self.fixture(p);r,rows,_=a.audit(p);self.assertEqual(r['samples'],288);self.assertEqual(r['medians_checked'],144);self.assertEqual(r['frame_checks'],24)
 def corrupt(self,kind):
  with tempfile.TemporaryDirectory() as t:
   p=pathlib.Path(t);self.fixture(p)
   if kind=='frame':(p/'first/fixture.npy/zstd3.frame').write_bytes(b'g'*80)
   elif kind=='median':
    f=p/'SUMMARY.json';x=json.loads(f.read_text());x['first']['fixture.npy']['zstd3']['medians']['speed_cost_s']*=2;f.write_text(json.dumps(x))
   else:
    f=p/'first/fixture.npy/stdout.jsonl';x=f.read_text().splitlines()
    if kind=='duplicate':x.append(x[0])
    if kind=='missing':x.pop()
    if kind=='batch':r=json.loads(x[0]);r['batch']=1;x[0]=json.dumps(r)
    if kind=='nonfinite':r=json.loads(x[0]);r['wall_ns']=float('nan');x[0]=json.dumps(r)
    f.write_text('\n'.join(x)+'\n')
   self.manifest(p)
   with self.assertRaises(ValueError):a.audit(p)
 def test_frame_change(self):self.corrupt('frame')
 def test_median_change(self):self.corrupt('median')
 def test_duplicate(self):self.corrupt('duplicate')
 def test_missing(self):self.corrupt('missing')
 def test_batch(self):self.corrupt('batch')
 def test_nonfinite(self):self.corrupt('nonfinite')
if __name__=='__main__':unittest.main()
