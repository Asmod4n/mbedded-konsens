#!/bin/bash
# Three konsensd processes on loopback, with curl as the client (plan 7.5).
# The test exists because only the running program shows that the loop,
# the frames, HTTP and the sessions work together. The third argument
# names the form of the addresses: tcp (the default) or unix. The unix
# form also shows that a restart after a clean stop and after SIGKILL
# binds the socket file again. The fourth argument sync sets
# sync_by_the_caller, so that the test also runs the path on which the
# node holds its messages until the sync of the caller.
set -u

konsensd=$1
base=${2:-17150}
form=${3:-tcp}
sync=${4:-}
dir=$(mktemp -d)
failed=0
declare -A pids

if [ "$form" = unix ]; then
    peer_address() { echo "unix:$dir/peer$1.sock"; }
    client_address() { echo "unix:$dir/client$1.sock"; }
    client_url() { echo "http://localhost"; }
    client_socket() { echo "--unix-socket $dir/client$1.sock"; }
else
    peer_address() { echo "127.0.0.1:$((base + $1))"; }
    client_address() { echo "127.0.0.1:$((base + 10 + $1))"; }
    client_url() { echo "http://127.0.0.1:$((base + 10 + $1))"; }
    client_socket() { :; }
fi

write_config() {
    local p=$1 q
    {
        printf '[cluster]\nname = "test"\nconfiguration_id = 1\n\n[node]\n'
        printf 'pid = %s\ndata_directory = "%s/data%s"\n' "$p" "$dir" "$p"
        printf 'peer_address = "%s"\n' "$(peer_address "$p")"
        printf 'client_address = "%s"\n' "$(client_address "$p")"
        [ "$sync" = sync ] && printf 'sync_by_the_caller = true\n'
        for q in 1 2 3; do
            [ "$q" = "$p" ] && continue
            printf '\n[[peers]]\npid = %s\naddress = "%s"\n' "$q" "$(peer_address "$q")"
        done
    } > "$dir/node$p.toml"
}

start() {
    local p=$1
    shift
    "$konsensd" --config "$dir/node$p.toml" "$@" 2>> "$dir/log$p" &
    pids[$p]=$!
}

# curl exits with 7 when the connect is refused.
refuses() {
    curl -s -o /dev/null --max-time 1 "$@"
    [ $? = 7 ]
}

# After the exit of a node, its listeners can accept for a short time,
# so a start at once can find its addresses in use. The test waits until
# both addresses refuse a connect.
released() {
    local p=$1 i
    for i in $(seq 50); do
        if [ "$form" = unix ]; then
            refuses --unix-socket "$dir/peer$p.sock" http://localhost/ &&
                refuses --unix-socket "$dir/client$p.sock" http://localhost/ && return 0
        else
            refuses "http://$(peer_address "$p")/" && refuses "$(client_url "$p")/" && return 0
        fi
        sleep 0.1
    done
    return 1
}

# A node that does not end within its bound gets SIGKILL, so a hung node
# fails the test and does not hang it.
ends_within() {
    local pid=$1 seconds=$2
    timeout "$seconds" tail --pid="$pid" -f /dev/null && return 0
    kill -KILL "$pid"
    return 1
}

stop() {
    local p=$1 status
    kill -TERM "${pids[$p]}"
    ends_within "${pids[$p]}" 30
    wait "${pids[$p]}"
    status=$?
    unset "pids[$p]"
    released "$p" || return 1
    return $status
}

check() {
    local what=$1
    shift
    if "$@"; then
        echo "ok ($SECONDS s): $what"
    else
        echo "FAILED: $what"
        failed=1
    fi
}

status_of() {
    curl -s --max-time 5 -o "$dir/body" -w '%{http_code}' "$@"
}

put() { status_of $(client_socket "$1") -X PUT --data-binary "$3" "$(client_url "$1")/kv/$2"; }
get() { status_of $(client_socket "$1") "$(client_url "$1")/kv/$2"; }

# A write waits for an elected leader: a 503 asks the client to try again.
put_until_stored() {
    local i
    for i in $(seq 50); do
        [ "$(put "$@")" = 204 ] && return 0
        sleep 0.2
    done
    return 1
}

# Sends bytes as they are to the peer port of a node. curl has no form
# that sends a NUL, so the unix form takes only bytes without one.
send_to_the_peer_port() {
    local p=$1 bytes=$2 encoded=$3
    if [ "$form" = unix ]; then
        curl -s --max-time 2 --unix-socket "$dir/peer$p.sock" "gopher://localhost/_$encoded" > /dev/null
    else
        exec 3<> "/dev/tcp/127.0.0.1/$((base + p))"
        printf "$bytes" >&3
        sleep 0.2
        exec 3>&-
    fi
}

