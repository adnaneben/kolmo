#!/usr/bin/env python3
"""Exercise the shipping Linux resolver with unprivileged filesystem fixtures."""
import itertools
from pathlib import Path
import subprocess
import sys
import tempfile

UNLIMITED = 2**64 - 1
PROBE = Path(sys.argv[1]).resolve()


def escape(path):
    return str(path).replace("\\", "\\134").replace(" ", "\\040").replace("\t", "\\011").replace("\n", "\\012")


def mount(root, point, version=2, opts="rw"):
    fstype = "cgroup2" if version == 2 else "cgroup"
    return f"49 31 0:50 {escape(root)} {escape(point)} rw shared:9 - {fstype} cgroup {opts}\n"


def counters(directory, limit=None, usage=None, *, version=2, hierarchy=1):
    directory.mkdir(parents=True, exist_ok=True)
    if version == 2:
        files = {"memory.max": limit, "memory.current": usage}
    else:
        files = {"memory.limit_in_bytes": limit, "memory.usage_in_bytes": usage,
                 "memory.use_hierarchy": hierarchy}
    for name, value in files.items():
        if value is not None:
            (directory / name).write_text(f"{value}\n", encoding="utf-8")


def case(name, member, mounts, room, limit):
    membership.write_text(member, encoding="utf-8")
    mountinfo.write_text(mounts, encoding="utf-8")
    args = [PROBE, membership, mountinfo]
    result = tuple(map(int, subprocess.check_output(args, text=True).split()))
    expected = (0 if limit != UNLIMITED else -1, room, limit)
    print(f"{name}: {result}", flush=True)
    assert result == expected, (name, result, expected)


with tempfile.TemporaryDirectory() as td:
    tmp = Path(td)
    membership = tmp / "membership"
    mountinfo = tmp / "mountinfo"
    # Host files above either mount must never be included in the hierarchy.
    counters(tmp, 1, 1)
    counters(tmp, 1, 1, version=1)

    v2 = tmp / "unified"
    job = v2 / "jobs" / "tenant" / "worker"
    counters(job, 600, 100)
    counters(job.parent, 950, 900)
    counters(job.parent.parent, 2000, 1500)
    counters(v2, "max", 1600)
    member = "0::/jobs/tenant/worker\n"
    case("v2 leaf limit and parent headroom", member, mount("/", v2), 50, 600)
    counters(job, "max", 100)
    case("v2 unlimited leaf", member, mount("/", v2), 50, 950)
    counters(job, "max", 98)
    counters(job.parent, 950, 1000)
    case("v2 over-limit clamps to zero", member, mount("/", v2), 0, 950)
    counters(job.parent, 950, 900)

    delegated = tmp / "delegated space\\tab"
    counters(delegated, 500, 475)
    counters(delegated / "job", "max", 33)
    mounts = mount("/org/delegated", delegated)
    case("v2 delegated mount root with escaped mountpoint", "0::/org/delegated/job\n", mounts, 25, 500)
    case("v2 process at delegated root", "0::/org/delegated\n", mounts, 25, 500)
    case("v2 private namespace root", "0::/\n", mount("/", delegated), 25, 500)
    case("v2 private namespace child", "0::/job\n", mount("/", delegated), 25, 500)
    tighter = tmp / "another-view"
    counters(tighter, 1000, 996)
    counters(tighter / "delegated" / "job", "max", 75)
    case("additional mount exposes tighter parent", "0::/org/delegated/job\n",
         mounts + mount("/org", tighter), 4, 500)
    case("mount rooted above namespace cannot be mapped", "0::/job\n", mount("/..", delegated),
         UNLIMITED, UNLIMITED)
    case("v2 escaped subtree root", "0::/org name/job\n", mount("/org name", delegated), 25, 500)
    case("sibling prefix routes nowhere", "0::/org/delegated-other/job\n", mounts, UNLIMITED, UNLIMITED)
    case("cgroup outside namespace", "0::/../delegated/job\n", mount("/", v2), UNLIMITED, UNLIMITED)
    case("missing process directory", "0::/missing\n", mount("/", v2), UNLIMITED, UNLIMITED)

    components = ["x" * 100] * 6
    longdir = v2.joinpath(*components)
    counters(longdir, 400, 30)
    case("membership path beyond the old buffer", "0::/" + "/".join(components) + "\n",
         mount("/", v2), 370, 400)

    v1 = tmp / "legacy"
    counters(v1, 10000, 9999, version=1, hierarchy=0)
    parent = v1 / "group"
    counters(parent, 900, 860, version=1)
    counters(parent / "child", 400, 150, version=1)
    v1line = "8:cpu,memory,cpuacct:/delegation/group/child\n"
    v1mount = mount("/delegation", v1, 1, "rw,cpu,memory,cpuacct")
    case("v1 delegated combined controllers and ancestor", v1line, v1mount, 40, 400)
    counters(parent, 900, 860, version=1, hierarchy=0)
    case("v1 nonhierarchical ancestor", v1line, v1mount, 250, 400)
    counters(parent / "child", 9223372036854771712, 150, version=1)
    case("v1 unlimited sentinel and nonhierarchical ancestors", v1line, v1mount, UNLIMITED, UNLIMITED)
    counters(parent / "child", 400, 150, version=1, hierarchy=0)
    case("v1 leaf applies even with hierarchy disabled", v1line, v1mount, 250, 400)
    case("v1 mount controller token boundary", v1line, mount("/delegation", v1, 1, "rw,notmemory"), UNLIMITED, UNLIMITED)
    case("v1 membership controller token boundary", "8:memoryfake:/delegation/group/child\n", v1mount, UNLIMITED, UNLIMITED)
    for order in itertools.permutations([v1mount, mount("/", v2)]):
        case("hybrid visible hierarchies", member + v1line, "".join(order), 50, 400)

    empty = tmp / "incomplete"
    counters(empty, 384)
    case("limit with missing usage", "0::/\n", mount("/", empty), 384, 384)
    counters(empty, "max", 0)
    case("no finite limit", "0::/\n", mount("/", empty), UNLIMITED, UNLIMITED)
    counters(empty, "100stray", 0)
    case("malformed counter", "0::/\n", mount("/", empty), UNLIMITED, UNLIMITED)
    case("missing layout", "0::/\n", "", UNLIMITED, UNLIMITED)
