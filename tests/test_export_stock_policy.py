"""Native empty-policy deployment eligibility and atomic publication checks."""
from pathlib import Path
import shutil
import sys
import tempfile
import unittest
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tune"))
import encoder_policy as policy
import export_stock_policy as deploy


class ExportTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        exp = self.root / "experiment"
        shutil.copytree(ROOT / "benchmark/learning-speed-ppp", exp)
        # Re-scope the checked-in pilot to two local executable fixtures.
        encoder = self.root / "stock"
        encoder.write_bytes(b"stock test image")
        encoder.chmod(0o700)
        decoder = self.root / "reader"
        decoder.write_bytes(b"reader test image")
        decoder.chmod(0o700)
        self.args = SimpleNamespace(experiment=exp, kolmo=encoder, decoder=decoder,
                                    output=self.root / "deployed")
        self.resign("draft", fingerprints=policy.identity(self.args))
        self.resign("accepted", fingerprints=policy.identity(self.args),
                    draft=policy.load(exp / "draft.json")["id"])

    def resign(self, name, **changes):
        path = self.args.experiment / (name + ".json")
        value = policy.load(path)
        value.update(changes)
        del value["id"]
        value["id"] = policy.object_digest(value)
        path.unlink()
        policy.save(path, value)

    def test_copy_and_existing_output(self):
        receipt = deploy.export(self.args)
        self.assertEqual(receipt["encoder_sha256"], policy.digest(self.args.output))
        self.assertEqual(self.args.output.read_bytes(), self.args.kolmo.read_bytes())
        self.assertEqual(self.args.output.stat().st_mode & 0o777, 0o700)
        self.assertNotEqual(self.args.output.stat().st_ino, self.args.kolmo.stat().st_ino)
        self.args.output.write_bytes(b"operator replacement")
        with self.assertRaises(FileExistsError):
            deploy.export(self.args)
        self.assertEqual(self.args.output.read_bytes(), b"operator replacement")
        self.assertEqual(list(self.root.glob(".kolmo-export-*")), [])

    def test_reject_nonempty_even_with_valid_identity(self):
        self.resign("accepted", routes=[{"format": "elf", "bucket": 1, "choice": {"analysis": "none"}}])
        with self.assertRaisesRegex(ValueError, "nonempty policy"):
            deploy.export(self.args)
        self.assertFalse(self.args.output.exists())

    def test_reject_unvalidated_or_mismatched_evidence(self):
        self.resign("accepted", kind="draft")
        with self.assertRaisesRegex(ValueError, "expected a v1 policy"):
            deploy.export(self.args)
        self.resign("accepted", kind="policy")
        path = self.args.experiment / "holdout-receipts.json"
        path.write_text("[]")
        with self.assertRaisesRegex(ValueError, "differ from validated evidence"):
            deploy.export(self.args)
        self.assertFalse(self.args.output.exists())

    def test_reject_changed_reader_and_encoder(self):
        for name in ("decoder", "kolmo"):
            with self.subTest(name=name):
                path = getattr(self.args, name)
                raw = path.read_bytes()
                path.write_bytes(raw + b"other")
                with self.assertRaisesRegex(ValueError, "differs from validation"):
                    deploy.export(self.args)
                path.write_bytes(raw)
        self.assertFalse(self.args.output.exists())

    def test_reject_missing_holdout_even_with_consistent_receipt_hash(self):
        path = self.args.experiment / "holdout-receipts.json"
        rows = policy.load(path)[:-1]
        path.unlink()
        policy.save(path, rows)
        self.resign("accepted", receipts_sha256=policy.object_digest(rows))
        with self.assertRaisesRegex(ValueError, "every holdout"):
            deploy.export(self.args)


if __name__ == "__main__":
    unittest.main()
