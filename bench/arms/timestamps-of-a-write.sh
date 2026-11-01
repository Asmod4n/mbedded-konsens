#!/bin/bash
source /b/cluster.sh
export KONSENSD=konsensd_g16TSS2u NODE_EXTRA="sync_by_the_caller = true" UNIX=1
for conc in 1 64; do
d=/z/b/tracets-c$conc; rm -rf -- $d; mkdir -p $d
for i in 1 2 3; do rm -rf -- /zn$i/b/n; mkdir -p /zn$i/b/n; ln -s /zn$i/b/n $d/n$i; done
t=$(start_konsensd $d) || exit 1
n=200; [ $conc = 64 ] && n=4096
/b/bin/driver -system konsensd -target $t -conc $conc -size 64 -samples 1 -per-sample $n -warmup 0 -label ts | grep -o 'median_writes_per_s=[0-9]*.*median_latency_us=[0-9]*'
grep -h 'is leader in accept' $d/n*.log | tail -1
stop_all; sleep 0.5
for i in 1 2 3; do cp $d/n$i/trace.txt $d/t$i.txt; done
done
