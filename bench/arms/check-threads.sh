#!/bin/bash
for d in /z/b /zn1/b /zn2/b /zn3/b; do [ -w "$d" ] || { echo "the bed $d is not mounted or not writable"; exit 1; }; done
source /b/cluster3.sh
cpu_of() { local u=0 s=0; for p in $(pgrep -u "$(id -u)" -f "bin/$KONSENSD --config"); do read a b < <(awk '{print $14, $15}' /proc/$p/stat); u=$((u+a)); s=$((s+b)); done; echo "$u $s"; }
for round in $(seq 1 ${ROUNDS:-3}); do
 for size in ${SIZES:-64}; do
 for conc in 64; do
  for arm in ${ARMS}; do
   k=${arm%:*}; threads=${arm#*:}
   export KONSENSD=$k
   export NODE_EXTRA="sync_by_the_caller = true"
   [ "$threads" != 0 ] && NODE_EXTRA="$NODE_EXTRA\ncheck_threads = $threads"
   export UNIX=1
   n=300; [ $conc = 64 ] && n=8192
   d=/z/b/arms/$k-t$threads-c$conc; rm -rf -- $d; mkdir -p $d
   for i in 1 2 3; do rm -rf -- /zn$i/b/n; mkdir -p /zn$i/b/n; ln -s /zn$i/b/n $d/n$i; done
   t=$(start_konsensd $d) || { echo "round=$round arm=$arm size=$size conc=$conc start failed"; stop_all; continue; }
   read u0 s0 < <(cpu_of)
   r=$(/b/bin/driver -system konsensd -target $t -conc $conc -size $size -samples 3 -per-sample $n -warmup 0 -label $arm | grep -o 'median_writes_per_s=[0-9]* min=[0-9]* max=[0-9]* median_latency_us=[0-9]*')
   read u1 s1 < <(cpu_of)
   echo "round=$round arm=$arm size=$size conc=$conc $r user_us_per_write=$(( (u1-u0)*10000/(3*n) )) sys_us_per_write=$(( (s1-s0)*10000/(3*n) ))"
   stop_all
  done
 done
 done
done
