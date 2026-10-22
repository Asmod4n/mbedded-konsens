#!/bin/bash
set -euo pipefail

image=localhost/cxx-matrix
compiler=g++-16
c_compiler=gcc-16
build_jobs=2
bench_user=bench
bench_uid=1001
default_samples=31
default_rounds=7
binaries=(konsens_bench konsens_bench_aa)
cases=(
    "crc32c 64" "crc32c 1024" "crc32c 65536"
    "encode_accept_decide 0" "decode_accept_decide 0" "encode_decide 0" "decode_decide 0"
    "handle_accept_decide 0" "handle_decide 0"
    "commit_memory 64" "commit_memory 1024" "commit_memory 65536"
    "commit_lmdb 64" "commit_lmdb 1024" "commit_lmdb 65536"
    "write_memory 64" "write_memory 1024" "write_memory 65536"
    "write_lmdb 64" "write_lmdb 1024" "write_lmdb 65536"
    "read_memory 0" "read_lmdb 0"
    "write_batch_memory 64" "write_batch_memory 1024" "write_batch_memory 65536"
    "write_batch_lmdb 64" "write_batch_lmdb 1024" "write_batch_lmdb 65536"
    "lmdb_put 64" "lmdb_put 1024" "lmdb_put 65536"
    "lmdb_put_like_storage 64" "lmdb_put_like_storage 1024" "lmdb_put_like_storage 65536"
    "lmdb_get 64" "lmdb_get 1024" "lmdb_get 65536"
    "get_entries_lmdb 64" "get_entries_lmdb 1024" "get_entries_lmdb 65536"
    "write_memory_threads 64" "write_memory_threads 1024" "write_memory_threads 65536"
    "write_lmdb_threads 64" "write_lmdb_threads 1024" "write_lmdb_threads 65536"
    "read_memory_threads 0" "read_lmdb_threads 0"
    "write_batch_memory_threads 64" "write_batch_memory_threads 1024" "write_batch_memory_threads 65536"
    "write_batch_lmdb_threads 64" "write_batch_lmdb_threads 1024" "write_batch_lmdb_threads 65536"
)

usage() {
    echo "Usage: bench/run.sh <work_dir> <ccache_dir> [<samples> [<rounds> [<result_dir>]]]" >&2
    echo "The work directory holds the build, the LMDB stores and the logs." >&2
    echo "The result file goes to <result_dir>, by default bench/results." >&2
    exit 1
}

