#!/bin/sh
# Build kolmo and every comparison tool of the public benchmark at pinned versions.
#
#   sh bench/repro/build.sh WORK [kolmo-ref]
#
# kolmo-ref: a tag or commit of this repository (default: HEAD), or WORKTREE for the working tree.
# Writes WORK/tools.json (paths, versions, commits, flags), read by every other step.
#
# Pins (change them here, nowhere else):
#   lzbench  inikep/lzbench   9be0e25505b1317cde1afe70d2114164eb4faf8f (2.4.1, 2026-11-11)
#   OpenZL   facebook/openzl  50f8789a0c6da9273079045310149db396bb5bba (zli reports 0.3.0)
#   CLIs     zstd 1.5.7, xz 5.8.x, bzip3 1.5.5: the system's copies are used when their versions match;
#            otherwise the mismatch is recorded in tools.json (and shown on the page), not hidden.
# Reuse local clones instead of cloning: LZBENCH_SRC=/path OPENZL_SRC=/path (they are checked out at the pin
# in a fresh copy, never modified in place).
#
# Fairness: kolmo is built with `make ARCH=` (generic -O3, no -mcpu/-march), the flags of the release binaries.
# lzbench builds every codec, kolmo included, with its own default flags. OpenZL is a CMake Release build.
set -e
WORK=${1:?usage: build.sh WORK [kolmo-ref]}
REF=${2:-HEAD}
LZBENCH_PIN=9be0e25505b1317cde1afe70d2114164eb4faf8f
OPENZL_PIN=50f8789a0c6da9273079045310149db396bb5bba
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
mkdir -p "$WORK"; WORK=$(cd "$WORK" && pwd)
ncpu=$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)
[ -f "$root/bench/lzbench/install.py" ] || { echo "build.sh: needs bench/lzbench/install.py (the lzbench plugin)"; exit 1; }

# 1. kolmo CLI, from a clean export of REF.
rm -rf "$WORK/kolmo"; mkdir "$WORK/kolmo"
if [ "$REF" = WORKTREE ]; then
    (cd "$root" && git ls-files -z | xargs -0 tar -cf -) | tar -xf - -C "$WORK/kolmo"; kcommit="WORKTREE($(git -C "$root" rev-parse HEAD))"
else
    kcommit=$(git -C "$root" rev-parse --verify "$REF^{commit}")
    git -C "$root" archive "$kcommit" | tar -xf - -C "$WORK/kolmo"
fi
make -C "$WORK/kolmo" -j"$ncpu" ARCH= build/kolmo > "$WORK/kolmo.log" 2>&1 || { tail -20 "$WORK/kolmo.log"; exit 1; }
echo "kolmo $(cat "$WORK/kolmo/VERSION") ($kcommit) built"

# 2. lzbench at the pin, with kolmo added by bench/lzbench/install.py (same source as the CLI).
rm -rf "$WORK/lzbench"
git clone -q "${LZBENCH_SRC:-https://github.com/inikep/lzbench}" "$WORK/lzbench"
git -C "$WORK/lzbench" checkout -q "$LZBENCH_PIN"
if [ "$REF" = WORKTREE ]; then lref=WORKTREE; else lref=$kcommit; fi
python3 -I "$root/bench/lzbench/install.py" "$WORK/lzbench" --kolmo "$root" --ref "$lref" > "$WORK/lzbench-install.log" 2>&1 \
    || { tail -20 "$WORK/lzbench-install.log"; exit 1; }
make -C "$WORK/lzbench" -j"$ncpu" > "$WORK/lzbench.log" 2>&1 || { tail -20 "$WORK/lzbench.log"; exit 1; }
echo "lzbench $LZBENCH_PIN built"

# 3. OpenZL's zli at the pin.
rm -rf "$WORK/openzl"
git clone -q "${OPENZL_SRC:-https://github.com/facebook/openzl}" "$WORK/openzl"
git -C "$WORK/openzl" checkout -q "$OPENZL_PIN"
cmake -S "$WORK/openzl" -B "$WORK/openzl/build" -DCMAKE_BUILD_TYPE=Release -DOPENZL_BUILD_CLI=ON \
    -DOPENZL_BUILD_TESTS=OFF -DOPENZL_BUILD_EXAMPLES=OFF > "$WORK/openzl.log" 2>&1 \
    && cmake --build "$WORK/openzl/build" -j"$ncpu" --target zli >> "$WORK/openzl.log" 2>&1 \
    || { tail -20 "$WORK/openzl.log"; exit 1; }
echo "OpenZL $OPENZL_PIN built"

# 4. Command-line baselines, and the record of everything.
python3 -I - "$WORK" "$kcommit" "$LZBENCH_PIN" "$OPENZL_PIN" <<'PY'
import json, os, platform, re, shutil, subprocess, sys
work, kc, lz, oz = sys.argv[1:5]
def ver(argv, rx):
    try:
        out = subprocess.run(argv, capture_output=True, text=True).stdout + subprocess.run(argv, capture_output=True, text=True).stderr
        m = re.search(rx, out); return m.group(1) if m else out.strip()[:60]
    except FileNotFoundError:
        return None
want = {"zstd": "1.5.7", "xz": "5.8", "bzip3": "1.5.5"}
cli = {}
for name, argv, rx in [("zstd", ["zstd", "--version"], r"v(\d+\.\d+\.\d+)"), ("xz", ["xz", "--version"], r"(\d+\.\d+\.\d+)"),
                       ("bzip3", ["bzip3", "--version"], r"(\d+\.\d+\.\d+)")]:
    v = ver(argv, rx)
    cli[name] = dict(path=shutil.which(name), version=v, pinned=want[name], matches=bool(v and v.startswith(want[name])))
    if not cli[name]["matches"]:
        print(f"warning: {name} {v} is not the pinned {want[name]}; recorded in tools.json")
cpu = platform.processor()
try:
    cpu = subprocess.run(["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True).stdout.strip() or cpu
except FileNotFoundError:
    try:
        cpu = next(l.split(":", 1)[1].strip() for l in open("/proc/cpuinfo") if l.startswith("model name"))
    except Exception:
        pass
cc = subprocess.run(["cc", "--version"], capture_output=True, text=True).stdout.split("\n")[0]
tools = dict(kolmo=dict(path=f"{work}/kolmo/build/kolmo", version=open(f"{work}/kolmo/VERSION").read().strip(), commit=kc,
                        flags="make ARCH= (generic -O3)"),
             lzbench=dict(path=f"{work}/lzbench/lzbench", commit=lz),
             openzl=dict(path=f"{work}/openzl/build/cli/zli", commit=oz, version="0.3.0"),
             cli=cli, machine=dict(cpu=cpu, os=f"{platform.system()} {platform.release()}", arch=platform.machine(),
                                   cores=os.cpu_count(), compiler=cc))
json.dump(tools, open(f"{work}/tools.json", "w"), indent=1)
print(f"wrote {work}/tools.json")
PY
