#!/bin/sh
# Usage: ./replay.sh KERNEL INSTALLER_INITRD [v2|v1] [QEMU_SYSTEM_X86_64] [FIRMWARE_DIRECTORY]
set -eu
dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
kernel=$1
initrd=$2
mode=${3:-v2}
qemu=${4:-qemu-system-x86_64}
firmware=${5:-}
case $mode in v1) extra=systemd.unified_cgroup_hierarchy=0;; v2) extra=cgroup_no_v1=all;; *) exit 2;; esac
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/root"
tar -xzf "$dir/guest-probes.tar.gz" -C "$tmp/root"
cp "$dir/guest-init.sh" "$tmp/root/qualify"
cp "$dir/guest-cases.sh" "$tmp/root/cases"
chmod 755 "$tmp/root/"*
(cd "$tmp/root" && find . -type f -print | cpio -o -H newc | gzip -n -c) > "$tmp/overlay.gz"
cat "$initrd" "$tmp/overlay.gz" > "$tmp/initramfs.gz"
if test -n "$firmware"; then set -- -L "$firmware"; else set --; fi
timeout 120 "$qemu" "$@" -machine pc,accel=tcg -cpu max -m 512M -smp 1 -display none -nodefaults -nic none -monitor none -no-reboot -serial stdio -kernel "$kernel" -initrd "$tmp/initramfs.gz" -append "console=ttyS0,115200 rdinit=/qualify panic=1 quiet ppp.layout=$mode $extra" > "$tmp/serial.log" 2>&1 || true
cat "$tmp/serial.log"
grep -q 'KOLMO_CGROUP_PASS' "$tmp/serial.log"
