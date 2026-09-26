# elec

This folder holds the firmware for the car's RP2040 boards.

Each board has its own program in its own folder under `src/` (for example `template-project`).
`src/common/` holds code shared by all boards.

The Pico SDK is vendored at the repo root in `vendor/pico-sdk/` as a git submodule.

## Setup

### Windows

- Make sure you have `winget` installed already. if not, install it
- cd to the `elec/` folder:
  ```powershell
  cd C:\path\to\urban-concept\elec
  ```
- Run the setup script:
  ```powershell
  powershell -ExecutionPolicy Bypass -File setup/setup-windows.ps1
  ```
  It installs anything missing (CMake, Ninja, Just, the GitHub CLI, and Python) with `winget`, and downloads the arm compiler and picotool. Windows may pop up permission windows during installs. click yes for all of them.
- Close and open a new terminal, because PATH may not be updated yet.

### macOS

- Install [Homebrew](https://brew.sh) if you don't have it
- cd to the `elec/` folder:
  ```sh
  cd /path/to/urban-concept/elec
  ```
- Run the setup script from the `elec/` folder:
  ```sh
  ./setup/setup-macos.sh
  ```
  It installs CMake, Ninja, Just, the GitHub CLI, picotool, and the arm compiler.
- Close and open a new terminal, because PATH may not be updated yet.

### Linux

- Install these with your package manager:
  - CMake 3.21 or newer
  - Ninja
  - Just 1.52.0 or newer
  - Python 3
  - the arm compiler (`arm-none-eabi-gcc`) 15.3, from [Arm](https://gitlab.arm.com/tooling/gnu-toolchains-for-arm)
  - picotool 2.3
- Run `just doctor` from the `elec/` folder. It checks the arm compiler and picotool versions, and
  that CMake, Ninja, and Python are installed

### Nix

Use the provided flake. `nix develop` (or `direnv allow`) sets everything up.

## Building

From the `elec/` folder:

```sh
just build                        # build every project
just build-target template-project # build just one project
just clean                        # delete the build files and start fresh
just doctor                       # check if tools are installed and working
```

(From the repo root instead, add the `elec` prefix, ie `just elec build`.)

The project name is the folder name under `src/`. After a build, the file to flash onto the board is located at:

```
elec/build/src/<project>/<project>.uf2
```

If a build fails right after installing or switching tools (for example it uses
the wrong compiler), run `just clean` and build again. The old `build/` folder
caches the previous compiler, so a stale cache can break the next build.

## Editor (VS Code)

We use the clangd extension instead of Microsoft's C/C++ IntelliSense because it integrates better with other platform's tools.
For setup:

- Build once first, because clangd reads `build/compile_commands.json`
- Open the repo root (`urban-concept/`) in VS Code, not `elec/`
- Install the recommended clangd extension when VS Code asks
- If VS Code asks whether you trust the folder, click Trust
- If it says clangd was not found on your PATH, click Install

For other editors: pass `--query-driver=**/arm-none-eabi-*` to clangd.

## Flashing (putting firmware on a board)

First, put the board in BOOTSEL mode:

- unplug it
- hold the BOOTSEL button
- plug it back in while still holding the button down

### manually flashing

In BOOTSEL mode the board shows up as a USB drive called **`RPI-RP2`**. Just drag the project's `.uf2` file (see the path above) onto that drive. The board flashes itself and restarts

### via a command

```sh
just flash template-project
```

This builds the project and loads it onto the board. On macOS and nix this
works out of the box. On Windows it needs a USB driver for picotool. If it says
"no accessible RP2 devices," use the manual method.

## Adding a new board

Each subdirectory of `src/` is an independent project with its own
`CMakeLists.txt`. To add one

- copy `src/template-project` to a new folder. The folder name becomes the target name
- write the board in `main.cpp`
- in the board's `CMakeLists.txt`, list extra libraries in `add_board(...)` and any source files
  besides `main.cpp` in `target_sources(...)`
- register the folder in the firmware inventory in `elec/CMakeLists.txt`
- compile with `just build-target {project-name}`
- flash with `just flash {project-name}`

For example, a board in `src/steering/` with `main.cpp`, `pid.cpp`, and `encoder.cpp` has this
`src/steering/CMakeLists.txt`:

```cmake
add_board(hardware_pwm)
target_sources(steering PRIVATE pid.cpp encoder.cpp)
```
