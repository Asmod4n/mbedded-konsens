#!/bin/bash
# start_<system> <data root>; prints the target of the driver on stdout
set -uo pipefail
B=/b/bin

wait_for() { for i in $(seq 1 300); do eval "$1" >/dev/null && return 0; sleep 0.2; done; echo "timeout: $1" >&2; return 1; }

start_etcd() {
  local d=$1 cl="n1=http://127.0.0.1:2380,n2=http://127.0.0.1:2390,n3=http://127.0.0.1:2400"
  for i in 1 2 3; do
    local cp=$((2379 + (i-1)*10)) pp=$((2380 + (i-1)*10))
    ${WRAP:+$WRAP -o $d/strace.n${i:-1}} etcd --name n$i --data-dir $d/n$i --listen-client-urls http://127.0.0.1:$cp --advertise-client-urls http://127.0.0.1:$cp \
      --listen-peer-urls http://127.0.0.1:$pp --initial-advertise-peer-urls http://127.0.0.1:$pp --initial-cluster $cl \
      --initial-cluster-state new --log-level error > $d/n$i.log 2>&1 &
  done
  local eps=127.0.0.1:2379,127.0.0.1:2389,127.0.0.1:2399
  wait_for "etcdctl --endpoints $eps endpoint status -w json | grep -q '\"leader\"'"
  etcdctl --endpoints $eps endpoint status -w json | python3 -c 'import json,sys; print(next(e["Endpoint"] for e in json.load(sys.stdin) if e["Status"]["leader"] == e["Status"]["header"]["member_id"]))'
}

start_rqlite() {
  local d=$1
  for i in 1 2 3; do
    ${WRAP:+$WRAP -o $d/strace.n${i:-1}} $B/rqlited -node-id $i -http-addr 127.0.0.1:$((4000 + 2*i - 1)) -raft-addr 127.0.0.1:$((4000 + 2*i)) \
      -bootstrap-expect 3 -join 127.0.0.1:4002,127.0.0.1:4004,127.0.0.1:4006 $d/n$i > $d/n$i.log 2>&1 &
  done
  wait_for "curl -sf 127.0.0.1:4001/readyz"
  local leader
  for i in $(seq 1 100); do
    leader=$(curl -s 127.0.0.1:4001/status | python3 -c 'import json,sys; d=json.load(sys.stdin); print(d["store"]["leader"]["node_id"])') && [ -n "$leader" ] && break; sleep 0.2
  done
  local api=127.0.0.1:$((4000 + 2*leader - 1))
  curl -sf -XPOST $api/db/execute -H 'Content-Type: application/json' -d '["CREATE TABLE kv (k TEXT PRIMARY KEY, v TEXT NOT NULL)"]' >/dev/null
  echo $api
}

start_dqlite() {
  local d=$1
  ${WRAP:+$WRAP -o $d/strace.n${i:-1}} $B/dqlite-demo --api 127.0.0.1:8001 --db 127.0.0.1:9001 --dir $d/n1 > $d/n1.log 2>&1 &
  wait_for "curl -sf 127.0.0.1:8001/x || curl -s 127.0.0.1:8001/x | grep -q ."
  for i in 2 3; do
    ${WRAP:+$WRAP -o $d/strace.n${i:-1}} $B/dqlite-demo --api 127.0.0.1:800$i --db 127.0.0.1:900$i --dir $d/n$i --join 127.0.0.1:9001 > $d/n$i.log 2>&1 &
  done
  wait_for "curl -s 127.0.0.1:8003/x | grep -q ."
  echo 127.0.0.1:9001,127.0.0.1:9002,127.0.0.1:9003
}

start_zookeeper() {
  local d=$1
  for i in 1 2 3; do
    mkdir -p $d/n$i && echo $i > $d/n$i/myid
    cat > $d/n$i.cfg <<CFG
tickTime=200
initLimit=10
syncLimit=5
dataDir=$d/n$i
clientPort=$((2180 + i))
4lw.commands.whitelist=srvr
admin.enableServer=false
server.1=127.0.0.1:2888:3888
server.2=127.0.0.1:2889:3889
server.3=127.0.0.1:2890:3890
CFG
    ZOOCFGDIR=$d ZOO_LOG_DIR=$d ${WRAP:+$WRAP -o $d/strace.n$i} java -cp "/usr/share/java/*:/usr/share/zookeeper/*" org.apache.zookeeper.server.quorum.QuorumPeerMain $d/n$i.cfg > $d/n$i.log 2>&1 &
  done
  local leader=""
  for k in $(seq 1 300); do
    for i in 1 2 3; do
      if echo srvr | nc -q1 127.0.0.1 $((2180 + i)) | grep -q "Mode: leader"; then leader=$i; break 2; fi
    done
    sleep 0.2
  done
  [ -n "$leader" ] || { echo "no zookeeper leader" >&2; return 1; }
  echo 127.0.0.1:$((2180 + leader))
}

