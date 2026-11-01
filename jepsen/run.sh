#!/bin/bash
set -eu

here=$(cd "$(dirname "$0")" && pwd)
build=$here/build
nodes=(n1 n2 n3 n4 n5)

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
        podman rm -f -t 2 "jepsen-$n" > /dev/null 2>&1 || true
    done
}

remove_the_nodes
trap remove_the_nodes EXIT
for n in "${nodes[@]}"; do
    podman run -d --init --name "jepsen-$n" --hostname "$n" --network jepsen --network-alias "$n" \
        --cap-add NET_ADMIN,SYS_ADMIN --device /dev/fuse localhost/jepsen-node > /dev/null
done

mkdir -p "$build/m2" "$build/lein-home" "$here/results"
status=0
podman run --rm --network jepsen \
    -v "$here:/jepsen:Z" -v "$here/results:/jepsen/store:Z" \
    -v "$build/m2:/root/.m2:Z" -v "$build/lein-home:/root/.lein:Z" \
    -w /jepsen localhost/jepsen-control \
    lein run test --nodes "$(IFS=,; echo "${nodes[*]}")" --username root --ssh-private-key /root/.ssh/id_rsa "$@" ||
    status=$?

pack_the_runs() {
    local run
    for run in "$here"/results/*/20*/; do
        [ -d "$run" ] || continue
        run=${run%/}
        tar -C "$(dirname "$run")" -cJf "$run.tar.xz" "$(basename "$run")"
        rm -rf "$run"
    done
}

pack_the_runs
exit "$status"
