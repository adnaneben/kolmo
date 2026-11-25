#!/bin/sh
set -eu
if grep -q 'ppp.layout=v1' /proc/cmdline; then mode=v1; else mode=v2; fi
case $mode in
v2) base=/sys/fs/cgroup; mount -t cgroup2 none "$base"; echo +memory > "$base/cgroup.subtree_control"
    cap=memory.max; current=memory.current;;
v1) base=/run/memory; mkdir "$base"; mount -t cgroup -o memory none "$base";
    cap=memory.limit_in_bytes; current=memory.usage_in_bytes;;
esac
echo "GUEST_MODE=$mode kernel=$(uname -r)"
cat /proc/cgroups
echo "ROOT before=$(/before) patched=$(/cgroup_probe)"
parent="$base/ppp-parent"; leaf="$parent/ppp-leaf"
mkdir "$parent"
if test "$mode" = v2; then echo +memory > "$parent/cgroup.subtree_control"; fi
mkdir "$leaf"
echo 67108864 > "$parent/$cap"
echo 100663296 > "$leaf/$cap"
mkdir '/run/extra mount'
mount --bind "$parent" '/run/extra mount'
mkdir /run/ppp-charge
mount -t tmpfs -o size=16m tmpfs /run/ppp-charge
echo $$ > "$leaf/cgroup.procs"
cat /proc/self/cgroup
grep ' - cgroup' /proc/self/mountinfo
sample() {
    tag=$1; expect=$2
    read -r pc < "$parent/$current"; read -r lc < "$leaf/$current"
    read -r pl < "$parent/$cap"; read -r ll < "$leaf/$cap"
    a=$((pl - pc)); b=$((ll - lc)); exact=$a
    if test "$b" -lt "$exact"; then exact=$b; fi
    before=$(/before)
    set -- $(/cgroup_probe)
    diff=$(($2 - exact)); if test "$diff" -lt 0; then diff=$((-diff)); fi
    echo "CASE=$tag parent=$pl parent_current=$pc leaf=$ll leaf_current=$lc independent_room=$exact patched_rc=$1 patched_room=$2 patched_limit=$3 old=$before drift=$diff"
    test "$1" = 0 && test "$3" = "$expect" && test "$diff" -le 4194304 || return 1
    LAST_ROOM=$2
    /governor "$expect"
    /live-floor
}
sample parent_initial 67108864
first=$LAST_ROOM
dd if=/dev/zero of=/run/ppp-charge/held bs=1048576 count=8 2>/dev/null
sample parent_charged 67108864
used=$((first - LAST_ROOM))
echo "EIGHT_MIB_CHARGE observed_room_drop=$used"
test "$used" -ge 6291456
echo 25165824 > "$leaf/$cap"
sample leaf_tighter 25165824
rm /run/ppp-charge/held
sample leaf_released 25165824
echo 100663296 > "$leaf/$cap"
mkdir "$parent/ppp-sibling"
sample sibling_before_charge 67108864
room_before=$LAST_ROOM
read -r leaf_before < "$leaf/$current"
sh -c 'echo $$ > "$1/cgroup.procs"; exec dd if=/dev/zero of=/run/ppp-charge/from-sibling bs=1048576 count=8' sh "$parent/ppp-sibling" 2>/dev/null
sample sibling_charged 67108864
parent_drop=$((room_before - LAST_ROOM))
read -r leaf_after < "$leaf/$current"
leaf_change=$((leaf_after - leaf_before))
read -r sibling_current < "$parent/ppp-sibling/$current"
echo "SIBLING_CHARGE parent_room_drop=$parent_drop leaf_usage_change=$leaf_change sibling_usage=$sibling_current"
test "$parent_drop" -ge 6291456
test "$leaf_change" -ge -2097152 && test "$leaf_change" -le 2097152
test "$sibling_current" -ge 6291456
umount "$base"
parent='/run/extra mount'; leaf="$parent/ppp-leaf"
echo DELEGATED_VIEW_ONLY
grep ' - cgroup' /proc/self/mountinfo
sample delegated_only 67108864