start_nats() {
  local d=$1
  for i in 1 2 3; do
    cat > $d/n$i.conf <<CFG
server_name: n$i
port: $((4221 + i))
http_port: $((8221 + i))
jetstream { store_dir: "$d/n$i", sync_interval: always }
cluster { name: c, port: $((6221 + i)), routes: [nats://127.0.0.1:6222, nats://127.0.0.1:6223, nats://127.0.0.1:6224] }
CFG
    ${WRAP:+$WRAP -o $d/strace.n${i:-1}} nats-server -c $d/n$i.conf > $d/n$i.log 2>&1 &
  done
  wait_for "curl -sf 127.0.0.1:8222/healthz?js-enabled-only=true" || return 1
  wait_for "curl -sf 127.0.0.1:8222/jsz | grep -q \"leader\""  || return 1
  echo 127.0.0.1:4222
}

# UNIX=1 puts the peer and client sockets of konsensd under the run
# directory, as unix:<path>. A path stays under 108 bytes (sun_path).
konsensd_peer() { if [ "${UNIX:-0}" = 1 ]; then echo "unix:$1/p$2.sock"; else echo "127.0.0.1:$((7150 + $2))"; fi; }
konsensd_client() { if [ "${UNIX:-0}" = 1 ]; then echo "unix:$1/c$2.sock"; else echo "127.0.0.1:$((7160 + $2))"; fi; }
# konsensd_curl <dir> <pid> <curl options...> <path>
konsensd_curl() {
  local d=$1 p=$2; shift 2
  local args=("$@") path=${!#}
  unset 'args[${#args[@]}-1]'
  if [ "${UNIX:-0}" = 1 ]; then
    curl --unix-socket "$d/c$p.sock" "${args[@]}" "http://localhost$path"
  else
    curl "${args[@]}" "127.0.0.1:$((7160 + p))$path"
  fi
}

start_konsensd() {
  local d=$1
  for i in 1 2 3; do
    mkdir -p $d/n$i
    {
      echo '[cluster]'; echo 'name = "bench"'; echo 'configuration_id = 1'
      echo '[node]'; echo "pid = $i"; echo "data_directory = \"$d/n$i\""
      echo "peer_address = \"$(konsensd_peer $d $i)\""; echo "client_address = \"$(konsensd_client $d $i)\""
      [ -n "${NODE_EXTRA:-}" ] && printf "%b\n" "$NODE_EXTRA"
      for p in 1 2 3; do [ $p = $i ] && continue; echo '[[peers]]'; echo "pid = $p"; echo "address = \"$(konsensd_peer $d $p)\""; done
    } > $d/n$i.toml
    ${WRAP:+$WRAP -o $d/strace.n${i:-1}} $B/${KONSENSD:-konsensd} --config $d/n$i.toml --first-start > $d/n$i.log 2>&1 &
  done
  local leader=""
  for k in $(seq 1 300); do
    leader=$(grep -ho "node [0-9] is leader in accept" $d/n*.log | tail -1 | awk '{print $2}')
    [ -n "$leader" ] && break; sleep 0.2
  done
  [ -n "$leader" ] || { echo "no konsensd leader" >&2; return 1; }
  local probes=0
  until konsensd_curl $d $leader -sf -m 2 -X PUT --data-binary probe /kv/probe-$probes >/dev/null; do
    probes=$((probes + 1)); [ $probes -ge 60 ] && { echo "konsensd: no answer to a PUT after $probes probes" >&2; return 1; }
  done
  echo "konsensd: probes before the first answer=$probes" >&2
  local seen="" now="" still=0
  for k in $(seq 1 ${STABLE_LIMIT:-300}); do
    now=$(cat $d/n1.log $d/n2.log $d/n3.log | grep -c " is ")
    if [ "$now" = "$seen" ]; then still=$((still + 1)); else still=0; seen=$now; fi
    [ $still -ge ${STABLE_TICKS:-30} ] && break
    sleep 0.1
  done
  echo "konsensd: state lines $seen, unchanged for $still ticks of 0.1 s" >&2
  for i in 1 2 3; do grep -q "node $i is leader in accept" <(grep "node $i is " $d/n$i.log | tail -1) && leader=$i; done
  echo "konsensd: the client goes to node $leader" >&2
  konsensd_client $d $leader
}

stop_all() {
  local names="etcd rqlited dqlite-demo nats-server java konsensd_.*"
  for n in $names; do pkill -TERM -u "$(id -u)" -x $n; done
  for i in $(seq 1 100); do
    local any=0
    for n in $names; do pgrep -u "$(id -u)" -x $n >/dev/null && any=1; done
    [ $any = 0 ] && return 0; sleep 0.1
  done
  for n in $names; do pkill -KILL -u "$(id -u)" -x $n; done
}
