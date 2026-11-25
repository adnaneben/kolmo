"""Exercise the benchmark resume guard without building or timing any codec."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


class ResumeIdentityTest(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory(prefix="kolmo-bench-identity-")
        self.addCleanup(directory.cleanup)
        root = Path(directory.name)
        self.repo = root / "source"
        self.work = root / "work"
        self.results = self.work / "results"
        here = Path(__file__).resolve().parent
        self.repro = self.repo / "bench/repro"
        self.repro.mkdir(parents=True)
        (self.repo / "bench/lzbench").mkdir()
        (self.repo / "site").mkdir()
        for name in ("run.sh", "identity.py"):
            shutil.copyfile(here / name, self.repro / name)
        self.runner = self.repro / "lzbench_run.py"
        self.runner.write_text(
            'import pathlib, sys\n'
            'p = pathlib.Path(sys.argv[3]) / "ran"\n'
            'p.parent.mkdir(exist_ok=True)\n'
            'p.touch()\n')
        (self.repo / "VERSION").write_text("sample\n")
        (self.repo / "src").mkdir()
        self.source = self.repo / "src/codec.c"
        self.source.write_text("first source\n")
        self.git("init", "-q")
        self.git("add", ".")
        self.git("commit", "-qm", "fixture")
        self.first_ref = self.git("rev-parse", "HEAD").stdout.strip()
        self.source.write_text("second source\n")
        self.git("add", "src")
        self.git("commit", "-qm", "other source")
        self.git("tag", "vsample")
        self.binary = root / "candidate"
        self.binary.write_bytes(b"codec binary")
        self.corpus = self.work / "data/corpus"
        self.corpus.mkdir(parents=True)
        (self.corpus / "canterbury.tar").write_bytes(b"original corpus")

    def git(self, *args):
        return subprocess.run(["git", "-c", "user.name=Test", "-c", "user.email=test@example.com", *args],
                              cwd=self.repo, text=True, check=True, capture_output=True)

    def run_bench(self, *args, steps="", okay=True):
        result = subprocess.run(["sh", str(self.repro / "run.sh"), str(self.work),
                                 "--steps", steps, *args],
                                cwd=self.repo, text=True, capture_output=True)
        self.assertEqual(result.returncode == 0, okay, result.stdout + result.stderr)
        return result

    def setup_tools(self):
        self.run_bench()
        tools = {k: {"path": str(self.binary)} for k in ("kolmo", "lzbench", "openzl")}
        tools["cli"] = {}
        (self.results / "tools.json").write_text(json.dumps(tools))

    def test_same_commit_alias_resumes_and_other_ref_fails(self):
        self.run_bench("--ref", "vsample")
        latest = self.git("rev-parse", "HEAD").stdout.strip()
        self.run_bench("--ref", latest)
        self.assertIn("run-identity.json changed",
                      self.run_bench("--ref", self.first_ref, okay=False).stderr)
        self.assertIn("run-identity.json changed", self.run_bench("--quick", okay=False).stderr)

    def test_protocol_change_fails(self):
        self.run_bench()
        self.runner.write_text("raise SystemExit(3)\n")
        self.assertIn("run-identity.json changed", self.run_bench(okay=False).stderr)

    def test_worktree_change_fails(self):
        self.run_bench("--ref", "WORKTREE")
        self.source.write_text("new local source\n")
        self.assertIn("run-identity.json changed",
                      self.run_bench("--ref", "WORKTREE", okay=False).stderr)

    def test_binary_and_input_changes_fail(self):
        self.setup_tools()
        self.run_bench(steps="lzbench")
        self.assertTrue((self.results / "lzbench/ran").is_file())
        self.run_bench(steps="lzbench")
        self.binary.write_bytes(b"different binary")
        self.assertIn("tool-identity.json changed", self.run_bench(steps="lzbench", okay=False).stderr)
        self.binary.write_bytes(b"codec binary")
        (self.corpus / "canterbury.tar").write_bytes(b"different corpus")
        self.assertIn("input-identity.json changed", self.run_bench(steps="lzbench", okay=False).stderr)

    def test_unidentified_results_are_preserved(self):
        self.results.mkdir()
        legacy = self.results / "panel.json"
        legacy.write_text("{}\n")
        self.assertIn("no run identity", self.run_bench(okay=False).stderr)
        self.assertEqual(legacy.read_text(), "{}\n")

    def test_busy_work_is_not_reused(self):
        (self.results / ".run.lock").mkdir(parents=True)
        self.assertIn("WORK is locked", self.run_bench(okay=False).stderr)
        self.assertTrue((self.results / ".run.lock").is_dir())


if __name__ == "__main__":
    unittest.main()
