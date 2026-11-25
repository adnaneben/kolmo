#!/bin/sh
export PATH=/usr/bin:/usr/sbin:/bin:/sbin
mount -t devtmpfs devtmpfs /dev
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mkdir -p /run
exec </dev/console >/dev/console 2>&1
echo "BEGIN_GUEST $(uname -a)"
if sh /cases; then echo KOLMO_CGROUP_PASS; else echo KOLMO_CGROUP_FAIL; fi
sync
poweroff -f
