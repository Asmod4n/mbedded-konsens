#!/bin/bash
source /b/cluster3.sh
export KONSENSD=konsensd_g16C0g UNIX=1 WRAP=/b/vgwrap.sh STABLE_LIMIT=900
export NODE_EXTRA="election_ticks = 100\nsync_by_the_caller = true"
d=/z/b/vg15; rm -rf -- $d; mkdir -p $d
for i in 1 2 3; do rm -rf -- /zn$i/b/n; mkdir -p /zn$i/b/n; ln -s /zn$i/b/n $d/n$i; done
t=$(start_konsensd $d) || exit 1
p=$(for i in 1 2 3; do pgrep -u "$(id -u)" -f "valgrind.*n$i.toml"; done | tr "\n" " ")
echo "target=$t valgrind_pids=$p"
echo "warm $(/b/bin/driver -system konsensd -target $t -conc 64 -size 64 -samples 1 -per-sample 512 -warmup 0 -label warm | grep -o 'median_writes_per_s=[0-9]*')"
for x in $p; do callgrind_control -z $x > /dev/null; done
echo "c64 $(/b/bin/driver -system konsensd -target $t -conc 64 -size 64 -samples 1 -per-sample 4096 -warmup 0 -label c64 | grep -o 'median_writes_per_s=[0-9]*')"
for x in $p; do callgrind_control -d c64 $x > /dev/null; done
grep -h " is " $d/n*.log
stop_all
ls $d
