"""Synthetic gate tests. No test here supplies compression-performance evidence."""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('n2_check', Path(__file__).with_name('check.py'))
g = importlib.util.module_from_spec(spec)
spec.loader.exec_module(g)

class GateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.original = b'exact original bytes' * 4
        self.rows = []
        self.exe = str(Path(sys.executable).resolve())
        self.config = {x: {'argv': [self.exe, '{input}', '{output}'],
                          'sha256': g.sha(Path(self.exe).read_bytes())}
                       for x in ('kolmo', 'openzl', 'zstd')}
        for arm in g.ARMS:
            frame = b'COPY-FIXTURE-ONLY'
            (self.root / (arm + '.frame')).write_bytes(frame)
            for op in ('encode', 'decode'):
                self.rows.append(dict(arm=arm, operation=op, round=0, batch=16,
                                      input_bytes=len(self.original), stored_bytes=len(frame),
                                      wall_ns=100, cpu_ns=90))

    def audit(self):
        return g.audit_frames(self.rows, self.original, self.root)

    def test_valid_synthetic_ledger(self):
        self.assertEqual(len(self.audit()), 12)

    def test_self_consistent_wrong_original_length(self):
        for row in self.rows: row['input_bytes'] += 1
        with self.assertRaisesRegex(ValueError, 'actual input'): self.audit()

    def test_self_consistent_wrong_batch(self):
        for row in self.rows: row['batch'] = 1
        with self.assertRaisesRegex(ValueError, 'actual input'): self.audit()

    def test_missing_sample(self):
        self.rows.pop()
        with self.assertRaisesRegex(ValueError, 'missing sample'): self.audit()

    def test_duplicate_sample(self):
        self.rows.append(self.rows[0].copy())
        with self.assertRaisesRegex(ValueError, 'duplicate sample'): self.audit()

    def test_unexpected_arm(self):
        self.rows[0]['arm'] = 'unregistered'
        with self.assertRaises(ValueError): self.audit()

    def test_invalid_integers(self):
        for field, value in [('round', False), ('cpu_ns', float('nan')), ('wall_ns', -1)]:
            with self.subTest(field=field):
                old = self.rows[0][field]
                self.rows[0][field] = value
                with self.assertRaises(ValueError): self.audit()
                self.rows[0][field] = old

    def test_changed_frame_length(self):
        (self.root / 'tp_pent.frame').write_bytes(b'x')
        with self.assertRaisesRegex(ValueError, 'size mismatch'): self.audit()

    def test_encode_decode_size_disagreement(self):
        self.rows[1]['stored_bytes'] += 1
        with self.assertRaisesRegex(ValueError, 'sizes differ'): self.audit()

    def test_duplicate_control_bytes_mismatch(self):
        path = self.root / 'zstd3_duplicate.frame'
        path.write_bytes(b'x' * path.stat().st_size)
        with self.assertRaisesRegex(ValueError, 'control frame mismatch'): self.audit()

    def test_missing_and_extra_frames(self):
        path = self.root / 'zstd3.frame'
        data = path.read_bytes()
        path.unlink()
        with self.assertRaisesRegex(ValueError, 'missing or extra frame'): self.audit()
        path.write_bytes(data)
        (self.root / 'extra.frame').write_bytes(b'x')
        with self.assertRaisesRegex(ValueError, 'missing or extra frame'): self.audit()

    def test_symlink_frame(self):
        path = self.root / 'tp_prod3.frame'
        path.unlink()
        path.symlink_to(self.root / 'tp_pent.frame')
        with self.assertRaises(ValueError): self.audit()

    def test_input_and_batch_bounds(self):
        self.assertEqual([g.batch_for(n) for n in (1, 1 << 20, 4 << 20, 16 << 20)], [16, 4, 1, 1])
        for n in (0, (16 << 20) + 1, True):
            with self.assertRaises(ValueError): g.batch_for(n)

    def test_complete_decoder_config(self):
        self.assertEqual(g.validate_decoder_config(self.config), self.config)

    def test_missing_decoder(self):
        del self.config['openzl']
        with self.assertRaises(ValueError): g.validate_decoder_config(self.config)

    def test_decoder_hash_mismatch(self):
        self.config['kolmo']['sha256'] = '0' * 64
        with self.assertRaisesRegex(ValueError, 'hash mismatch'): g.validate_decoder_config(self.config)

    def test_decoder_placeholder_and_path_validation(self):
        original = self.config['kolmo']['argv']
        for argv in ([self.exe, '{input}'], [self.exe, '{input}', '{output}', '{other}'],
                     ['python', '{input}', '{output}']):
            self.config['kolmo']['argv'] = argv
            with self.assertRaises(ValueError): g.validate_decoder_config(self.config)
        self.config['kolmo']['argv'] = original

    def test_restored_bytes_and_length(self):
        path = self.root / 'restored'
        path.write_bytes(b'original')
        g.check_restored(path, b'original')
        path.write_bytes(b'changed!')
        with self.assertRaisesRegex(ValueError, 'byte mismatch'): g.check_restored(path, b'original')
        path.write_bytes(b'x')
        with self.assertRaisesRegex(ValueError, 'length/file mismatch'): g.check_restored(path, b'original')
        path.unlink()
        with self.assertRaises(ValueError): g.check_restored(path, b'original')

    def test_command_success_and_failure_receipts(self):
        prefix = self.root / 'ok'
        self.assertEqual(g.execute([self.exe, '-c', 'print("fixture")'], prefix, 3)['returncode'], 0)
        self.assertEqual(prefix.with_suffix('.stdout').read_text(), 'fixture\n')
        prefix = self.root / 'failure'
        with self.assertRaises(RuntimeError):
            g.execute([self.exe, '-c', 'import sys; print("diagnostic",file=sys.stderr); sys.exit(7)'], prefix, 3)
        self.assertEqual(json.loads(prefix.with_suffix('.command.json').read_text())['returncode'], 7)
        self.assertIn('diagnostic', prefix.with_suffix('.stderr').read_text())

    def test_timeout_and_limits(self):
        prefix = self.root / 'timeout'
        with self.assertRaises(RuntimeError):
            g.execute([self.exe, '-c', 'import time; time.sleep(5)'], prefix, .05)
        self.assertTrue(json.loads(prefix.with_suffix('.command.json').read_text())['timed_out'])
        for value in (0, -1, 181):
            with self.assertRaises(ValueError): g.execute([self.exe], self.root / 'invalid', value)

    def test_frozen_runner_change(self):
        path = self.root / 'research/codec/N2-matched-native/run.py'
        path.parent.mkdir(parents=True)
        path.write_text('raise RuntimeError("must not execute")\n')
        with self.assertRaisesRegex(ValueError, 'frozen N2 runner changed'): g.load_runner(self.root)

    def test_prerequisite_failure_records_not_run(self):
        out = self.root / 'receipt'
        argv = [self.exe, g.__file__]
        for option in ('repo', 'binary', 'source-receipt', 'artifact', 'decoders'):
            argv += ['--' + option, str(self.root / 'absent')]
        argv += ['--output', str(out)]
        result = subprocess.run(argv, capture_output=True, text=True, timeout=5)
        self.assertEqual(result.returncode, 2)
        status = json.loads((out / 'STATUS.json').read_text())
        self.assertEqual(status['status'], 'BLOCKED_PREREQUISITE')
        self.assertEqual(status['performance'], 'NOT_RUN')
        manifest = json.loads((out / 'MANIFEST.json').read_text())
        self.assertEqual(manifest['STATUS.json'], g.sha((out / 'STATUS.json').read_bytes()))

if __name__ == '__main__': unittest.main()
