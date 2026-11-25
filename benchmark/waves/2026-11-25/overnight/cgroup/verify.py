"""Check the saved results without a VM. replay.sh reruns the guest measurements."""
import hashlib
from pathlib import Path

here = Path(__file__).resolve().parent
patch = (here / "source.patch").read_bytes()
assert hashlib.sha256(patch).hexdigest() == "2527120adc85a810da2c31bfa7522353ae9883b419e560e2741691b0a231b1cd"
opt = (here / "fixtures-optimized.log").read_text().splitlines()
san = (here / "fixtures-asan-ubsan.log").read_text().splitlines()
assert len(san) == 26 and opt[-26:] == san
assert (here / "memdyn-quick-macos.log").read_text().rstrip().endswith("MEMDYN TESTS PASSED")
for distro in ("debian", "ubuntu"):
    live = (here / "real-kernel" / f"{distro}-v2.log").read_text().splitlines()
    cases = [s for s in live if s.startswith("CASE=")]
    assert len(cases) == 7 and "KOLMO_CGROUP_PASS" in live
    for case in cases:
        expected = 24 << 20 if case.startswith(("CASE=leaf_tighter ", "CASE=leaf_released ")) else 64 << 20
        fields = dict(t.split("=", 1) for t in case.split() if "=" in t)
        assert int(fields["patched_limit"]) == expected and int(fields["patched_rc"]) == 0
        assert abs(int(fields["patched_room"]) - int(fields["independent_room"])) < 1 << 20
        if case.startswith("CASE=parent_initial "):
            assert case.split("old=")[1].split()[2] == str(96 << 20)
        elif case.startswith("CASE=delegated_only "):
            assert fields["old"] == "-1"
    floors = [s.split() for s in live if s.startswith("limit=")]
    assert len(floors) == 7
    for row in floors:
        kv = dict(pair.split("=") for pair in row)
        assert int(kv["auto"]) == int(kv["limit"]) // 2
        assert [kv[k] for k in ("first_jobs", "extra", "final_jobs", "final_used")] == ["1", "0", "0", "0"]
    sibling = next(s for s in live if s.startswith("SIBLING_CHARGE"))
    kv = dict(t.split("=") for t in sibling.split()[1:])
    assert abs(int(kv["parent_room_drop"]) - (8 << 20)) < 1 << 20
    assert abs(int(kv["leaf_usage_change"])) < 1 << 20
    v1 = (here / "real-kernel" / f"{distro}-v1.log").read_text()
    assert "Invalid argument" in v1 and "KOLMO_CGROUP_FAIL" in v1
assert (here / "real-kernel" / "kernel-config.txt").read_text().count("# CONFIG_MEMCG_V1 is not set") == 2
print("Verified patch digest, 26 paired fixtures, Darwin governor, 14 Linux v2 cases and both v1 kernel exclusions.")
