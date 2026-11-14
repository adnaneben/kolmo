import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

from openzl_cli_probe import run, RELEASE

MOCK = '''#!/usr/bin/env python3
import pathlib,sys,zlib
v=sys.argv
if '--version' in v:
 print('zli v0.3.0 mock-test');sys.exit(0)
if v[1]=='compress':
 src=pathlib.Path(v[v.index('--level')+2]);dst=pathlib.Path(v[v.index('--output')+1]);dst.write_bytes(zlib.compress(src.read_bytes()))
elif v[1]=='decompress':
 src=pathlib.Path(v[2]);dst=pathlib.Path(v[v.index('--output')+1]);dst.write_bytes(zlib.decompress(src.read_bytes()))
else:sys.exit(2)
'''

class ProbeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.d = Path(self.tmp.name)
        self.src = self.d/'numbers.bin'; self.src.write_bytes(bytes(range(128))*32)
        self.zli = self.d/'fake_zli.py'; self.zli.write_text(MOCK); self.zli.chmod(0o755)
        h = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
        self.manifest = dict(schema=1, openzl_git_sha=RELEASE,
                             training_regime='untrained', scope='mock_test', level=7,
                             profile='le-u64', repetitions=3, timeout_seconds=5,
                             max_input_bytes=1_000_000, input_path=str(self.src),
                             input_sha256=h(self.src), zli_binary_sha256=h(self.zli),
                             expected_version_fragment='0.3.0')

    def measure(self):
        return run(self.manifest, self.zli, self.d/'out.json')

    def test_roundtrip_and_repetitions(self):
        q = self.measure(); self.assertEqual(len(q['rows']),3)
        self.assertEqual(q['status'], 'measured_mock_only')
        self.assertTrue(q['byte_exact_verified'])
        self.assertFalse(q['comparison_eligible'])
        self.assertEqual({x['pair_id'] for x in q['rows']}, {'r0','r1','r2'})
        self.assertEqual(len({x['frame_sha256'] for x in q['rows']}), 1)

    def test_input_change_rejected(self):
        self.src.write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'hash mismatch'): self.measure()

    def test_binary_change_rejected(self):
        self.zli.write_text(MOCK+'\n')
        with self.assertRaisesRegex(ValueError, 'pinned build'): self.measure()

    def test_trained_rejected(self):
        self.manifest['training_regime']='trained'
        with self.assertRaisesRegex(ValueError,'training'): self.measure()

    def test_profile_rejected(self):
        self.manifest['profile']='../../evil'
        with self.assertRaisesRegex(ValueError,'profile'): self.measure()

    def test_wrong_release_rejected(self):
        self.manifest['openzl_git_sha']='bad'
        with self.assertRaisesRegex(ValueError,'release'): self.measure()

    def test_repetitions_bounded(self):
        self.manifest['repetitions']=999
        with self.assertRaisesRegex(ValueError,'repetitions'): self.measure()

    def test_version_rejected(self):
        self.manifest['expected_version_fragment']='v9.9'
        with self.assertRaisesRegex(ValueError,'version'): self.measure()

    def test_missing_zli_rejected(self):
        self.zli.unlink()
        with self.assertRaisesRegex(ValueError,'unavailable'): self.measure()

    def test_no_cli_record_can_claim_policy_win(self):
        q=self.measure()
        self.assertFalse(q['comparison_eligible'])
        self.assertNotEqual(q['measurement_scope'],'complete_pipeline')

if __name__=='__main__':unittest.main()
