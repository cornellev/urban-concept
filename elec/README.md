# elec

RP2040 firmware and stuff

pic-sdk is vendored here

each subdirectory of `src/` should include a CMakeLists.txt and is an independent project (ie front aux, back aux)

## Setup

### Windows

Run the setup script once, in a fresh terminal:

```powershell
powershell -ExecutionPolicy Bypass -File setup/setup-windows.ps1
```

It installs any of these tools that are missing (via `winget`) and downloads a prebuilt picotool:

- the arm GNU toolchain (`arm-none-eabi-gcc`)
- CMake
- Ninja
- Just
- the GitHub CLI

Then build from the repo root with `just` (open a new terminal first so freshly installed tools are on PATH):

```powershell
just elec build-all              # build every project
just elec build template-project # build one
```

### macOS

todo

### nix

Just use the provided flake.
