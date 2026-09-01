# elec

RP2040 firmware and stuff

pic-sdk is vendored here

each subdirectory of `src/` should include a CMakeLists.txt and is an independent project (ie front aux, back aux)

## Setup

### Windows

Run this in the terminal:

```sh
./setup/setup-windows.ps1
```

It handles the installation of:

- picotools
- the arm GNU toolchain
- CMake
- Ninja
- Just
- the GitHub CLI

### macOS

todo

### nix

Just use the provided flake.
