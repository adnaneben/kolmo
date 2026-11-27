import json, struct, sys, tempfile, unittest
from pathlib import Path
import baselines as b

class BaselineTests(unittest.TestCase):
    def test_npy(self):
        h=b"{'descr': '<u4', 'fortran_order': False, 'shape': (3,)}\n"
        data=b'\x93NUMPY\x01\x00'+len(h).to_bytes(2,'little')+h+struct.pack('<III',0,255,2**32-1)
        self.assertEqual(b.npy_u32(data),[0,255,2**32-1])
        with self.assertRaises(ValueError): b.npy_u32(data[:-1])
    def test_not_npy(self):
        with self.assertRaises(ValueError): b.npy_u32(b'nope')
    def test_csv(self): self.assertEqual(b.csv_bytes(['a','b'],[[1,2],[3,4]]),b'a,b\n1,3\n2,4\n')
    def test_bad_csv(self):
        with self.assertRaises(ValueError): b.csv_bytes(['a','b'],[[1],[2,3]])
        with self.assertRaises(ValueError): b.csv_bytes(['a,b'],[[1]])
    def test_policy_count(self):
        p=b.policies('numpy',dict(kolmo='k',openzl='o',zstd='z'))
        self.assertEqual(len(p),8);self.assertEqual(len({x['id'] for x in p}),8)
        self.assertEqual([x['level'] for x in p if x.get('profile')=='numpy'],[6,9])
    def test_commands(self):
        p=b.policies('csv',dict(kolmo='k',openzl='o',zstd='z'))
        self.assertEqual(b.command(p[0],'encode',Path('a'),Path('b'))[-3:],['a','-o','b'])
        self.assertEqual(b.command(p[1],'decode',Path('a'),Path('b'))[-2:],['a','b'])
    def test_cost(self):
        x={'wall_s':1.,'cpu_s':2.};y={'wall_s':3.,'cpu_s':1.}
        c=b.cost(x,y,125_000_000);self.assertEqual(c['speed_cost_s'],6.)
        self.assertGreater(c['archive_cost_usd'],0)
    def test_child_success(self):
        with tempfile.TemporaryDirectory() as t:
            r=b.timed([sys.executable,'-c','print(123)'],Path(t)/'log',5)
            self.assertEqual(r['returncode'],0);self.assertGreater(r['wall_s'],0)
            self.assertGreater(r['peak_rss_kib'],0)
    def test_child_fail(self):
        with tempfile.TemporaryDirectory() as t:
            r=b.timed([sys.executable,'-c','raise SystemExit(7)'],Path(t)/'log',5)
            self.assertEqual(r['returncode'],7)
    def test_child_timeout(self):
        with tempfile.TemporaryDirectory() as t:
            r=b.timed([sys.executable,'-c','import time;time.sleep(10)'],Path(t)/'log',0.05)
            self.assertTrue(r['timed_out']);self.assertNotEqual(r['returncode'],0)
    def test_confirm(self):
        with tempfile.TemporaryDirectory() as t:
            a=Path(t)/'a';a.mkdir();c=Path(t)/'b';c.mkdir()
            data={'records':[dict(input_id='x',policy={'id':'y'},status='MEASURED_DEVELOPMENT_CONTROL',stored_bytes=10,frame_sha256='h',input_sha256='s')],'completed':1,'failed_profiles':0}
            b.dump(a/'records.json',data);b.dump(c/'records.json',data)
            self.assertEqual(b.confirm(a,c)['status'],'IDENTITIES_MATCH')
            data['records'][0]['stored_bytes']=11;b.dump(c/'records.json',data)
            with self.assertRaises(ValueError): b.confirm(a,c)
    def test_full_runner_with_labelled_copy_fixtures(self):
        # A command/receipt plumbing test, deliberately not a codec benchmark.
        with tempfile.TemporaryDirectory() as t:
            root=Path(t); prepared=root/'prepared'; prepared.mkdir()
            data=b'not compression evidence\n'; (prepared/'input.bin').write_bytes(data)
            b.dump(prepared/'manifest.json',{'inputs':[dict(id='fixture',family='synthetic_test',view='npy',profile='numpy',file='input.bin',input_bytes=len(data),input_sha256=b.sha(data))]})
            tool=root/'copy-tool'
            tool.write_text('#!'+sys.executable+'\nimport sys,shutil\na=sys.argv[1:]\ni=a.index("-o") if "-o" in a else -1\ns,d=(a[i-1],a[i+1]) if i>=0 else a[-2:]\nshutil.copyfile(s,d)\n')
            tool.chmod(0o755)
            r=b.run(prepared,root/'out',{k:str(tool) for k in ['openzl','zstd','kolmo']},rounds=1)
            self.assertEqual(r['completed'],8)
            self.assertTrue(all(x['byte_exact_verified'] for x in r['records']))
            self.assertIn('NOT_POLICY_ACCEPTANCE',r['status'])
if __name__=='__main__': unittest.main()
