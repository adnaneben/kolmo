"""Compare all versioned native binding files with canonical sources and declared packaging exclusions."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

repo = Path(sys.argv[1]).resolve()
def files(prefix):
    return subprocess.check_output(["git", "-C", str(repo), "ls-files", "-z", prefix]).decode().split("\0")[:-1]
def sha(p):
    return hashlib.sha256((repo / p).read_bytes()).hexdigest()
eligible = {p for p in files("src") if not (p.startswith("src/experimental/") and not Path(p).name.startswith("frontier_"))}
report = {}
for kind in ("python", "rust"):
    root = f"bindings/{kind}/native/"
    pairs = {p[len(root):]: p for p in files(root)}
    mismatches = [{"source": src, "canonical_sha256": sha(src), "mirror_sha256": sha(pairs[src])}
                  for src in sorted(eligible & pairs.keys()) if sha(src) != sha(pairs[src])]
    report[kind] = {"canonical_files": len(eligible), "tracked_mirrors": len(pairs),
                    "missing": sorted(eligible - pairs.keys()), "extra": sorted(pairs.keys() - eligible),
                    "different": mismatches}
print(json.dumps(report, indent=2))
