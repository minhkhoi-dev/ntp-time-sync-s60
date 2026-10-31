#!/usr/bin/env bash
# Builds ntpsyncd.sisx in the SDK Docker image from a checkout the toolchain cannot
# read in place, e.g. a Windows drive seen from WSL 2 (/mnt/c, /mnt/d): the 32-bit GCCE
# tools fail there with "Value too large for defined data type". The sources are copied
# into the container, built, and the package and signing keys copied back (only on
# success, so the same key signs later builds).
# Requires: docker (inside WSL). From Windows:
#   wsl -- bash /mnt/c/path/to/ntp-time-sync-s60/group/docker-build.sh
set -euo pipefail
IMAGE="${NTPSYNCD_SDK_IMAGE:-minhkhoi4dev/s60v3-sdk-docker:1.1.0}"

repo="$(cd "$(dirname "$0")/.." && pwd)"

docker run --rm -v "$repo":/src "$IMAGE" bash -c '
    set -euo pipefail
    mkdir /build
    cp -r /src/. /build/
    bash /build/group/build.sh
    cd /build
    cp sis/ntpsyncd.sisx sis/dev.key sis/dev.cer /src/sis/
'
echo "Built $repo/sis/ntpsyncd.sisx"
