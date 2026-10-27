#!/bin/sh
set -u
config=${KONSENSD_CONFIG:-$HOME/.config/konsensd/konsensd.toml}
podman machine start
podman rm --ignore konsensd
podman run --rm --name konsensd -p 7151:7151 -p 7161:7161 \
    -v konsensd-data:/var/lib/konsensd \
    -v "$config":/etc/konsensd/konsensd.toml:ro \
    localhost/konsensd:latest &
container=$!
trap 'podman stop --time 300 konsensd; wait "$container"; exit 0' TERM INT
wait "$container"
