"""Whole-file A/A2/B measurements for an experimental WebAssembly header transform."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def phase(args):
    start = time.perf_counter_ns()
    child = subprocess.Popen([str(arg) for arg in args], stdout=subprocess.DEVNULL)
    _, status, usage = os.wait4(child.pid, 0)
    wall = (time.perf_counter_ns() - start) / 1e9
    child.returncode = os.waitstatus_to_exitcode(status)
    if child.returncode:
        raise subprocess.CalledProcessError(child.returncode, args)
    return {"wall": wall, "cpu": usage.ru_utime + usage.ru_stime, "rss": usage.ru_maxrss}


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("node_modules", type=Path)
parser.add_argument("vscode_resources", type=Path)
parser.add_argument("kolmo", type=Path)
parser.add_argument("transform", type=Path)
parser.add_argument("out", type=Path)
parser.add_argument("--analysis-none", action="store_true", help="Use the supported stock --analysis none on original bytes for arm B")
parser.add_argument("--guard", type=Path, help="Use an opt-in source build for B, operating on the original module")
parser.add_argument("--stock-decoder", type=Path, help="Use a separately pinned decoder for all readbacks")
parser.add_argument("--holdout", action="store_true", help="Include previously unseen modules and producers")
args = parser.parse_args()
tree = args.vscode_resources / "node_modules.asar.unpacked/@vscode/tree-sitter-wasm/wasm"
files = {
    "qcms": args.node_modules / "pdfjs-dist/wasm/qcms_bg.wasm",
    "jbig2": args.node_modules / "pdfjs-dist/wasm/jbig2.wasm",
    "openjpeg": args.node_modules / "pdfjs-dist/wasm/openjpeg.wasm",
    "webp": args.node_modules / "playwright-core/lib/webp_codec.wasm",
    "ini": tree / "tree-sitter-ini.wasm",
    "typescript": tree / "tree-sitter-typescript.wasm",
    "oniguruma": args.vscode_resources / "node_modules.asar.unpacked/vscode-oniguruma/release/onig.wasm",
    "diff": args.vscode_resources / "node_modules.asar.unpacked/@vscode/diff/dist/diff_wasm_bg-DOQYEGe4.wasm",
}
if args.holdout:
    runtime = args.node_modules
    extra = {
        "tesseract": runtime / "tesseract.js-core/tesseract-core.wasm",
        "embedded-font": runtime / "@oai/artifact-tool/dist/embedded_font_decoder.wasm",
        "emf": runtime / "@oai/artifact-tool/dist/oai_js_emf.wasm",
        "dotnet-json": next((runtime / "@oai/artifact-tool/node_modules/@oai/walnut/wasm").glob("System.Text.Json.*.wasm")),
        "tree-sitter-cpp": args.vscode_resources / "extensions/copilot/dist/tree-sitter-cpp.wasm",
        "vsda": args.vscode_resources / "node_modules.asar.unpacked/vsda/rust/web/vsda_bg.wasm",
    }
    files.update(extra)
rng = random.Random(230025)
kolmo, transform = args.kolmo, args.transform
with tempfile.TemporaryDirectory() as tmp, args.out.open("x") as output:
    frame = Path(tmp) / "frame"
    grouped = Path(tmp) / "grouped"
    decoded = Path(tmp) / "decoded"
    restored = Path(tmp) / "readback"
    for name, source in files.items():
        source_hash = sha(source)
        for level in (1, 3):
            for rep in range(6):
                arms = ["A", "A2", "B"]
                rng.shuffle(arms)
                for arm in arms:
                    for path in (grouped, frame, decoded, restored):
                        path.unlink(missing_ok=True)
                    timings = {}
                    data = source
                    grouping = arm == "B" and not args.analysis_none and not args.guard
                    if grouping:
                        timings["group"] = phase([transform, "group", source, grouped])
                        data = grouped
                    analysis = ["--analysis", "none"] if arm == "B" and args.analysis_none else []
                    binary = args.guard if arm == "B" and args.guard else kolmo
                    if arm == "B" and args.guard:
                        analysis = ["--wasm-check-shuffle"]
                    timings["encode"] = phase([binary, f"-{level}", *analysis, "-T", "1", "-f", "-o", frame, data])
                    final = decoded if grouping else restored
                    timings["decode"] = phase([args.stock_decoder or kolmo, "-d", "-T", "1", "-f", "-o", final, frame])
                    if grouping:
                        timings["restore"] = phase([transform, "restore", decoded, restored])
                    observed = sha(restored)
                    if observed != source_hash:
                        raise ValueError(f"SHA-256 mismatch for {name}")
                    row = dict(file=name, source_path=str(source), arm=arm, level=level, rep=rep,
                               input_bytes=source.stat().st_size, sha256=source_hash, frame_bytes=frame.stat().st_size,
                               frame_sha256=sha(frame), grouped_bytes=data.stat().st_size,
                               grouped_sha256=sha(data), decoded_sha256=observed, times=timings)
                    stored = frame.read_bytes()
                    blocks = int.from_bytes(stored[20:24], "little")
                    row["first_block"] = dict(codec=stored[44], tflag=stored[45],
                                              prefix=stored[32+16*blocks:36+16*blocks].hex())
                    output.write(json.dumps(row) + "\n")
                    output.flush()
        print(f"Completed {name}", flush=True)