alive() { kill -0 "${pids[$1]}"; }

holds() {
    [ "$(get "$1" "$2")" = 200 ] && [ "$(cat "$dir/body")" = "$3" ]
}

holds_soon() {
    local i
    for i in $(seq 50); do
        holds "$@" && return 0
        sleep 0.2
    done
    return 1
}

leader() {
    local p
    for p in 1 2 3; do
        [ -n "${pids[$p]:-}" ] || continue
        if grep "is " "$dir/log$p" | tail -1 | grep -q "is leader in accept"; then
            echo "$p"
            return
        fi
    done
}

sessions_at() {
    sqlite3 -readonly "$dir/data$1/log/konsens.db" "SELECT count(*) FROM session"
}

end() {
    local p
    for p in "${!pids[@]}"; do
        kill -TERM "${pids[$p]}"
    done
    for p in "${!pids[@]}"; do
        ends_within "${pids[$p]}" 30 || failed=1
    done
    wait
    if [ "$failed" != 0 ]; then
        for p in 1 2 3; do
            echo "--- log of node $p"
            cat "$dir/log$p"
        done
    fi
    rm -r "$dir"
    exit "$failed"
}

for p in 1 2 3; do
    write_config "$p"
    start "$p" --first-start
done

# Each node takes writes and reads, and sees what another node wrote.
check "a write at node 1" put_until_stored 1 greeting hello
check "a read at node 2 sees it" holds_soon 2 greeting hello
check "a read at node 3 sees it" holds_soon 3 greeting hello
check "a write at node 3" put_until_stored 3 a one
check "a read at node 1 sees it" holds_soon 1 a one
check "a delete at node 2" test "$(status_of $(client_socket 2) -X DELETE "$(client_url 2)/kv/greeting")" = 204
check "a read after the delete gives 404" test "$(get 1 greeting)" = 404
check "an empty value round trip" put_until_stored 2 empty ""
check "the empty value reads back" holds_soon 1 empty ""

# The node closes a peer connection that sends a frame head above the
# bound, or a hello that does not decode, inside the handler of the
# receive. The connection must stay alive until that handler returns,
# and the node must keep serving.
send_to_the_peer_port 1 '\xff\xff\xff\xff' '%FF%FF%FF%FF'
check "node 1 lives after a frame head above the bound" alive 1
if [ "$form" = tcp ]; then
    send_to_the_peer_port 1 '\x00\x00\x00\x01x' ''
    check "node 1 lives after a hello that does not decode" alive 1
fi
check "node 1 serves a write after the refused peers" put_until_stored 1 after-refused yes

# A clean stop keeps the vote of a node.
check "node 2 stops cleanly" stop 2
start 2
check "node 2 votes again and serves a write" put_until_stored 2 after-restart yes
check "node 1 reads it" holds_soon 1 after-restart yes

# A stop of the leader during writes loses no write that got 204.
l=$(leader)
check "a leader is known" test -n "$l"
other=$(( l % 3 + 1 ))
: > "$dir/acknowledged"
(
    for i in $(seq 200); do
        [ "$(put "$other" "w$i" "v$i")" = 204 ] && echo "$i" >> "$dir/acknowledged"
    done
) &
writer=$!
sleep 0.5
check "the leader stops cleanly during writes" stop "$l"
wait "$writer"
start "$l"
lost=0
for i in $(cat "$dir/acknowledged"); do
    holds_soon "$l" "w$i" "v$i" || lost=1
done
check "every acknowledged write is there ($(wc -l < "$dir/acknowledged") of 200)" test "$lost" = 0

# 1000 connections through a follower, each with one write. Each closed
# connection expires its session, so the session count goes back.
l=$(leader)
follower=$(( l % 3 + 1 ))
before=$(sessions_at "$l")
for i in $(seq 1000); do
    put "$follower" "c$i" "x" > /dev/null
done
for i in $(seq 50); do
    [ "$(sessions_at "$l")" = "$before" ] && break
    sleep 0.2
done
check "the session count of the leader goes back to $before" test "$(sessions_at "$l")" = "$before"

# A follower that is stopped during many writes catches up after its start.
l=$(leader)
follower=$(( l % 3 + 1 ))
check "follower $follower stops cleanly" stop "$follower"
check "10000 writes on one connection" test "$(curl -s --max-time 120 -o /dev/null -w '%{http_code}\n' \
    $(client_socket "$l") -X PUT --data-binary bulk "$(client_url "$l")/kv/bulk[1-10000]" | sort -u)" = 204
