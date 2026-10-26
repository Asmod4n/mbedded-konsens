#!/bin/bash
set -euo pipefail

image=localhost/cxx-matrix-fuse
compiler=g++-16
c_compiler=gcc-16
build_jobs=2
bench_user=bench
bench_uid=1001
default_samples=31
default_rounds=7
main_binary=konsens_bench
aa_binary=konsens_bench_aa

usage() {
    echo "Usage: bench/run.sh <work_dir> <ccache_dir> [<samples> [<rounds> [<result_dir> [<prefixes> [<arm commit>...]]]]]" >&2
    echo "The work directory holds the builds, the LMDB stores and the logs." >&2
    echo "The result file goes to <result_dir>, by default bench/results." >&2
    echo "<prefixes>: a comma-separated list, or 'all'. Only a case whose name starts with one of them runs." >&2
    echo "<arm commit>: a commit of this repository. Its tree is built as one more arm, konsens_bench_<commit>." >&2
    echo "When bench/arms/<commit>.patch exists, the arm keeps its own bench with that patch applied;" >&2
    echo "otherwise the arm takes bench/konsens_bench.cpp of the working tree." >&2
    exit 1
}

host() {
    (($# >= 2)) || usage
    local work ccache samples=${3:-$default_samples} rounds=${4:-$default_rounds} prefixes=${6:-all} repo results stamp
    local commit dirty arm
    local -a arms=("${@:7}")
    work=$(realpath -m "$1")
    ccache=$(realpath -m "$2")
    repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
    results=$(realpath -m "${5:-$repo/bench/results}")
    stamp=$(date -u +%Y-%m-%d-%H%M%SZ)
    mkdir -p "$work/build" "$work/lmdb" "$work/arms" "$ccache" "$results"
    find "$work/lmdb" -mindepth 1 -delete
    commit=$(git -C "$repo" rev-parse HEAD)
    git -C "$repo" status --porcelain > "$work/dirty-paths-$stamp.txt"
    if [[ -s $work/dirty-paths-$stamp.txt ]]; then dirty=dirty; else dirty=clean; fi
    : > "$work/arms-$stamp.txt"
    for arm in "${arms[@]}"; do
        prepare_arm "$repo" "$work/arms/$arm" "$arm"
        echo "$arm $(git -C "$repo" rev-parse "$arm") $(cat "$work/arms/$arm/bench-source.txt")" >> "$work/arms-$stamp.txt"
    done
    podman --version > "$work/podman-version-$stamp.txt"
    podman image inspect --format "$image {{.Id}} created {{.Created}}" "$image" > "$work/image-$stamp.txt"
    podman run --rm -v "$repo:/src:ro" -v "$work:/work" -v "$work/lmdb:/lmdb" -v "$ccache:/ccache" \
        -e CCACHE_DIR=/ccache "$image" bash /src/bench/run.sh --inside "$samples" "$rounds" "$commit" "$dirty" "$stamp" \
        "$prefixes" "${arms[@]}"
    cp "$work/result-$stamp.txt" "$results/$stamp.txt"
    echo "$results/$stamp.txt"
}

prepare_arm() {
    local repo=$1 directory=$2 arm=$3 patch
    rm -rf "$directory"
    mkdir -p "$directory/src"
    git -C "$repo" archive "$arm" | tar -x -C "$directory/src"
    tar -C "$repo" --exclude=.git -c deps | tar -x -C "$directory/src"
    patch=$repo/bench/arms/$arm.patch
    if [[ -f $patch ]]; then
        (cd "$directory/src" && patch -p1 --quiet < "$patch")
        echo "its own bench/konsens_bench.cpp with bench/arms/$arm.patch" > "$directory/bench-source.txt"
    else
        cp "$repo/bench/konsens_bench.cpp" "$directory/src/bench/konsens_bench.cpp"
        echo "bench/konsens_bench.cpp of the working tree" > "$directory/bench-source.txt"
    fi
}

configure() {
    local source=$1 build=$2 log=$3
    cmake -S "$source" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_FLAGS_RELEASE=-O2 -DNDEBUG" \
        "-DCMAKE_C_FLAGS_RELEASE=-O2 -DNDEBUG" \
        -DCMAKE_CXX_COMPILER=$compiler -DCMAKE_C_COMPILER=$c_compiler -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DKONSENS_BENCH=ON -Wno-dev \
        > "$log" 2>&1 || { tail -n 40 "$log" >&2; exit 1; }
}

build() {
    local stamp=$1 arm
    shift
    configure /src /work/build "/work/cmake-$stamp.log"
    cmake --build /work/build --target $main_binary $aa_binary -j $build_jobs \
        > "/work/build-$stamp.log" 2>&1 || { tail -n 40 "/work/build-$stamp.log" >&2; exit 1; }
    for arm in "$@"; do
        configure "/work/arms/$arm/src" "/work/arms/$arm/build" "/work/cmake-$arm-$stamp.log"
        cmake --build "/work/arms/$arm/build" --target $main_binary -j $build_jobs \
            > "/work/build-$arm-$stamp.log" 2>&1 || { tail -n 40 "/work/build-$arm-$stamp.log" >&2; exit 1; }
        cp "/work/arms/$arm/build/$main_binary" "/work/build/${main_binary}_$arm"
    done
}

context() {
    local commit=$1 dirty=$2 stamp=$3 b
    echo "# commit $commit $dirty"
    sed 's/^/# dirty path /' "/work/dirty-paths-$stamp.txt"
    sed 's/^/# arm /' "/work/arms-$stamp.txt"
    echo "# podman $(cat "/work/podman-version-$stamp.txt")"
    echo "# container podman, image $(cat "/work/image-$stamp.txt")"
    echo "# os $(. /etc/os-release && echo "$PRETTY_NAME")"
    echo "# compiler $($compiler --version | head -n 1)"
    echo "# build type Release, CMAKE_CXX_FLAGS_RELEASE $(grep -E '^CMAKE_CXX_FLAGS_RELEASE:' /work/build/CMakeCache.txt | cut -d= -f2-)"
    echo "# build type Release, CMAKE_C_FLAGS_RELEASE $(grep -E '^CMAKE_C_FLAGS_RELEASE:' /work/build/CMakeCache.txt | cut -d= -f2-)"
    echo "# compile command $(ninja -C /work/build -t commands $main_binary | grep -F -- '-c /src/bench/konsens_bench.cpp')"
    echo "# compile command $(ninja -C /work/build -t commands $main_binary | grep -F -- '-c /src/deps/lmdb/libraries/liblmdb/mdb.c')"
    echo "# instruction set $($compiler -O2 -Q --help=target | awk '$1 == "-march=" || $1 == "-mtune=" {printf "%s%s ", $1, $2}')"
    echo "# instruction set macros $(echo | $compiler -O2 -dM -E -x c++ - | grep -oE '__(MMX|SSE[0-9_]*|AVX[0-9A-Z_]*|POPCNT|BMI2?|FMA|CRC32)__' | sort -u | tr '\n' ' ')"
    for b in "${binaries[@]}"; do
        echo "# binary $b sha256 $(sha256sum "/work/build/$b" | cut -c1-16) needs $(objdump -T "/work/build/$b" | grep -oE '(GLIBCXX|CXXABI|GLIBC)_[0-9.]+' | sort -uV | tr '\n' ' ')"
        echo "# binary $b comment $(readelf -p .comment "/work/build/$b" | grep -oE 'GCC: .*' | sort -u | tr '\n' ' ')"
    done
    echo "# libraries $(ldd /work/build/$main_binary | awk '{printf "%s %s; ", $1, $3}')"
    echo "# packages $(dpkg-query -W -f='${Package} ${Version}; ' libstdc++6 libc6 || true)"
    echo "# lmdb $(grep -E '^#define MDB_VERSION_(MAJOR|MINOR|PATCH)' /src/deps/lmdb/libraries/liblmdb/lmdb.h | awk '{printf "%s ", $3}')from the submodule deps/lmdb"
    echo "# kernel $(uname -r) $(uname -v)"
    if [[ -r /sys/kernel/sched_ext/state ]]; then
        echo "# scheduler sched_ext $(cat /sys/kernel/sched_ext/state)"
    else
        echo "# scheduler no /sys/kernel/sched_ext"
    fi
    echo "# virtual machine hypervisor flag $(grep -m 1 -cw hypervisor /proc/cpuinfo || true)"
    echo "# processor $(grep -m 1 '^model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ *//') nproc $(nproc)"
    echo "# cpu flags sha256 $(grep -m 1 '^flags' /proc/cpuinfo | sha256sum | cut -c1-16)"
    echo "# user $bench_user uid $bench_uid through setpriv, nice $(nice), threads 1 in a serial case; 4 in a case whose name ends in _threads (the main thread and three node threads); 5 in a pipeline or execute_verify case (the main thread, three node threads and a thread that ticks)"
    echo "# lmdb mount $(awk '$2 == "/lmdb"' /proc/mounts)"
    echo "# loadavg at start $(cat /proc/loadavg)"
}

run_one() {
    local round=$1 name=$2 argument=$3 binary=$4 samples=$5
    local directory=/lmdb/$round-$name-$argument-$binary other running steal_before steal_after line status=0
    local -a arguments=("$name" "$samples" "$argument")
    if [[ $name == *lmdb* || $name == pipeline* || $name == execute_verify* || $name == read_back || $name == scan* ]]; then
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

select_cases() {
    local prefixes=$1 item prefix
    local -a wanted
    mapfile -t cases < <(/work/build/$main_binary --list)
    [[ $prefixes != all ]] || return 0
    local -a chosen=()
    IFS=, read -r -a wanted <<< "$prefixes"
    for item in "${cases[@]}"; do
        for prefix in "${wanted[@]}"; do
            if [[ $item == "$prefix"* ]]; then
                chosen+=("$item")
                break
            fi
        done
    done
    ((${#chosen[@]} > 0)) || { echo "no case starts with one of: $prefixes" >&2; exit 1; }
    cases=("${chosen[@]}")
}

binary_lists() {
    local item binary
    for binary in "${binaries[@]}"; do
        /work/build/$binary --list > "/work/list-$binary.txt"
    done
}

rounds() {
    local samples=$1 rounds=$2 round item name argument binary
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
            read -r name argument <<< "$item"
            for binary in "${binary_order[@]}"; do
                if grep -qxF "$item" "/work/list-$binary.txt"; then
                    run_one "$round" "$name" "$argument" "$binary" "$samples"
                fi
            done
        done
    done
}

summary() {
    awk -v main="$main_binary" -v aa="$aa_binary" -v others="$*" '
    BEGIN {
        other_count = split(others, other, " ")
        detail_count = split("latency_p50_ns latency_p99_ns processor_ns_per_op runs_per_request runs_again_per_1000 commits_per_op write_atomically_per_op messages_per_op", detail, " ")
    }
    function middle(table, key, binary,    n, i, j, t, a) {
        n = table[key, binary, 0] + 0
        if (n == 0) { low = 0; high = 0; return "" }
        for (i = 1; i <= n; i++) a[i] = table[key, binary, i] + 0
        for (i = 2; i <= n; i++) { t = a[i]; for (j = i - 1; j >= 1 && a[j] > t; j--) a[j + 1] = a[j]; a[j + 1] = t }
        low = a[1]; high = a[n]
        if (n % 2 == 1) return a[(n + 1) / 2]
        return (a[n / 2] + a[n / 2 + 1]) / 2
    }
    function add(table, key, binary, v) { table[key, binary, ++table[key, binary, 0]] = v }
    function percent(x, y) { return y > 0 ? sprintf("%+.1f %%", (x - y) / y * 100) : "-" }
    {
        split("", field)
        for (i = 1; i <= NF; i++) { p = index($i, "="); if (p > 0) field[substr($i, 1, p - 1)] = substr($i, p + 1) }
        if (field["exit"] != 0 || !("ns_per_op_median" in field)) { failed++; failures[failed] = $0; next }
        argument = ("payload_bytes" in field) ? field["payload_bytes"] : field["parameter"]
        key = field["case"] " " argument
        if (!(key in seen)) { seen[key] = 1; keys[++key_count] = key }
        add(ns, key, field["binary"], field["ns_per_op_median"])
        for (d = 1; d <= detail_count; d++) if (detail[d] in field) { add(dv, key SUBSEP detail[d], field["binary"], field[detail[d]]); has[key, detail[d]] = 1 }
    }
    END {
        print "## A/A difference of each case"
        print ""
        print "ns per operation: the median of the process medians of each binary."
        print ""
        print "| case | argument | " main " | " aa " | A/A |"
        print "|---|---|---|---|---|"
        largest = 0
        for (k = 1; k <= key_count; k++) {
            split(keys[k], part, " ")
            m = middle(ns, keys[k], main); a = middle(ns, keys[k], aa)
            if (m == "" || a == "") continue
            d = m > 0 ? (a - m) / m * 100 : 0
            if ((d < 0 ? -d : d) > largest) largest = d < 0 ? -d : d
            printf "| %s | %s | %.2f | %.2f | %+.1f %% |\n", part[1], part[2], m, a, d
        }
        print ""
        printf "Largest A/A difference: %.1f %%. This is the noise floor of the run.\n", largest
        print ""
        print "## Result of each binary"
        print ""
        print "ns per operation: the median of the process medians. Spread: the lowest and the highest process median."
        print "Each other arm: the branch binary " main " against that arm, (branch - arm) / arm."
        print ""
        header = "| case | argument | " main " | spread"
        line = "|---|---|---|---"
        for (o = 1; o <= other_count; o++) { header = header " | " main "_" other[o] " | spread | branch vs arm"; line = line "|---|---|---" }
        print header " |"; print line "|"
        for (k = 1; k <= key_count; k++) {
            split(keys[k], part, " ")
            m = middle(ns, keys[k], main); ml = low; mh = high
            row = sprintf("| %s | %s | %s | %s", part[1], part[2], m == "" ? "-" : sprintf("%.2f", m), m == "" ? "-" : sprintf("%.2f-%.2f", ml, mh))
            for (o = 1; o <= other_count; o++) {
                b = main "_" other[o]
                x = middle(ns, keys[k], b)
                row = row sprintf(" | %s | %s | %s", x == "" ? "-" : sprintf("%.2f", x), x == "" ? "-" : sprintf("%.2f-%.2f", low, high), (x == "" || m == "") ? "-" : percent(m, x))
            }
            print row " |"
        }
        print ""
        print "## Details of each binary"
        print ""
        print "The median over the processes of each field of the raw lines."
        print ""
        print "| case | argument | binary | field | median | spread |"
        print "|---|---|---|---|---|---|"
        nb = 2 + other_count; bl[1] = main; bl[2] = aa
        for (o = 1; o <= other_count; o++) bl[2 + o] = main "_" other[o]
        for (k = 1; k <= key_count; k++) {
            split(keys[k], part, " ")
            for (d = 1; d <= detail_count; d++) {
                if (!((keys[k], detail[d]) in has)) continue
                for (b = 1; b <= nb; b++) {
                    v = middle(dv, keys[k] SUBSEP detail[d], bl[b])
                    if (v == "") continue
                    printf "| %s | %s | %s | %s | %.3f | %.3f-%.3f |\n", part[1], part[2], bl[b], detail[d], v, low, high
                }
            }
        }
        print ""
        printf "Processes that failed: %d.\n", failed
        for (f = 1; f <= failed; f++) print "failed: " failures[f]
    }'
}

header() {
    local samples=$1 rounds=$2
    cat <<EOF
# Bench: konsens_bench

What: konsens_bench, built from bench/konsens_bench.cpp at the commit in
the context below, and one binary for each arm commit. Each process runs
one case with one argument (a payload in bytes or a parameter). It takes
one warm-up sample and then $samples samples. A sample is a fixed number of
operations (ops_per_sample in each raw line). The time of one operation is
the time of the sample divided by that number. Each process prints the
median, the minimum and the maximum of its samples.

Cases of 06a52bf, with the same names, inputs and operations:
- crc32c: crc32c::Crc32c over one payload, taken in turn from 16 buffers.
- encode_accept_decide, decode_accept_decide: one AcceptDecide with 64
  entries of 1024 B. encode_decide, decode_decide: one Decide.
- commit_lmdb: one write transaction of the log with one append of one
  entry and its commit, on a fresh store for each sample (06a52bf: one
  write_atomically of one append_entries). This is the commit of a step.
- write_lmdb: three nodes in one process, messages through encode and
  decode, the network a queue. One client_request at a follower, timed
  from the call to its reply.
- read_lmdb: one client_query at a follower after one write, timed from
  the call to its reply. This is a read after a step.
- write_batch_lmdb: 64 client_request calls at a follower before the
  first round, timed until the last reply, divided by 64.
- write_batch_size_lmdb (new; the arm 06a52bf has it through
  bench/arms/06a52bf.patch): as write_batch_lmdb with a payload of 1024 B,
  at the batch size of the argument (1, 16 or 256).
- lmdb_put, lmdb_put_like_storage, lmdb_get: LMDB alone, env flags 0, as
  at 06a52bf. get_entries_lmdb: get_entries(0, 1) of one entry in a read
  transaction.
- write_lmdb_threads, read_lmdb_threads, write_batch_lmdb_threads: the
  same operations with each node on its own thread, no tick.
Removed at the branch: handle_accept_decide and handle_decide. They ran
on a store that keeps no entry, and each step of the branch reads its
entries back.

New cases:
- pipeline_<work>_<delay>_<rate>: one leader and two followers, each node
  on its own thread, each message delayed by the delay (0 or 20 ms). A
  thread puts a tick call into each inbox every tick_ms (1 ms at 0 ms
  delay, 20 ms at 20 ms delay). Requests arrive at the leader in an open
  loop at the rate (10000/s, the rate of
  bench/results/2026-10-24-231534Z, or 20000/s), with an empty payload.
  The argument is max_in_flight. Work put: one get and one put of a
  counter. Work loop: the same and 63 more puts. ns per op: from the
  planned arrival of the first request of a sample to the last reply of
  that sample, divided by the requests of the sample. Latency: from the
  planned arrival to the release of the reply, of each request after the
  warm-up. processor_ns_per_op: getrusage of the process (all threads)
  over the requests after the warm-up. runs_per_request: the calls of
  apply at all nodes over all requests, warm-up included;
  runs_again_per_1000: the calls above one per node and request, per
  1000 requests.
- execute_verify_int, execute_verify_float: the pipeline harness at 0 ms
  delay and 10000 requests/s, argument max_in_flight. int: the counter.
  float: each node returns (n + 1/3) * (1 + e) as a CBOR double, with e
  0, 1e-14 and -1e-14 at nodes 1, 2 and 3, and float_digits 12 where the
  commit has Execute-Verify.
- read_back: a copy of the reads of node::written_values_read_back, done
  in one open write transaction of a state_storage after one put of a
  value of the payload size. Before the samples, the case compares the
  copy with the state of each node after a step of the cluster.
- scan_the_whole_log: sequence_paxos::scan_the_whole_log over a log of
  the argument in entries of 64 B.

Checks: as at 06a52bf for its cases. The cluster cases compare each
reply with its request number. A process whose check fails exits with 1;
its raw line shows the exit status.

Run: $rounds rounds. Each round runs every case in every binary that
lists it (konsens_bench --list). The order of the cases and of the
binaries turns around each second round. konsens_bench_aa is the source
of the branch built a second time; the difference between
konsens_bench and konsens_bench_aa is the A/A difference.

User and priority: each process runs as the user bench (uid 1001, not
root), started by setpriv. Not pinned. Each raw line gives nice,
other_processes_of_user (the processes of the user bench before the
start), procs_running of /proc/stat before the start, and the steal ticks
of /proc/stat during the process.

EOF
}

inside() {
    local samples=$1 rounds=$2 commit=$3 dirty=$4 stamp=$5 prefixes=${6:-all} arm
    shift 6 || true
    local -a arms=("$@")
    binaries=($main_binary $aa_binary)
    for arm in "${arms[@]}"; do
        binaries+=("${main_binary}_$arm")
    done
    build "$stamp" "${arms[@]}"
    select_cases "$prefixes"
    binary_lists
    getent passwd "$bench_user" > /dev/null || useradd -u "$bench_uid" -M "$bench_user"
    chown "$bench_uid:$bench_uid" /lmdb
    context "$commit" "$dirty" "$stamp" > "/work/context-$stamp.txt"
    echo "# cases chosen by the prefixes '$prefixes': ${#cases[@]}" >> "/work/context-$stamp.txt"
    rounds "$samples" "$rounds" > "/work/raw-$stamp.txt"
    echo "# loadavg at end $(cat /proc/loadavg)" >> "/work/context-$stamp.txt"
    {
        header "$samples" "$rounds"
        echo "## Context"
        echo ""
        cat "/work/context-$stamp.txt"
        echo ""
        summary "${arms[@]}" < "/work/raw-$stamp.txt"
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
