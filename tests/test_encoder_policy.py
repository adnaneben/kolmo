"""Unit tests and optional integration test against real v12 Kolmo builds."""
from hashlib import sha256
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tune"))
import encoder_policy as tool


class PolicyTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.catalogue = tool.load(tool.CATALOGUE)

    def sample(self, name, data, origin="example"):
        path = self.root / name
        path.write_bytes(data)
        return path, tool.digest(path), origin

    def manifest(self, name, samples):
        path = self.root / name
        tool.save(path, [{"path": p.name, "sha256": h, "origin": o} for p, h, o in samples])
        return path

    def test_signatures_and_fallback(self):
        for name, data, fmt in [
            ("weights.gguf", b"plain text", "unknown"),
            ("unknown", b"\x7fELF" + b"data", "elf"),
            ("recording", b"RIFFxxxxWAVE", "wav"),
            ("riff", b"RIFFxxxxAVI ", "unknown"),
            ("parquet", b"PAR1dataPAR1", "parquet"),
            ("truncated", b"PAR1", "unknown"),
        ]:
            path, _, _ = self.sample(name, data)
            self.assertEqual(tool.classify(path, self.catalogue), (fmt, len(data)))
        model = json.dumps({"x": {"dtype": "F32", "shape": [1], "data_offsets": [0, 4]}}).encode()
        file, _, _ = self.sample("tensor", struct.pack("<Q", len(model)) + model + b"data")
        self.assertEqual(tool.bucket(file, self.catalogue), ("safetensors", 0))

    def test_bounded_text_signatures(self):
        rows = b'{"value":1}\r\n{"field":"quoted, tab\\t"}\r\n{"value":[1,2]}'
        csv = b'city,temperature\r\n"Riyadh, SA",12\r\nLondon,-2.0\r\nParis,3.5e1\r\n'
        tsv = b'city\ttemperature\nRiyadh\t12\nLondon\t0.0\nParis\t+3.5e1'
        fastq = b'@r1\nACGTN\n+r1\nIIII!\n@r2\nRYKMS\n+\nJABCD\n@r3 foo\nacgtn\n+\n@@@@@\n'
        for name, data, fmt in [
            ("decimal.txt", rows, "jsonl"),
            ("numbers.bin", csv, "csv"),
            ("data.csv", tsv, "tsv"),
            ("reads", fastq, "fastq"),
            ("log.jsonl", rows + b'\nbad line', "unknown"),
            ("nonfinite.jsonl", rows.replace(b'"value":[1,2]', b'"value":NaN'), "unknown"),
            ("single.jsonl", b'{"a":1}\n{"a":2}\n', "unknown"),
            ("array.jsonl", b'[1]\n[2]\n[3]\n', "unknown"),
            ("untyped.csv", b'name,tag\nAda,one\nBob,two\nCleo,three\n', "unknown"),
            ("prose.csv", b'This, that\nSome sentences, some words\nOne, two\nAgain, here\n', "unknown"),
            ("duplicate.csv", b'Value,vAlue\n9,10\n11,13\n14,18\n', "unknown"),
            ("broken.csv", csv.replace(b'"Riyadh, SA"', b'"Riyadh, SA'), "unknown"),
            ("broken.fastq", fastq.replace(b'JABCD', b'ABCD'), "unknown"),
            ("wrong-id.fastq", fastq.replace(b'+r1', b'+r4'), "unknown"),
            ("short.fastq", fastq + b'@r4\nACGT\n', "unknown"),
            ("control.csv", csv.replace(b'Paris', b'Pa\x00ris'), "unknown"),
            ("binary.csv", csv.replace(b'Paris', b'Pa\xffris'), "unknown"),
            ("ordered.jsonl", b"GGUF\n" + rows, "gguf"),
        ]:
            with self.subTest(name=name):
                sample, _, _ = self.sample(name, data)
                self.assertEqual(tool.classify(sample, self.catalogue), (fmt, len(data)))

        header = ",".join(f"field{i}" for i in range(197))
        row = ",".join(str(i) for i in range(197))
        wide, _, _ = self.sample("wide", ("\n".join([header, row, row, row]) + "\n").encode())
        self.assertEqual(tool.classify(wide, self.catalogue)[0], "csv")

        self.assertEqual(self.catalogue["revision"], 2)
        self.assertEqual(self.catalogue["schema"], 1)

    def test_truncated_probe_and_ambiguous_signature(self):
        prefix = b'{"a":1}\n{"a":2}\n{"a":3}\n{"value":"'
        data = prefix + b'x' * (tool.TEXT_PROBE_LIMIT - len(prefix) - 1) + b'\xc3\xa9"}\n'
        sample, _, _ = self.sample("sample", data)
        self.assertEqual(tool.classify(sample, self.catalogue), ("jsonl", len(data)))

        data = prefix + b'truncated-within-the-file'
        sample, _, _ = self.sample("invalid", data)
        self.assertEqual(tool.classify(sample, self.catalogue), ("unknown", len(data)))

        data = b'a,b\na,1\nb,2\nc,3\n'
        sample, _, _ = self.sample("ambiguous", data)
        catalogue = {**self.catalogue,
                     "formats": [*self.catalogue["formats"],
                                 {"id": "table", "text_probe": "csv"}]}
        self.assertEqual(tool.classify(sample, catalogue), ("unknown", len(data)))

    def test_seal_requires_distinct_origins(self):
        dev = self.manifest("dev.json", [self.sample("dev", b"development")])
        holdout = self.manifest("test.json", [self.sample("test", b"validation")])
        with self.assertRaisesRegex(ValueError, "share sample content or an origin"):
            tool.seal(SimpleNamespace(development=dev, holdout=holdout, output=self.root / "s.json"))
        holdout = self.manifest("test2.json", [self.sample("test", b"development", "other-origin")])
        with self.assertRaisesRegex(ValueError, "share sample content or an origin"):
            tool.seal(SimpleNamespace(development=dev, holdout=holdout, output=self.root / "s.json"))

    def test_split_drift_and_immutable_evidence(self):
        dev = self.manifest("dev.json", [self.sample("dev", b"development")])
        path, h, source = self.sample("test", b"validation", "other-origin")
        holdout = self.manifest("test.json", [(path, h, source)])
        snapshot = self.root / "s.json"
        args = SimpleNamespace(development=dev, holdout=holdout, output=snapshot)
        tool.seal(args)
        with self.assertRaises(FileExistsError):
            tool.seal(args)
        path.write_bytes(b"changed validation")
        with self.assertRaisesRegex(ValueError, "sample content does not match"):
            tool.split_samples(SimpleNamespace(manifest=holdout), "holdout", tool.load(snapshot))
        with holdout.open("a") as file:
            file.write(" ")
        with self.assertRaisesRegex(ValueError, "manifest changed"):
            tool.split_samples(SimpleNamespace(manifest=holdout), "holdout", tool.load(snapshot))

    def test_admission_is_per_file_and_per_budget(self):
        config = tool.configuration(ROOT / "tune" / "encoder-policy.example.v1.json")
        baseline = {"bytes": 1000, "encode_ns": 100, "decode_ns": 100,
                    "encode_rss": 100, "decode_rss": 100}
        good = {**baseline, "bytes": 900}
        invalid = {"bytes": 990, "encode_ns": 106, "decode_ns": 104,
                   "encode_rss": 106, "decode_rss": 106}
        self.assertTrue(tool.passes(baseline, good, config))
        for key, value in invalid.items():
            self.assertFalse(tool.passes(baseline, {**good, key: value}, config))
        self.assertFalse(tool.passes(baseline, {**good, "encode_ns": 105, "decode_ns": 103}, config))
        samples = [self.sample("sample" + str(i), b"GGUF" + bytes([i]), str(i)) for i in range(2)]
        opts = [dict(analysis="full"), dict(analysis="none")]
        opts_map = {("gguf", 0): opts}
        with patch.object(tool, "measure", side_effect=[
            (baseline, good, {**good, "bytes": 850}),
            (baseline, good, {**good, "decode_rss": 200}),
        ]):
            ns = SimpleNamespace(kolmo="encoder", decoder="decoder")
            routes, receipts = tool.score(ns, config, samples, self.catalogue, opts_map)
        self.assertEqual(routes, [{"format": "gguf", "bucket": 0, "choice": opts[0],
                                   "samples": 2, "origins": 2}])
        self.assertEqual(len(receipts), 4)

    def test_opt_in_routes_and_rollback(self):
        file, _, _ = self.sample("model", b"GGUFdata")
        binary, _, _ = self.sample("encoder", b"encoder bytes")
        config = {"level": "1", "threads": 1}
        artifact = {"schema": 1, "kind": "policy", "config": config,
                    "fingerprints": tool.identity(SimpleNamespace(kolmo=binary, decoder=binary)),
                    "routes": [{"format": "gguf", "bucket": 0, "choice": {"analysis": "full"}}]}
        artifact["id"] = tool.object_digest(artifact)
        policy = self.root / "p.json"
        tool.save(policy, artifact)
        args = SimpleNamespace(kolmo=binary, input=file, output=self.root / "out",
                               level="1", threads=1, policy=policy)
        with patch.object(tool.os, "execve") as run:
            tool.encode(args)
            self.assertIn("--analysis", run.call_args.args[1])
            args.threads = 2
            tool.encode(args)
            self.assertNotIn("--analysis", run.call_args.args[1])
            args.policy = None
            args.threads = 1
            tool.encode(args)
            self.assertNotIn("--analysis", run.call_args.args[1])
        artifact["routes"][0]["choice"] = {"analysis": "none"}
        with self.assertRaisesRegex(ValueError, "artifact identity mismatch"):
            tool.accept_artifact(artifact, "policy")

    @unittest.skipUnless(os.getenv("KOLMO_TEST_ENCODER") and os.getenv("KOLMO_TEST_DECODER"),
                         "set both encoder and decoder binary paths")
    def test_real_cli_seal_learn_validate_and_v12_roundtrip(self):
        # Distinct payloads exercise two independent manifest entries per split.
        # Synthetic fixtures here validate wiring, not policy performance.
        arrays = []
        for i in range(4):
            head = b"{'descr': '<i4', 'fortran_order': False, 'shape': (10000,), }"
            padding = (-(11 + len(head))) % 16
            head += b" " * padding + b"\n"
            payload = struct.pack("<10000i", *(j * i + (j // 11) ** (i % 2 + 1) for j in range(10000)))
            raw = b"\x93NUMPY\x01\x00" + struct.pack("<H", len(head)) + head + payload
            arrays.append(self.sample(f"test-{i}.npy", raw, f"synthetic-test-{i}"))
        dev, holdout = self.manifest("dev.json", arrays[:2]), self.manifest("test.json", arrays[2:])
        cfg = tool.configuration(ROOT / "tune" / "encoder-policy.example.v1.json")
        cfg.update(repetitions=3, candidates=[{"analysis": "full"}])
        config = self.root / "config.json"
        tool.save(config, cfg)
        encoder, decoder = (Path(os.environ[name]).resolve() for name in
                            ("KOLMO_TEST_ENCODER", "KOLMO_TEST_DECODER"))
        script = [sys.executable, str(ROOT / "tune" / "encoder_policy.py")]
        def cli(*argv):
            subprocess.run([*script, *map(str, argv)], check=True)

        seal_path, draft, policy, frame, restored = (self.root / n for n in
                                                    ("seal", "draft", "policy", "frame", "restored"))
        cli("seal", "--development", dev, "--holdout", holdout, "--output", seal_path)
        common = ["--kolmo", encoder, "--decoder", decoder, "--config", config, "--seal", seal_path]
        cli("learn", *common, "--manifest", dev, "--output", draft,
            "--receipts", self.root / "dev-receipts")
        self.assertEqual(len(tool.load(self.root / "dev-receipts")), 2)
        cli("validate", *common, "--manifest", holdout, "--draft", draft,
            "--output", policy, "--receipts", self.root / "test-receipts")
        cli("encode", "--kolmo", encoder, "--level", "1", "--policy", policy, arrays[3][0], frame)
        self.assertTrue(frame.read_bytes().startswith(tool.WIRE_HEADER))
        subprocess.run([str(decoder), "-d", "-o", str(restored), "--", str(frame)], check=True)
        self.assertEqual(sha256(restored.read_bytes()).hexdigest(), arrays[3][1])


if __name__ == "__main__":
    unittest.main()