start "$follower"
check "the follower reads the last write" holds_soon "$follower" bulk10000 bulk

# All three stop and start again: the data is there.
for p in 1 2 3; do
    check "node $p stops cleanly" stop "$p"
done
for p in 1 2 3; do
    start "$p"
done
check "the data is there after a stop of all" holds_soon 3 a one

# A start with --first-start after a clean stop is a clean start: the
# clean-stop mark decides, and the flag changes nothing.
check "node 3 stops cleanly" stop 3
start 3 --first-start
check "--first-start after a clean stop serves a write" put_until_stored 3 after-flag yes

# Owner 2026-11-09: a SIGTERM during a repair stops the node cleanly at
# once. Before, it waited for the end of the repair, and a node whose peers
# were gone waited until SIGKILL and was retired (Jepsen, rw-register,
# node 2). The repair that the stop interrupts does not count, so the next
# start repairs by itself. The changed byte is the first byte of the
# encoded entry, so the CRC of the record fails; a changed byte in the
# tail is repaired at the start and opens no repair. Node 2 starts alone,
# so no peer answers its repair until nodes 1 and 3 start.
for p in 1 2 3; do
    check "node $p stops cleanly" stop "$p"
done
sqlite3 "$dir/data2/log/konsens.db" "UPDATE log SET record = unhex(
    CASE substr(hex(record), 1, 2) WHEN 'FF' THEN 'FE' ELSE 'FF' END || substr(hex(record), 3))
    WHERE idx = (SELECT min(idx) FROM log)"
check "the changed record is a BLOB" test "$(sqlite3 -readonly "$dir/data2/log/konsens.db" \
    "SELECT typeof(record) FROM log WHERE idx = (SELECT min(idx) FROM log)")" = blob
start 2
count_soon() {
    local i
    for i in $(seq 50); do
        [ "$(grep -c "$2" "$3")" = "$1" ] && return 0
        sleep 0.2
    done
    return 1
}
grep_soon() {
    local i
    for i in $(seq 50); do
        grep -q "$1" "$2" && return 0
        sleep 0.2
    done
    return 1
}
check "node 2 repairs its store" grep_soon "node 2 repairs its store" "$dir/log2"
check "node 2 did not start retired" test -z "$(grep "started.*retired" "$dir/log2")"
check "a read at node 2 during the repair answers 503" test "$(get 2 a)" = 503
kill -TERM "${pids[2]}"
check "node 2 ends within 30 s after SIGTERM, with no peer" ends_within "${pids[2]}" 30
wait "${pids[2]}"
check "node 2 exits with 0" test $? = 0
unset "pids[2]"
check "node 2 stopped cleanly" test "$(tail -1 "$dir/log2")" = "konsensd: node 2 stopped cleanly"
check "the addresses of node 2 are released" released 2
start 1
start 3
start 2
check "node 2 repairs its store again" count_soon 2 "node 2 repairs its store" "$dir/log2"
check "node 2 does not wait for the operator" test -z "$(grep "waits for the operator" "$dir/log2")"
check "node 2 ended its repair" grep_soon "node 2 ended its repair" "$dir/log2"
check "node 2 serves a write after its repair" put_until_stored 2 after-repair yes
check "node 1 reads it" holds_soon 1 after-repair yes

# Plan cluster-stop 4.8: one call on the control socket of one node shuts
# the whole cluster down. The entry goes through the log, every node
# ends with 0 after a clean stop, and every node starts clean.
check "POST /cluster/shutdown at node 1 answers 200" test "$(status_of --unix-socket \
    "$dir/data1/control/control.sock" -X POST http://localhost/cluster/shutdown)" = 200
check "the answer names no peer that did not acknowledge" grep -q '"peers_that_did_not_acknowledge":\[\]' \
    "$dir/body"
for p in 1 2 3; do
    check "node $p ends after the shutdown of the cluster" ends_within "${pids[$p]}" 30
    wait "${pids[$p]}"
    check "node $p exits with 0 after the shutdown of the cluster" test $? = 0
    unset "pids[$p]"
    check "node $p stopped cleanly after the shutdown of the cluster" \
        test "$(tail -1 "$dir/log$p")" = "konsensd: node $p stopped cleanly"
    check "the addresses of node $p are released after the shutdown" released "$p"
done
for p in 1 2 3; do
    start "$p"
