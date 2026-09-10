#!/usr/bin/env bash
# set up the macos toolchain for building rp2040 firmware
# after this, build from elec/ with: just build-all
set -euo pipefail

if ! command -v brew >/dev/null; then
    echo "install Homebrew first: https://brew.sh"
    exit 1
fi

brew install cmake ninja just gh picotool
brew install --cask gcc-arm-embedded

echo "setup complete. build from elec/ with: just build-all"
echo "if the build can't find arm-none-eabi-gcc, open a new terminal and retry."
