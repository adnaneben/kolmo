import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

src = Path(__file__).parent / "bench/repro/panel.py"
spec = importlib.util.spec_from_file_location("panel", src)
panel = importlib.util.module_from_spec(spec)
spec.loader.exec_module(panel)


class TrainingTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.train = self.root / "train"
        self.train.mkdir()
        self.sample = self.train / "sample.csv"
        self.sample.write_text("a,b\n1,2\n")
        self.tool = self.root / "zli"
        self.tool.write_text("mock-zli")
        self.tool.chmod(0o700)
        self.artifact = self.root / "artifact.zlc"
        self.artifact.write_bytes(b"legacy bytes")

    def run_fit(self, previous=None, options=()):
        return panel.train_compressor(str(self.tool), ["-p", "csv"], str(self.train),
                                      options, str(self.artifact), previous or {})

    def successful_trainer(self, args):
        candidate = Path(args[args.index("-o") + 1])
        self.assertNotEqual(candidate, self.artifact)
        self.assertEqual(candidate.parent.parent, self.artifact.parent)
        candidate.write_bytes(b"new compressor")
        return (0.01, 0.01)

    def failing_trainer(self, args):
        self.successful_trainer(args)
        return None

    def test_partial_failure_does_not_publish_or_reuse(self):
        with patch.object(panel, "run", self.failing_trainer):
            result, updated = self.run_fit({"ok": True})
        self.assertTrue(updated)
        self.assertFalse(result["ok"])
        self.assertIsNone(result["compressor_bytes"])
        self.assertEqual(self.artifact.read_bytes(), b"legacy bytes")
        self.assertEqual(list(self.root.glob("train-*")), [])

    def test_verified_reuse_and_compressor_drift(self):
        with patch.object(panel, "run", side_effect=self.successful_trainer) as runner:
            record, updated = self.run_fit()
            self.assertTrue(updated)
            self.assertTrue(record["ok"])
            self.assertEqual(self.run_fit(record), (record, False))
            self.artifact.write_bytes(b"other bytes")
            self.assertTrue(self.run_fit(record)[1])
        self.assertEqual(runner.call_count, 2)

    def test_input_tool_arguments_and_training_drift(self):
        with patch.object(panel, "run", self.successful_trainer):
            record, _ = self.run_fit()
            self.sample.write_text("a,b\n4,5\n")
            record, updated = self.run_fit(record)
            self.assertTrue(updated)
            self.tool.write_text("other mock-zli")
            record, updated = self.run_fit(record)
            self.assertTrue(updated)
            record, updated = self.run_fit(record, options=["--max-time-secs", "1"])
            self.assertTrue(updated)

        def drift_during_training(args):
            self.sample.write_text("a,b\n6,7\n")
            return self.successful_trainer(args)
        with patch.object(panel, "run", drift_during_training):
            record, updated = self.run_fit(record)
        self.assertTrue(updated)
        self.assertFalse(record["ok"])

    def test_resume_refuses_stale_trained_rows_and_replays_after_recovery(self):
        sample = self.root / "test.csv"
        sample.write_text("a,b\n7,3\n")
        tools = self.root / "tools.json"
        tools.write_text(json.dumps({"kolmo": {"path": "unused"}, "openzl": {"path": str(self.tool)}}))
        suite = self.root / "suite.json"
        suite.write_text(json.dumps({"sets": [{"name": "s", "label": "s", "files": "test.csv",
                                             "train": "train", "openzl": [["-p", "csv"]]}]}))
        self.artifact = self.root / "work-panel/s-p_csv.zlc"
        self.artifact.parent.mkdir()
        self.artifact.write_bytes(b"legacy")
        output = self.root / "output.json"
        baseline = {"tool": "kolmo -3", "size": 22}
        label = "openzl trained [-p csv]"
        output.write_text(json.dumps({"_training": {"s/-p csv": {"ok": False}},
                                      "test.csv": {"set": "s", "rows": [baseline, {"tool": label, "size": 12}]}}))
        argv = [str(src), str(tools), str(self.root), str(suite), str(output)]
        with patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
            with patch.object(panel, "run", self.failing_trainer), patch.object(panel, "measure") as measure:
                panel.main()
                measure.assert_not_called()
            failed = json.loads(output.read_text())["test.csv"]["rows"]
            self.assertEqual(failed[0], baseline)
            self.assertEqual(failed[1]["tool"], label)
            self.assertIn("error", failed[1])
            self.assertEqual(self.artifact.read_bytes(), b"legacy")
            with patch.object(panel, "run", self.successful_trainer), patch.object(
                panel, "measure", return_value={"tool": label, "size": 19}
            ) as measure:
                panel.main()
                measure.assert_called_once()
                panel.main()
                measure.assert_called_once()
        recovered = json.loads(output.read_text())["test.csv"]["rows"]
        self.assertEqual(recovered, [baseline, {"tool": label, "size": 19, "trained": True}])


if __name__ == "__main__":
    unittest.main()
