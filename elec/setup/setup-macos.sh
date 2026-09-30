#!/usr/bin/env bash
# set up the macos build tools for rp2040 firmware
# the first build downloads the pinned arm compiler and picotool into elec/.tools
# after this, build from elec/ with: just build
set -euo pipefail

if ! command -v brew >/dev/null; then
    echo "install Homebrew first: https://brew.sh"
    exit 1
fi

# arm publishes the pinned compiler for apple silicon only, and a rosetta terminal reports x86_64
if [ "$(uname -m)" != arm64 ]; then
    echo "this terminal runs as $(uname -m), so it is an Intel Mac or a Rosetta terminal."
    echo "Intel Macs aren't supported. On Apple Silicon, open a terminal that isn't set to use Rosetta"
    exit 1
fi

brew install cmake ninja just

# fetch the pico-sdk and the one nested submodule the build needs
repo="$(cd "$(dirname "$0")/../.." && pwd)"
git -C "$repo" submodule update --init
git -C "$repo/vendor/pico-sdk" submodule update --init lib/tinyusb

echo "setup complete. build from elec/ with: just build"
