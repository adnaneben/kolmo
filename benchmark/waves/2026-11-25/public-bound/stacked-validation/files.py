"""Compare complete-file frames and restores for the control and both stack builds."""
import argparse
import csv
from hashlib import sha256
from pathlib import Path
import subprocess
import tempfile


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("control", "default", "assembly", "bin-dir", "corpus", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    programs = [p.resolve(strict=True) for p in (args.control, args.default, args.assembly)]
    source = Path(__file__).resolve().parents[5]
    cases = [(args.bin_dir / "cwebp", [str(i) for i in range(1, 10)] + ["ultra"], 1),
             (args.bin_dir / "pdfinfo", ["1", "3", "7", "9", "ultra"], 4),
             (source / "SPEC.md", ["1", "5", "9", "ultra"], 2),
             (args.corpus / "lfw_subset.npy", ["1", "3", "8"], 4),
             (args.corpus / "img_camera.u8", ["1", "5", "9"], 2),
             (args.corpus / "api.json", ["1", "3", "5"], 4)]
    with tempfile.TemporaryDirectory() as work, args.output.open("w") as output:
        frame, restore = (Path(work) / name for name in ("frame", "restored"))
        writer = csv.writer(output, lineterminator="\n")
        writer.writerow(["input", "input_bytes", "input_sha256", "level", "threads",
                         "frame_bytes", "frame_sha256", "matching_writers", "matching_readers"])
        for path, levels, threads in cases:
            input_hash = digest(path)
            for level in levels:
                expected = None
                for program in programs:
                    subprocess.run([program, "--level", level, "-T", str(threads), "-q", "-f",
                                    "-o", frame, "--", path], check=True)
                    contents = frame.read_bytes()
                    if expected is None:
                        expected = contents
                    assert expected == contents, (path, level, program)
                for program in programs:
                    subprocess.run([program, "-d", "-T", str(threads), "-q", "-f",
                                    "-o", restore, "--", frame], check=True)
                    assert digest(restore) == input_hash, (path, level, program)
                writer.writerow([path.name, path.stat().st_size, input_hash, level, threads,
                                 frame.stat().st_size, digest(frame), len(programs), len(programs)])
                output.flush()


if __name__ == "__main__":
    main()
