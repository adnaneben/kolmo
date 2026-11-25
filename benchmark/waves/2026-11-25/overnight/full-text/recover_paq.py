"""Resume the local enwik8 comparison without re-encoding a completed tool.

Run with --ensure and the full-enwik8-live directory. Repeated calls are safe.
"""
import argparse
import datetime as dt
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import resource
import shutil
import subprocess
import sys
import time


def sha(path):
    value = hashlib.sha256()
    with path.open("rb") as src:
        for block in iter(lambda: src.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def read(path):
    return json.loads(path.read_text())


def write(path, value):
    temp = path.with_suffix(".tmp")
    temp.write_text(json.dumps(value, indent=2) + "\n")
    temp.replace(path)


def utc():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def run_phase(command, folder, phase, receipt):
    with (folder / (phase + ".log")).open("ab") as log:
        before = resource.getrusage(resource.RUSAGE_CHILDREN)
        begin = time.monotonic()
        proc = subprocess.Popen(command, cwd=folder, stdout=log, stderr=log)
        receipt.update(phase=phase, child_pid=proc.pid)
        write(folder / "attempt.json", receipt)
        if proc.wait():
            raise RuntimeError(phase + " returned " + str(proc.returncode))
        elapsed = time.monotonic() - begin
        after = resource.getrusage(resource.RUSAGE_CHILDREN)
        return elapsed, (after.ru_utime + after.ru_stime) - (before.ru_utime + before.ru_stime)


def publish(folder):
    script = Path(__file__).resolve().with_name("record_completed.py")
    subprocess.run([sys.executable, str(script), str(folder / "status.json")], check=True)
    write(folder / "paq-published.json", {"at_utc": utc()})


def worker(folder):
    with (folder / "paq-recovery.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        report = read(folder / "status.json")
        if report["current"] == "complete":
            if not (folder / "paq-published.json").is_file():
                publish(folder)
            return
        if report["current"][0] != "paq8px-8L":
            raise ValueError("Current phase is not PAQ")
        base = {"zstd-19": "baseline.zst", "xz-9": "baseline.xz", "kolmo-ultra": "baseline.kolmo"}
        rows = {r["tool"]: r for r in report["results"]}
        if rows.keys() != base.keys():
            raise ValueError("Expected the three exact completed non-PAQ runs")
        source = Path(rows["kolmo-ultra"]["encode_argv"][-1])
        tool = Path(shutil.which("paq8px") or "/opt/homebrew/bin/paq8px")
        source_sha = "2b49720ec4d78c3c9fabaee6e4179a5e997302b3a70029f30f2d582218c024a8"
        if source.stat().st_size != 100000000 or sha(source) != source_sha or report["source_sha256"] != source_sha:
            raise ValueError("Unexpected source")
        if sha(tool) != report["tool_sha256"]["paq8px"]:
            raise ValueError("Unexpected PAQ executable")
        for name, filename in base.items():
            frame = folder / filename
            if (rows[name]["recovered_sha256"] != source_sha or rows[name]["frame_sha256"] != sha(frame)
                    or rows[name]["stored_bytes"] != frame.stat().st_size):
                raise ValueError("Completed receipt differs for " + name)
        index = folder / "paq-recovery.json"
        if not index.is_file():
            old_pid = report["pid"]
            try:
                os.kill(old_pid, 0)
            except ProcessLookupError:
                pass
            else:
                raise RuntimeError("The previous comparison process is still live")
            old_log = folder / "paq8px-8L-encode.log"
            old_frame = folder / "enwik8.paq8px217"
            initial = {"old_pid": old_pid, "partial_bytes": old_frame.stat().st_size,
                       "partial_sha256": sha(old_frame),
                       "progress": re.findall(r"\d+\.\d+%", old_log.read_text(errors="replace"))[-1],
                       "partial_frame": old_frame.name, "log": old_log.name, "detected_utc": utc()}
            write(index, {"original_interruption": initial, "attempts": []})
        history = read(index)
        check = folder / "paq-encoded.json"
        if check.is_file():
            entry = read(check)
            frame = Path(entry["encode_argv"][-1])
            if sha(frame) != entry["frame_sha256"]:
                raise ValueError("Completed PAQ encode checkpoint changed")
            attempt = frame.parent
            receipt = read(attempt / "attempt.json")
        else:
            attempt = folder / ("paq-recovery-" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
            attempt.mkdir()
            history["attempts"].append(attempt.name)
            write(index, history)
            frame = attempt / "enwik8.paq8px217"
            cmd = [str(tool), "-8L", str(source), str(frame)]
            receipt = {"start_utc": utc(), "worker_pid": os.getpid()}
            report.update(pid=os.getpid(), current=["paq8px-8L", "encode"])
            write(folder / "status.json", report)
            elapsed, cpu = run_phase(cmd, attempt, "encode", receipt)
            entry = {"tool": "paq8px-8L", "encode_seconds": elapsed, "encode_cpu_seconds": cpu,
                     "encode_argv": cmd, "stored_bytes": frame.stat().st_size, "frame_sha256": sha(frame)}
            write(check, entry)
        outdir = attempt / ("restore-" + dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
        outdir.mkdir()
        dec = [str(tool), "-d", str(frame), str(outdir)]
        report.update(pid=os.getpid(), current=["paq8px-8L", "decode"])
        write(folder / "status.json", report)
        elapsed, cpu = run_phase(dec, attempt, "decode", receipt)
        restored = outdir / source.name
        if not restored.is_file() or sha(restored) != source_sha or restored.stat().st_size != source.stat().st_size:
            raise ValueError("PAQ product did not restore exact complete enwik8")
        entry.update(decode_argv=dec, decode_seconds=elapsed, decode_cpu_seconds=cpu,
                     recovered_sha256=source_sha, combined_MB_s=100 / (entry["encode_seconds"] + elapsed))
        report["results"].append(entry)
        report["current"] = "complete"
        write(folder / "status.json", report)
        receipt.update(completed_utc=utc(), phase="complete")
        write(attempt / "attempt.json", receipt)
        publish(folder)


def ensure(folder):
    with (folder / "paq-recovery.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print("A recovery worker already holds the file lock.")
            return
    if (folder / "paq-published.json").is_file():
        print("Already complete and published.")
        return
    with (folder / "paq-supervisor.log").open("ab") as log:
        cmd = [sys.executable, str(Path(__file__).resolve()), "--worker", str(folder)]
        proc = subprocess.Popen(cmd, cwd=folder, stdin=subprocess.DEVNULL, stdout=log, stderr=log,
                                close_fds=True, start_new_session=True)
        if shutil.which("caffeinate"):
            subprocess.Popen(["caffeinate", "-s", "-w", str(proc.pid)], cwd=folder, stdin=subprocess.DEVNULL,
                             stdout=log, stderr=log, close_fds=True, start_new_session=True)
        print("Started PAQ recovery worker", proc.pid)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--ensure", action="store_true")
    mode.add_argument("--worker", action="store_true")
    parser.add_argument("directory", type=Path)
    opt = parser.parse_args()
    (ensure if opt.ensure else worker)(opt.directory.resolve(strict=True))
