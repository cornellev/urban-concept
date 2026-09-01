# elec

RP2040 firmware and stuff

pic-sdk is vendored here

each subdirectory of `src/` should include a CMakeLists.txt and is an independent project (ie front aux, back aux)

## Setup

### Windows

you need to install the picotools, arm gnu toolchain, cmake, ninja, and just. The setup/setup-windows.ps1 file handles this. Run it with:
`./setup/setup-windows.ps1`

### macOS

todo

### nix

just use the provided flake. it installs everything for you.
if you have direnv, `direnv allow` too
