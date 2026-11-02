#!/bin/bash
set -eu

here=$(cd "$(dirname "$0")" && pwd)
build=$here/build
nodes=(n1 n2 n3 n4 n5)
engine=${JEPSEN_ENGINE:-kernel}

"$here/fetch.sh"
if [ ! -x "$build/konsensd" ]; then
    podman build -q -f "$here/../containers/konsensd/Containerfile" --target build -t localhost/konsensd-build "$here/.."
    id=$(podman create localhost/konsensd-build)
    podman cp "$id:/build/konsensd" "$build/konsensd"
    podman rm "$id" > /dev/null
fi
podman build -q -f "$here/containers/node/Containerfile" -t localhost/jepsen-node "$build"
podman build -q -f "$here/containers/control/Containerfile" -t localhost/jepsen-control "$build"
podman network exists jepsen || podman network create jepsen

remove_the_nodes() {
    local n
    for n in "${nodes[@]}"; do
        podman rm -f --ignore -t 2 "jepsen-$n" > /dev/null
    done
}

case "$engine" in
    kernel)
        python3 -I - /usr/share/containers/seccomp.json "$build/seccomp-io-uring.json" <<'PY'
import json, sys
profile = json.load(open(sys.argv[1]))
profile["syscalls"].append({"names": ["io_uring_setup", "io_uring_enter", "io_uring_register"],
                            "action": "SCMP_ACT_ALLOW"})
json.dump(profile, open(sys.argv[2], "w"))
PY
        security=(--security-opt "seccomp=$build/seccomp-io-uring.json") ;;
    emulation)
        security=() ;;
    *)
        echo "run.sh: JEPSEN_ENGINE is kernel or emulation, not $engine" >&2
        exit 2 ;;
esac

remove_the_nodes
trap remove_the_nodes EXIT
for n in "${nodes[@]}"; do
    podman run -d --init --name "jepsen-$n" --hostname "$n" --network jepsen --network-alias "$n" \
        --cap-add NET_ADMIN,SYS_ADMIN,SYS_PTRACE "${security[@]}" --device /dev/fuse localhost/jepsen-node > /dev/null
done

mkdir -p "$build/m2" "$build/lein-home" "$here/store" "$here/results"
status=0
podman run --rm --network jepsen \
    -v "$here:/jepsen:Z" -v "$here/store:/jepsen/store:Z" \
    -v "$build/m2:/root/.m2/repository:Z" -v "$build/lein-home:/root/.lein:Z" \
    -w /jepsen localhost/jepsen-control \
    lein run test --nodes "$(IFS=,; echo "${nodes[*]}")" --username root --ssh-private-key /root/.ssh/id_rsa --engine "$engine" "$@" ||
    status=$?

keep_the_summaries() {
    local run summary
    for run in "$here"/store/*/20*/; do
        [ -d "$run" ] || continue
        run=${run%/}
        summary="$here/results/$(basename "$(dirname "$run")")/$(basename "$run")"
        mkdir -p "$summary"
        [ ! -f "$run/results.edn" ] || xz -9 -c "$run/results.edn" > "$summary/results.edn.xz"
        for log in "$run"/n*/konsensd.log "$run"/n*/konsensd-threads.log; do
            [ -f "$log" ] || continue
            cp "$log" "$summary/$(basename "$(dirname "$log")")-$(basename "$log")"
        done
        tar -C "$(dirname "$run")" -cJf "$run.tar.xz" "$(basename "$run")"
        rm -rf "$run"
    done
}
keep_the_summaries
exit "$status"
