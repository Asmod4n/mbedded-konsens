#!/bin/bash
for d in /z/b /zn1/b /zn2/b /zn3/b; do [ -w "$d" ] || { echo "the bed $d is not writable"; exit 1; }; done
source /b/cluster3.sh
export KONSENSD=konsensd_g16C0 NODE_EXTRA="sync_by_the_caller = true" UNIX=1
thr() { awk '{printf "%s=%s ", $1, $2}' /sys/fs/cgroup/cpu/cpu.stat; }
for run in $(seq 1 ${RUNS:-12}); do
  d=/z/b/spread; rm -rf -- $d; mkdir -p $d
  for i in 1 2 3; do rm -rf -- /zn$i/b/n; mkdir -p /zn$i/b/n; ln -s /zn$i/b/n $d/n$i; done
  t=$(start_konsensd $d 2> $d/start.txt) || { echo "run=$run start failed"; cat $d/start.txt; stop_all; continue; }
  client=$(grep -o 'client goes to node [0-9]' $d/start.txt | awk '{print $NF}')
  for i in 1 2 3; do wc -l < $d/n$i.log > $d/lines$i; done
  a=$(thr)
  r=$(/b/bin/driver -system konsensd -target $t -conc 64 -size 64 -samples 3 -per-sample 8192 -warmup 0 -label spread | grep -o 'median_writes_per_s=[0-9]* min=[0-9]* max=[0-9]* median_latency_us=[0-9]*')
  b=$(thr)
  echo "run=$run client=$client $r"
  echo "  before: $a"; echo "  after:  $b"
  for i in 1 2 3; do echo "  node$i during the run: $(tail -n +$(( $(cat $d/lines$i) + 1 )) $d/n$i.log | sed 's/konsensd: //' | tr '\n' '|')"; done
  stop_all
done
