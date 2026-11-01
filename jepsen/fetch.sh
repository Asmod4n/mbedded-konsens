#!/bin/bash
set -eu

here=$(cd "$(dirname "$0")" && pwd)
build=$here/build
lein_version=2.11.2
lein_sha256=f83e3de252cb09d301f18f1912acffa807b54c1c9a96772ae63847a946032bd4

fetch() {
    local name=$1 url=$2 commit=$3
    if [ "$(git -C "$build/$name" rev-parse HEAD 2>/dev/null)" = "$commit" ]; then
        return
    fi
    rm -rf "${build:?}/$name"
    git init -q "$build/$name"
    git -C "$build/$name" fetch -q --depth 1 "$url" "$commit"
    git -C "$build/$name" checkout -q FETCH_HEAD
}

mkdir -p "$build"
fetch lazyfs https://github.com/dsrhaslab/lazyfs.git 9c5685e828da8b5d113d2f9134f6671e481d7162
fetch toml11 https://github.com/ToruNiina/toml11.git dcfe39a783a94e8d52c885e5883a6fbb21529019
fetch spdlog https://github.com/gabime/spdlog.git 76fb40d95455f249bd70824ecfcae7a8f0930fa3

if ! echo "$lein_sha256  $build/lein" | sha256sum -c --status 2>/dev/null; then
    curl -sSfLo "$build/lein" "https://raw.githubusercontent.com/technomancy/leiningen/$lein_version/bin/lein"
    echo "$lein_sha256  $build/lein" | sha256sum -c --quiet
    chmod 755 "$build/lein"
fi

if [ ! -f "$build/control" ]; then
    podman run --rm -v "$build:/k:Z" registry.opensuse.org/opensuse/tumbleweed:latest sh -c \
        'zypper -n -q in --no-recommends openssh-clients > /dev/null && ssh-keygen -q -t rsa -b 3072 -m PEM -N "" -C jepsen-control -f /k/control'
fi
