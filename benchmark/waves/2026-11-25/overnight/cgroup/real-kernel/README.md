# Linux cgroup memory-controller checks on live kernels

Seven nested-cgroup v2 cases passed independently on Debian 6.12.107+deb13-amd64 and Ubuntu 7.0.0-30-generic. Before these runs, the implementation had fixture evidence for these layouts. Both guests booted an actual distribution Linux kernel on QEMU 10.0.12’s x86_64 TCG software CPU, using 512 MiB of RAM, one vCPU, a read-only installer initramfs plus the test overlay, and no disks or network devices. The cases created and charged real guest-kernel cgroups. No host cgroups or existing virtual machines participated. These results qualify memory accounting and admission, not execution speed on x86.

Tested source was `bd2d8fbea1bb6f89b81a1003597e00c18e5d87bb` versus `ac10ce80048ab0664074f7ba031634055f4d2225`. The candidate sampler SHA-256 (`src/core/cgroup.h`) was `1677972ad57a763cfe75e635a82a2cd6fb9f06085901019354204b8ac6e4ada8`. The POSIX probes used Debian glibc 2.41. The baseline probe includes the old source functions. `governor.c` includes the real candidate translation unit and `../live_floor.c` independently includes its memory implementation. The fresh guest processes used OS sampling without a forced budget.

| Live case | Old helper | Patched result on both distributions |
|---|---|---|
| Parent cap 64 MiB, leaf cap 96 MiB | Reported 96 MiB | Reported 64 MiB |
| Charge eight MiB to the leaf | Counted leaf usage against 96 MiB | Counted usage against the 64 MiB parent |
| Cap the leaf at 24 MiB, then remove the leaf charge | Found 24 MiB | Found 24 MiB and recovered the released room |
| Separate sibling charges eight MiB under the 64 MiB parent | Used the 96 MiB leaf view | Parent room dropped 8,380,416 B on Debian and 8,572,928 B on Ubuntu, while leaf usage changed −20,480 B and +36,864 B respectively |
| Unmount the canonical hierarchy and retain the parent subtree bind at `/run/extra mount` | Failed to find any limit | Found the 64 MiB parent and measured 58,273,792 B / 58,028,032 B of room respectively |

Each sample separately read the real ancestor and leaf current usage/cap files. The permitted drift between that sample and the child sampler was 4 MiB, accounting for the short-lived probe process. The serial logs give every raw reading and drift. At each of the seven stages, the governed budget was finite, the first oversized accounting reservation was admitted, a simultaneous extra reservation was denied, and a new reservation succeeded after release. The second probe independently verified the automatic limit (32 MiB under the 64 MiB cap, 12 MiB under the 24 MiB cap), a single admitted job, and zero jobs/bytes after release. No large allocation was made for those admission tests.

Both kernels rejected an attempted cgroup v1 memory mount with `EINVAL`. The kernel configurations shipped on both installation media explicitly have `CONFIG_MEMCG=y` and `# CONFIG_MEMCG_V1 is not set`. Live v1 ancestor limits, headroom under pressure, and governor admission remain unverified. The fixture suite covers the v1 layouts separately. The first concurrent Ubuntu v2 boot stopped during initramfs unpacking before any cgroup tests ran. An isolated replay booted and finished with the success marker.

## Replay

The Debian `debian-13.7.0-amd64-netinst.iso` SHA-256 was `a7ef94ac2fb9a7fec454552abd629b7cc9d5155c886165a45649f5ce6167e355`. Extract `install.amd/vmlinuz` and `install.amd/initrd.gz` with bsdtar, or supply equivalent local copies. Their SHA-256 hashes were `2b2358b37674d2505350528875bb17afae2a36522a9e8a9417eaca65a7da0e08` and `8de544ab627ab794504c530c82de621814386b5b67a492f1e3133390c47ea933`. The second kernel was `casper/vmlinuz` from the Ubuntu 26.04.1 desktop amd64 ISO, SHA-256 `0ad39b13e289e1a5cf806d14541ac8f221eefe849017f5915e0846917ed67785`. Both used the same Debian installer userspace.

Run `./replay.sh /path/to/vmlinuz /path/to/initrd.gz v2 /path/to/qemu-system-x86_64`. An optional fifth argument supplies QEMU’s firmware directory. Replay requires timeout, gzip and newc-compatible cpio. The guest prints `KOLMO_CGROUP_PASS` only after all seven cases and both admission checks pass. Passing `v1` instead attempts the real v1 memory-controller mount; absence of the success marker gives a nonzero shell status. The supplied installer kernels cannot qualify v1.

On this Mac the installed UTM framework provided QEMU. The small adapter in `qemu-framework.c` links to UTM's `qemu-x86_64-softmmu` framework. QEMU used the firmware in UTM's Resources/qemu directory. The guest probes can also be rebuilt from the tested checkout with `rebuild-probes.sh`, Clang, LLVM's ELF linker, and a Debian amd64 sysroot containing glibc runtime and development files and linux-libc-dev headers. `tested-elf.sha256` identifies the four executables packed in `guest-probes.tar.gz`. All four rebuilt byte-for-byte using the stated recipe.
