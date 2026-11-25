# Linux memory admission

The combined source read only the process's leaf at a fixed filesystem path. On real Debian Linux, a 96 MiB leaf inside a 64 MiB parent caused Kolmo to select 96 MiB. Removing the canonical mount made it lose the limit entirely. The patch resolves the process's membership against mountinfo and takes the minimum of each visible enforcing ancestor's limit minus its own usage. The lowest absolute ceiling independently sets the base of the automatic cap. C, Python and Rust sources carry the same implementation.

## Real Linux v2

These are readings from a scratch, diskless x86-64 QEMU guest running Debian kernel 6.12.107. The new and old probes used the same hierarchy. Independent current-byte readings vary as each probe starts and exits.

| Guest setting | Old limit B | Patched limit B | Patched headroom B | Separately sampled headroom B |
| --- | ---: | ---: | ---: | ---: |
| Parent 64 MiB, leaf 96 MiB | 100,663,296 | 67,108,864 | 66,699,264 | 66,883,584 |
| Leaf tightened to 24 MiB | 25,165,824 | 25,165,824 | 16,371,712 | 16,531,456 |
| Sibling holding an 8 MiB file | 100,663,296 | 67,108,864 | 58,212,352 | 58,363,904 |
| Only the delegated mount visible | Unknown | 67,108,864 | 58,273,792 | 58,425,344 |

The sibling charge reduced reported headroom by 8,380,416 B while the independently bracketed leaf usage changed by −20,480 B. A second diskless guest running Ubuntu kernel 7.0.0-30 reproduced the seven cases. Its sibling charge reduced parent headroom by 8,572,928 B with a +36,864 B leaf change. With only the delegated mount, Ubuntu reported a 67,108,864 B limit and 58,028,032 B of headroom. Across the 14 live scenarios, the governor admitted a first oversized accounting reservation, declined a concurrent second one, and released all accounting bytes. The automatic cap was 32 MiB under the 64 MiB parent and 12 MiB under the tighter leaf. These probes reserve numbers in the governor and do not allocate an oversized job. [Debian serial trace](real-kernel/debian-v2.log) · [Ubuntu serial trace](real-kernel/ubuntu-v2.log) · [Guest cases](real-kernel/guest-cases.sh)

## Fixtures and limits

All 26 filesystem fixtures passed both an optimized Darwin build and Clang AddressSanitizer plus UndefinedBehaviorSanitizer. They exercise delegated v1 and v2 mounts, a v1 combined controller list, legacy v1 hierarchy opt-out and unlimited values, additional views of the same subtree, upper bounds with missing usage, path escaping, long membership paths and namespace boundaries. The existing quick memory suite also passed its governor, frame-admission and forced-budget byte-identity checks on macOS. [Optimized fixtures](fixtures-optimized.log) · [Sanitized fixtures](fixtures-asan-ubsan.log) · [Memory tests](memdyn-quick-macos.log)

Linux v1 remains fixture-qualified. The Debian and Ubuntu kernels available locally both have `CONFIG_MEMCG_V1` disabled and rejected a v1 memory mount. The two v2 guests supply the live gate for nested, delegated and sibling accounting. A cgroup namespace or a subtree-only mount can hide higher ancestors. Their usage is unavailable from that process view. An unreadable charge counter leaves the finite ceiling as an upper bound. This change concerns memory admission and concurrency. No compressor throughput or ratio claim was measured in the guest. [Kernel configs](real-kernel/kernel-config.txt) · [v2 kernel semantics](https://docs.kernel.org/admin-guide/cgroup-v2.html) · [v1 hierarchy semantics](https://docs.kernel.org/admin-guide/cgroup-v1/memory.html)

## Source and replay

Run `python3 verify.py` here to check the saved evidence without a guest. [Verification script](verify.py) · [Guest replay](real-kernel/replay.sh)

The source commit is `bd2d8fbea1bb6f89b81a1003597e00c18e5d87bb` on local branch `codex/ppp-cgroup-hierarchy-20261125`, based on combined `ac10ce80048ab0664074f7ba031634055f4d2225`. The patch also passed `git apply --check` against pinned main `a1d39559686a9ec17c8614b9154a64d7b5b98672`. Its SHA-256 is `2527120adc85a810da2c31bfa7522353ae9883b419e560e2741691b0a231b1cd`. Run the following from a scratch checkout of the combined base with the downloaded patch path. [Source patch](source.patch)

```sh
git am /path/to/source.patch
make check-cgroup
make build/test-memdyn
./build/test-memdyn --quick
clang -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer tests/cgroup_probe.c -o build/cgroup-probe-san
python3 tests/test_cgroup.py build/cgroup-probe-san
```
