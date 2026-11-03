#!/bin/bash
set -eu

here=$(cd "$(dirname "$0")" && pwd)
build=$here/build
lein_version=2.11.2
lein_sha256=f83e3de252cb09d301f18f1912acffa807b54c1c9a96772ae63847a946032bd4

fetch() {
    local name=$1 url=$2 commit=$3
    if [ -d "$build/$name/.git" ] && [ "$(git -C "$build/$name" rev-parse HEAD)" = "$commit" ]; then
        return
    fi
    rm -rf "${build:?}/$name"
    git init -q "$build/$name"
    git -C "$build/$name" fetch -q --depth 1 "$url" "$commit"
    git -C "$build/$name" checkout -q FETCH_HEAD
}

attempts=3
wait_seconds=30

lein_deps() {
    (cd "$build/deps" && LEIN_HOME="$build/lein-home" "$build/lein" update-in : assoc :local-repo "\"$build/m2\"" -- deps)
}

retried() {
    local attempt
    for attempt in $(seq "$attempts"); do
        "$@" && return 0
        [ "$attempt" = "$attempts" ] && return 1
        echo "fetch.sh: $1 failed, attempt $attempt of $attempts; next attempt in $((wait_seconds * attempt)) s" >&2
        sleep "$((wait_seconds * attempt))"
    done
}

mkdir -p "$build"
fetch lazyfs https://github.com/dsrhaslab/lazyfs.git 9c5685e828da8b5d113d2f9134f6671e481d7162
fetch toml11 https://github.com/ToruNiina/toml11.git dcfe39a783a94e8d52c885e5883a6fbb21529019
fetch spdlog https://github.com/gabime/spdlog.git 76fb40d95455f249bd70824ecfcae7a8f0930fa3

if [ ! -f "$build/lein" ] || ! echo "$lein_sha256  $build/lein" | sha256sum -c --status; then
    curl -sSfLo "$build/lein" "https://raw.githubusercontent.com/technomancy/leiningen/$lein_version/bin/lein"
    echo "$lein_sha256  $build/lein" | sha256sum -c --quiet
    chmod 755 "$build/lein"
fi

if [ ! -f "$build/control" ]; then
    podman run --rm -v "$build:/k:Z" registry.opensuse.org/opensuse/tumbleweed:latest sh -c \
        'zypper -n -q in --no-recommends openssh-clients > /dev/null && ssh-keygen -q -t rsa -b 3072 -m PEM -N "" -C jepsen-control -f /k/control'
fi

mkdir -p "$build/deps" "$build/m2" "$build/lein-home"
cp "$here/project.clj" "$build/deps/project.clj"
retried lein_deps