host() {
    (($# >= 2 && $# <= 5)) || usage
    local work ccache samples=${3:-$default_samples} rounds=${4:-$default_rounds} repo results stamp commit dirty
    work=$(realpath -m "$1")
    ccache=$(realpath -m "$2")
    repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    results=$(realpath -m "${5:-$repo/bench/results}")
    stamp=$(date -u +%Y-%m-%d-%H%M%SZ)
    mkdir -p "$work/build" "$work/lmdb" "$ccache" "$results"
    find "$work/lmdb" -mindepth 1 -delete
    commit=$(git -C "$repo" rev-parse HEAD)
    git -C "$repo" status --porcelain > "$work/dirty-paths-$stamp.txt"
    if [[ -s $work/dirty-paths-$stamp.txt ]]; then dirty=dirty; else dirty=clean; fi
    podman --version > "$work/podman-version-$stamp.txt"
    podman image inspect --format "$image {{.Id}} created {{.Created}}" "$image" > "$work/image-$stamp.txt"
    podman run --rm -v "$repo:/src:ro" -v "$work:/work" -v "$work/lmdb:/lmdb" -v "$ccache:/ccache" \
        -e CCACHE_DIR=/ccache "$image" bash /src/bench/run.sh --inside "$samples" "$rounds" "$commit" "$dirty" "$stamp"
    cp "$work/result-$stamp.txt" "$results/$stamp.txt"
    echo "$results/$stamp.txt"
}

build() {
    local stamp=$1
    cmake -S /src -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG" \
        -DCMAKE_CXX_COMPILER=$compiler -DCMAKE_C_COMPILER=$c_compiler -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DKONSENS_BENCH=ON -Wno-dev \
        > "/work/cmake-$stamp.log" 2>&1 || { tail -n 40 "/work/cmake-$stamp.log" >&2; exit 1; }
    cmake --build /work/build --target "${binaries[@]}" -j $build_jobs \
        > "/work/build-$stamp.log" 2>&1 || { tail -n 40 "/work/build-$stamp.log" >&2; exit 1; }
}

context() {
    local commit=$1 dirty=$2 stamp=$3 b
    echo "# commit $commit $dirty"
    sed 's/^/# dirty path /' "/work/dirty-paths-$stamp.txt"
    echo "# podman $(cat "/work/podman-version-$stamp.txt")"
    echo "# container podman, image $(cat "/work/image-$stamp.txt")"
    echo "# os $(. /etc/os-release && echo "$PRETTY_NAME")"
    echo "# compiler $($compiler --version | head -n 1)"
    echo "# build type Release, CMAKE_CXX_FLAGS_RELEASE $(grep -E '^CMAKE_CXX_FLAGS_RELEASE:' /work/build/CMakeCache.txt | cut -d= -f2-)"
    echo "# compile command $(ninja -C /work/build -t commands konsens_bench | grep -F -- '-c /src/bench/konsens_bench.cpp')"
    echo "# instruction set $($compiler -O2 -Q --help=target | awk '$1 == "-march=" || $1 == "-mtune=" {printf "%s%s ", $1, $2}')"
    echo "# instruction set macros $(echo | $compiler -O2 -dM -E -x c++ - | grep -oE '__(MMX|SSE[0-9_]*|AVX[0-9A-Z_]*|POPCNT|BMI2?|FMA|CRC32)__' | sort -u | tr '\n' ' ')"
    for b in "${binaries[@]}"; do
        echo "# binary $b sha256 $(sha256sum "/work/build/$b" | cut -c1-16) needs $(objdump -T "/work/build/$b" | grep -oE '(GLIBCXX|CXXABI|GLIBC)_[0-9.]+' | sort -uV | tr '\n' ' ')"
    done
    echo "# binary comment $(readelf -p .comment /work/build/konsens_bench | grep -oE 'GCC: .*' | sort -u | tr '\n' ' ')"
    echo "# libraries $(ldd /work/build/konsens_bench | awk '{printf "%s %s; ", $1, $3}')"
    echo "# packages $(dpkg-query -W -f='${Package} ${Version}; ' libstdc++6 libc6 liblmdb0 || true)"
    echo "# kernel $(uname -r) $(uname -v)"
    if [[ -r /sys/kernel/sched_ext/state ]]; then
        echo "# scheduler sched_ext $(cat /sys/kernel/sched_ext/state)"
    else
        echo "# scheduler no /sys/kernel/sched_ext"
    fi
    echo "# virtual machine hypervisor flag $(grep -m 1 -cw hypervisor /proc/cpuinfo || true)"
    echo "# processor $(grep -m 1 '^model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//') nproc $(nproc)"
    echo "# cpu flags sha256 $(grep -m 1 '^flags' /proc/cpuinfo | sha256sum | cut -c1-16)"
    echo "# user $bench_user uid $bench_uid through setpriv, nice $(nice), threads 1 in a serial case, 4 in a case whose name ends in _threads (the main thread and three node threads, at most 3 runnable)"
    echo "# lmdb mount $(awk '$2 == "/lmdb"' /proc/mounts)"
    echo "# loadavg at start $(cat /proc/loadavg)"
}

run_one() {
    local round=$1 name=$2 payload=$3 binary=$4 samples=$5
    local directory=/lmdb/$round-$name-$payload-$binary other running steal_before steal_after line status=0
    local -a arguments=("$name" "$samples" "$payload")
    if [[ $name == *lmdb* ]]; then
        mkdir "$directory"
        chown "$bench_uid:$bench_uid" "$directory"
        arguments+=("$directory")
    fi
    other=$(pgrep -c -u "$bench_user" || true)
    running=$(awk '$1 == "procs_running" {print $2}' /proc/stat)
    steal_before=$(awk '$1 == "cpu" {print $9}' /proc/stat)
    line=$(setpriv --reuid="$bench_uid" --regid="$bench_uid" --clear-groups "/work/build/$binary" "${arguments[@]}") \
        || status=$?
    steal_after=$(awk '$1 == "cpu" {print $9}' /proc/stat)
    echo "round=$round binary=$binary nice=$(nice) other_processes_of_user=$other procs_running=$running steal_ticks=$((steal_after - steal_before)) exit=$status $line"
    rm -rf "$directory"
}

rounds() {
    local samples=$1 rounds=$2 round item name payload binary
    local -a order binary_order
    for ((round = 1; round <= rounds; round++)); do
        if ((round % 2 == 0)); then
            mapfile -t order < <(printf '%s\n' "${cases[@]}" | tac)
            mapfile -t binary_order < <(printf '%s\n' "${binaries[@]}" | tac)
        else
            order=("${cases[@]}")
            binary_order=("${binaries[@]}")
        fi
        for item in "${order[@]}"; do
            read -r name payload <<< "$item"
            for binary in "${binary_order[@]}"; do
                run_one "$round" "$name" "$payload" "$binary" "$samples"
            done
        done
    done
}

summary() {
    awk -v main="${binaries[0]}" -v aa="${binaries[1]}" '
    BEGIN { payload_count = split("64 1024 65536", payloads, " ") }
    function shown(table, key) { return (key in table && table[key] != "") ? table[key] : "-" }
    function cell(key) { return (key in median) ? sprintf("%.2f", median[key]) : "-" }
    function middle(key, binary,    n, i, j, t, a) {
        n = count[key, binary]
        for (i = 1; i <= n; i++) a[i] = value[key, binary, i] + 0
        for (i = 2; i <= n; i++) { t = a[i]; for (j = i - 1; j >= 1 && a[j] > t; j--) a[j + 1] = a[j]; a[j + 1] = t }
        low = a[1]; high = a[n]
        if (n % 2 == 1) return a[(n + 1) / 2]
        return (a[n / 2] + a[n / 2 + 1]) / 2
    }
    {
        split("", field)
        for (i = 1; i <= NF; i++) { p = index($i, "="); if (p > 0) field[substr($i, 1, p - 1)] = substr($i, p + 1) }
        if (field["exit"] != 0 || !("ns_per_op_median" in field)) { failed++; next }
        key = field["case"] " " field["payload_bytes"]
        if (!(key in seen)) { seen[key] = 1; keys[++key_count] = key }
        n = ++count[key, field["binary"]]
        value[key, field["binary"], n] = field["ns_per_op_median"]
        if (field["binary"] == main) {
            messages_per_op[key] = field["messages_per_op"]
            writes_per_op[key] = field["write_atomically_per_op"]
            writes_per_node[key] = field["write_atomically_per_node_per_op"]
            leader_ns[key] = field["leader_ns_per_op"]; follower_ns[key] = field["follower_ns_per_op"]
        }
    }
    END {
        print "## A/A difference of each case"
        print ""
        print "The median of the round medians of each binary, in ns per operation."
        print ""
        print "| case | payload B | " main " | " aa " | A/A |"
        print "|---|---|---|---|---|"
        largest = 0
        for (k = 1; k <= key_count; k++) {
            split(keys[k], part, " ")
            m = middle(keys[k], main); a = middle(keys[k], aa)
            d = m > 0 ? (a - m) / m * 100 : 0
            if ((d < 0 ? -d : d) > largest) largest = d < 0 ? -d : d
            printf "| %s | %s | %.2f | %.2f | %+.1f %% |\n", part[1], part[2], m, a, d
        }
        print ""
        printf "Largest A/A difference: %.1f %%. This is the noise floor of the run.\n", largest
        print ""
        print "## Result, " main
        print ""
        print "ns per operation: the median of the round medians. Spread: the lowest and the highest round median."
        print ""
        print "Leader and follower: the commit time of the leader and of the slower follower, in ns per operation,"
        print "from the last raw line of the case."
        print ""
        print "| case | payload B | ns per op | spread | messages per op | write_atomically per op | per node | leader ns | follower ns |"
        print "|---|---|---|---|---|---|---|---|---|"
        for (k = 1; k <= key_count; k++) {
            split(keys[k], part, " ")
            m = middle(keys[k], main)
            median[keys[k]] = m
            printf "| %s | %s | %.2f | %.2f-%.2f | %s | %s | %s | %s | %s |\n", part[1], part[2], m, low, high, \
                shown(messages_per_op, keys[k]), shown(writes_per_op, keys[k]), shown(writes_per_node, keys[k]), \
                shown(leader_ns, keys[k]), shown(follower_ns, keys[k])
        }
        print ""
        print "## Each library case beside its LMDB baseline"
        print ""
        print "ns per operation, " main ". Commits: the LMDB commits in series of one write_lmdb operation."
        print "lmdb_put x commits: lmdb_put times that number."
        print ""
        print "| payload B | lmdb_put | lmdb_put_like_storage | commit_lmdb | write_lmdb | commits | lmdb_put x commits | write_lmdb_threads | write_batch_lmdb | write_batch_lmdb_threads |"
        print "|---|---|---|---|---|---|---|---|---|---|"
        for (p = 1; p <= payload_count; p++) {
            b = payloads[p]
            n = shown(writes_per_op, "write_lmdb " b)
            series = ("lmdb_put " b) in median && n != "-" ? sprintf("%.2f", median["lmdb_put " b] * n) : "-"
            printf "| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n", b, cell("lmdb_put " b), \
                cell("lmdb_put_like_storage " b), cell("commit_lmdb " b), cell("write_lmdb " b), n, series, \
                cell("write_lmdb_threads " b), cell("write_batch_lmdb " b), cell("write_batch_lmdb_threads " b)
        }
        print ""
        print "| payload B | lmdb_get | get_entries_lmdb |"
        print "|---|---|---|"
        for (p = 1; p <= payload_count; p++) {
            b = payloads[p]
            printf "| %s | %s | %s |\n", b, cell("lmdb_get " b), cell("get_entries_lmdb " b)
        }
        print ""
        printf "Processes that failed: %d.\n", failed
    }'
}

header() {
    local samples=$1 rounds=$2
    cat <<EOF
# Bench: each part of a write

What: konsens_bench, built from bench/konsens_bench.cpp at the commit in
the context below. Each process runs one case with one payload. It takes
one warm-up sample and then $samples samples. A sample is a fixed number of
operations (ops_per_sample in each raw line). The time of one operation is
the time of the sample divided by that number. Each process prints the
median, the minimum and the maximum of its samples.

Cases:
- crc32c: crc32c::Crc32c of Google crc32c over one payload, taken in turn from 16 buffers.
  The operations are independent, so the number is throughput.
- encode_accept_decide, decode_accept_decide: one AcceptDecide with 64
  entries of 1024 B. encode_decide, decode_decide: one Decide. A decode
  case copies the bytes before the timer starts.
- handle_accept_decide, handle_decide: node::handle_incoming and
  node::commit for each batch of at most konsens::max_batch_size messages
  of the sample, at a follower in
  (Follower, Accept), over a store that counts the payload bytes and keeps
  no entry. The messages are encoded and decoded before the timer starts.
- commit_memory, commit_lmdb: write_atomically of one append_entries with
  one entry, on a fresh memory_storage or persistent_storage for each
  sample.
- write_memory, write_lmdb: three nodes in one process. Each message goes
  through encode and decode, and the network is a queue. One
  client_request at a follower, timed from the call to its reply.
- read_memory, read_lmdb: one client_query at a follower, timed from the
  call to its reply. read_lmdb answers from the state machine in memory
  after one read-index round, so it stands beside read_memory, not
  beside lmdb_get.
- write_batch_memory, write_batch_lmdb: 64 client_request calls at a
  follower before the first round, timed until the last reply, divided
  by 64.

- lmdb_put: LMDB alone. One mdb_txn_begin, one mdb_put of the payload
  under a big-endian 8-byte key that rises by one, with flags 0, and one
  mdb_txn_commit. The env opens as persistent_storage::open opens it:
  env flags 0, so each commit syncs, mode 0600, maxdbs 2, one named
  database, the map size of commit_lmdb. A fresh env for each sample,
  opened before the timer starts.
- lmdb_put_like_storage: as lmdb_put, but each transaction calls
  mdb_stat once and reserves the payload plus the record overhead of
  the library (the kind and the CRC) with MDB_APPEND | MDB_RESERVE, as
  write_atomically does. It copies the record into the reserved bytes.
- lmdb_get: LMDB alone. One read-only mdb_txn_begin, one mdb_get of a
  key that was written before the timer starts, a comparison of the
  value with the payload, and one mdb_txn_abort.
- get_entries_lmdb: persistent_storage::get_entries(0, 1) of an entry
  that was written before the timer starts, and a comparison of the
  payload. It stands beside lmdb_get.

write_lmdb runs the three nodes in one process. Its time holds the
commits of all three nodes one after the other: write_atomically_per_op
LMDB commits in series, the number as measured. A cluster on three
machines commits on the followers at the same time.

leader_ns_per_op and follower_ns_per_op are the time spent in
node::commit at the leader and at the slower of the two followers, for
each operation. The latency on three machines is about the leader time
plus the slowest follower time plus the network. This bench does not
measure that latency. A cluster case fails when the leader changes
during the samples.

- write_memory_threads, write_lmdb_threads, read_memory_threads,
  read_lmdb_threads, write_batch_memory_threads,
  write_batch_lmdb_threads: the same operations with each node on its
  own thread. Each node has its own LMDB env in its own directory; the
  three envs share one disk. The election, the registration of the
  client and the write before the reads run on the serial harness
  before the threads start. A node thread takes the messages and the
  client calls that wait in its inbox, at most konsens::max_batch_size,
  calls handle_incoming once and commit once, and puts the encoded
  messages into the inbox of each target. The main thread puts the
  calls into the inbox of the follower and waits for the replies. After
  the last sample it waits until no message is in flight, stops and
  joins the threads, and checks that each node has the same log length
  and the same decided index and that the leader did not change. The
  memory cases mostly measure the wakeup of the threads.

The serial harness calls tick at each node in each round of the timed
part, so its rounds also carry heartbeats and resends. The threaded
harness calls no tick. The parts do not add up to a write.
messages_per_op gives the messages of each operation,
write_atomically_per_op the calls of write_atomically at all nodes and
write_atomically_per_node_per_op the calls at node 1, 2 and 3. These
numbers and the commit times count all samples, the warm-up sample
too. log_len_first and log_len_last give the log length of the
follower before the first sample and after the last sample.

write_lmdb_threads stands beside lmdb_put times the number of LMDB
commits in series of write_lmdb.

Checks: crc32c compares the CRC of "123456789" with the check value
e3069283. The encode and decode cases compare each decoded message with
the encoded one. handle_accept_decide compares the bytes counted with the
bytes sent; handle_decide compares the decided index with the last
Decide. The commit cases compare the log length with the number of
commits after each sample. The cluster cases compare each counter reply
with the number of writes. lmdb_put and lmdb_put_like_storage compare
the number of entries of mdb_stat with the number of puts after each
sample. lmdb_get and get_entries_lmdb compare each value with the
payload. A process whose check fails prints the
failure and exits with 1; its raw line shows the exit status.

Run: $rounds rounds. Each round runs every case with every payload in
konsens_bench and in konsens_bench_aa. The order of the cases and the
order of the two binaries turn around each second round.
konsens_bench_aa is the same source built a second time. The difference
between the two binaries is the A/A difference of the case.

User and priority: each process runs as the user bench (uid 1001, not
root), started by setpriv. Not pinned. Each raw line gives nice,
other_processes_of_user (the processes of the user bench before the
start), procs_running of /proc/stat before the start, and the steal ticks
of /proc/stat during the process.

EOF
}

inside() {
    local samples=$1 rounds=$2 commit=$3 dirty=$4 stamp=$5
    build "$stamp"
    getent passwd "$bench_user" > /dev/null || useradd -u "$bench_uid" -M "$bench_user"
    chown "$bench_uid:$bench_uid" /lmdb
    context "$commit" "$dirty" "$stamp" > "/work/context-$stamp.txt"
    rounds "$samples" "$rounds" > "/work/raw-$stamp.txt"
    echo "# loadavg at end $(cat /proc/loadavg)" >> "/work/context-$stamp.txt"
    {
        header "$samples" "$rounds"
        echo "## Context"
        echo ""
        cat "/work/context-$stamp.txt"
        echo ""
        summary < "/work/raw-$stamp.txt"
        echo ""
        echo "## Raw lines"
        echo ""
        cat "/work/raw-$stamp.txt"
    } > "/work/result-$stamp.txt"
}

if [[ ${1-} == --inside ]]; then
    shift
    inside "$@"
else
    host "$@"
fi