done
check "a write after the shutdown of the cluster" put_until_stored 3 after-shutdown yes
check "a read at node 1 sees the write after the shutdown" holds_soon 1 after-shutdown yes
for p in 1 2 3; do
    check "node $p started clean after the shutdown of the cluster" \
        test -z "$(grep "started.*retired" "$dir/log$p")"
done

# The operator allows one more repair on the control socket of a node
# (plan of the control socket). A node that votes does not wait for the
# operator, so the call changes nothing there and the node lives.
control() {
    status_of --unix-socket "$dir/data$1/control/control.sock" -X POST http://localhost/repair/allow-one-more
}
# The control socket exists before the store opens, so a start answers on
# it only after its loop runs. curl gives 000 until then.
control_soon() {
    local i status
    for i in $(seq 50); do
        status=$(control "$1")
        [ "$status" != 000 ] && { [ "$status" = "$2" ]; return; }
        sleep 0.2
    done
    return 1
}
check "the control directory has the mode 0700" test "$(stat -c %a "$dir/data1/control")" = 700
check "a POST at node 1, which votes, answers 409" test "$(control 1)" = 409
check "the 409 names its problem type" grep -q "not-waiting-for-the-operator" "$dir/body"
check "node 1 lives after the POST" alive 1

# The first repair used the one repair that runs by itself, so a second
# damage of node 2 waits for the operator. Without the POST, node 2
# waits for ever, and a stop retires it.
check "node 2 stops cleanly before the second damage" stop 2
sqlite3 "$dir/data2/log/konsens.db" "UPDATE log SET record = unhex(
    CASE substr(hex(record), 1, 2) WHEN 'FF' THEN 'FE' ELSE 'FF' END || substr(hex(record), 3))
    WHERE idx = (SELECT min(idx) FROM log)"
start 2
check "node 2 waits for the operator" grep_soon "node 2 waits for the operator;" "$dir/log2"
check "a read at node 2 that waits answers 503" test "$(get 2 a)" = 503
check "a POST at node 2 answers 204" test "$(control 2)" = 204
check "a second POST at node 2 answers 409" test "$(control 2)" = 409
check "node 2 ends its second repair" count_soon 2 "node 2 ended its repair" "$dir/log2"
check "node 2 says once that it waits for the operator" test "$(grep -c "waits for the operator;" "$dir/log2")" = 1
check "node 2 serves a write after its second repair" put_until_stored 2 after-second-repair yes
check "node 1 reads the write after the second repair" holds_soon 1 after-second-repair yes

# Other users must not open the control socket. konsensd does not change
# the mode that the operator set; it refuses to start.
check "node 2 stops cleanly before the change of the mode" stop 2
chmod 0755 "$dir/data2/control"
"$konsensd" --config "$dir/node2.toml" 2>> "$dir/log2"
check "a control directory with the mode 0755 stops the start" test $? != 0
check "the log names the control directory" grep -q "no control directory" "$dir/log2"
chmod 0700 "$dir/data2/control"
start 2
check "node 2 serves a write after the mode is 0700 again" put_until_stored 2 after-mode yes

# SIGKILL retires a node. --first-start on its store ends with an error,
# because the store holds a promise. Without the flag the node starts
# retired: it answers 503, and the other two serve.
kill -KILL "${pids[3]}"
wait "${pids[3]}"
unset "pids[3]"
check "the addresses of the killed node are released" released 3
"$konsensd" --config "$dir/node3.toml" --first-start 2>> "$dir/log3"
check "--first-start on the store of a killed node fails" test $? != 0
start 3
sleep 1
check "the retired node answers 503" test "$(get 3 a)" = 503
check "the other two serve a write" put_until_stored 1 after-kill yes
check "the other two serve a read" holds_soon 2 after-kill yes
check "the retired node exits with 0" stop 3

# A store without its meta record makes the node wait for the operator
# with no repair. A POST there is no call that allow_one_more_repair
# takes, so konsensd answers 409 and lives.
sqlite3 "$dir/data3/log/konsens.db" "DELETE FROM meta WHERE name = 'meta'"
start 3
check "a POST at a node without its meta record answers 409" control_soon 3 409
check "the node without its meta record lives" alive 3
kill -TERM "${pids[3]}"
check "the node without its meta record ends after SIGTERM" ends_within "${pids[3]}" 30
wait "${pids[3]}"
unset "pids[3]"

# A build with AddressSanitizer writes its report to the log of the node.
check "no log holds a report of AddressSanitizer" test -z "$(grep -l AddressSanitizer "$dir"/log*)"

end
